/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiModel.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>

#include "AdvaiConsent.h"

namespace Advai
{
namespace
{
constexpr Timestamp DAY_SECONDS = 86400;
constexpr Timestamp CONVERSATION_EXPIRY = 120;

std::uint32_t Mix(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352d;
    value ^= value >> 15;
    value *= 0x846ca68b;
    return value ^ (value >> 16);
}

bool Contains(std::vector<std::string> const& tokens, std::string const& token)
{
    return std::find(tokens.begin(), tokens.end(), token) != tokens.end();
}

bool Any(std::vector<std::string> const& tokens, std::initializer_list<char const*> words)
{
    return std::any_of(words.begin(), words.end(), [&](char const* word) { return Contains(tokens, word); });
}

void ExtractLinks(std::string const& input, std::string const& marker, std::vector<std::uint32_t>& output)
{
    std::size_t offset = 0;
    while ((offset = input.find(marker, offset)) != std::string::npos && output.size() < 8)
    {
        offset += marker.size();
        auto metadataEnd = input.find("|h", offset);
        auto displayEnd = metadataEnd == std::string::npos ? std::string::npos : input.find("|h", metadataEnd + 2);
        if (metadataEnd == std::string::npos || displayEnd == std::string::npos)
            continue;
        std::uint32_t id = 0;
        auto result = std::from_chars(input.data() + offset, input.data() + input.size(), id);
        if (result.ec == std::errc{} && id && result.ptr <= input.data() + metadataEnd &&
            (result.ptr == input.data() + metadataEnd || *result.ptr == ':'))
            output.push_back(id);
    }
}
}  // namespace

BotPersona BotPersona::Create(std::uint32_t seed)
{
    BotPersona result;
    result.Seed = seed;
    std::array<std::uint8_t*, 8> traits = {&result.Sociability,     &result.Patience,      &result.Generosity,
                                           &result.Competitiveness, &result.RiskTolerance, &result.Frugality,
                                           &result.Skill,           &result.SpeechStyle};
    for (auto* trait : traits)
    {
        seed = Mix(seed + 1);
        *trait = static_cast<std::uint8_t>(seed % 101);
    }
    constexpr std::array preferences = {Goal::Quest,   Goal::Gather,       Goal::Craft,     Goal::Trade,
                                        Goal::Dungeon, Goal::Battleground, Goal::Socialize, Goal::Quest,
                                        Goal::Quest,   Goal::Dungeon,      Goal::Raid,      Goal::Arena};
    result.Preference = preferences[Mix(seed) % preferences.size()];
    return result;
}

std::uint32_t BotPersona::ReactionMilliseconds(std::uint32_t context) const
{
    auto variation = context ? Mix(Seed ^ context) % (80 + (100 - Skill) * 3) : Seed % 151;
    return 250 + (100 - Skill) * 9 + variation;
}

float BotPersona::FormationOffset() const
{
    return (static_cast<int>(Mix(Seed ^ 0x504F5349U) % 201) - 100) * (0.001f + (100 - Skill) * 0.00002f);
}

std::uint32_t BotPersona::OptionalHesitationMilliseconds(std::uint32_t context) const
{
    if (Skill >= 60 || Mix(Seed ^ context ^ 0x534C4950U) % 100 >= (60U - Skill) / 3)
        return 0;
    return 600 + (60 - Skill) * 15 + Mix(Seed ^ context) % 400;
}

std::uint32_t BotPersona::BattlegroundRole(std::uint32_t instance) const
{
    auto choice = Mix(Seed ^ instance ^ 0x50565052U);
    if (RiskTolerance < 35)
        return choice % 4;
    if (Competitiveness > 70)
        return 3 + choice % 7;
    return choice % 10;
}

float BotPersona::SoloTargetCost(std::uint32_t target, float distance, int levelDifference, bool questTarget) const
{
    // Variation only ranks already visible, attackable, reachable candidates. It never broadens eligibility.
    auto preference = Mix(Seed ^ target) % 101;
    float cost = distance * (0.85f + preference * 0.003f);
    cost += std::max(0, levelDifference) * (100 - RiskTolerance) * 0.08f;
    return questTarget ? cost * 0.55f : cost;
}

BotPersona BotPersona::ForCharacter(BotId id)
{
    if (!id)
        throw std::invalid_argument("ADVAI character identity must have a GUID");
    constexpr std::uint32_t IDENTITY_NAMESPACE = 0x41445641U;
    return Create(Mix(id ^ IDENTITY_NAMESPACE));
}

bool BotActivitySnapshot::HasCommitment() const { return RequiresContinuousActivity() || InQueue; }

bool BotActivitySnapshot::RequiresContinuousActivity() const
{
    return !Alive || InCombat || InInstance || InTransit || ActionInProgress || Trading || HumanControlled || HumanGroup ||
           InBattleground || InArena;
}

void Relationship::Observe(BotSocialEvent const& event)
{
    if (Revision == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("ADVAI relationship revision exhausted");
    ++Revision;
    LastEvent = std::max(LastEvent, event.At);
    if (Encounters < std::numeric_limits<std::uint32_t>::max())
        ++Encounters;
    if (event.At < LastReward + 60)
        return;

    int magnitude = event.Kind == SocialEventKind::Conversation ? 1 : 5;
    Affinity =
        static_cast<std::int16_t>(std::clamp<int>(Affinity + (event.Positive ? magnitude : -magnitude), -100, 100));
    LastReward = event.At;
}

void Lifecycle::Start(Timestamp now, bool playerGuild, std::uint32_t entropy)
{
    Joined = now;
    WasPlayerGuild = playerGuild;
    // A character plays for three to eight weeks before it takes its long break and is replaced (was 2-4 days, or 7-14
    // days in a player guild).
    BreakDue = now + (21 + entropy % 36) * DAY_SECONDS;
    ReturnAt = 0;
    StartSession(now, entropy);
}

void Lifecycle::UpdateGuild(Timestamp now, bool playerGuild, std::uint32_t entropy,
                            std::optional<std::uint32_t> guild)
{
    if (guild)
    {
        if (GuildIdentityKnown && GuildIdentity != *guild)
            GuildSatisfaction = 0;
        GuildIdentity = *guild;
        GuildIdentityKnown = true;
    }
    if (playerGuild && !WasPlayerGuild)
        BreakDue = std::max(BreakDue, now + (21 + entropy % 36) * DAY_SECONDS);
    WasPlayerGuild = playerGuild;
}

bool Lifecycle::CanDepart(BotActivitySnapshot const& snapshot) const
{
    return Initialized && !ReturnAt && snapshot.Now >= DepartureDue() && !snapshot.HasCommitment() && snapshot.Alive;
}

// Characters stored before the tenure change keep a short BreakDue; enforce the three-to-eight-week minimum from the
// join time (a fixed per-character value, so it does not move between checks).
Timestamp Lifecycle::DepartureDue() const
{
    Timestamp const minimum = Joined ? Joined + (21 + static_cast<Timestamp>(Joined % 36)) * DAY_SECONDS : 0;
    return std::max(BreakDue, minimum);
}

void Lifecycle::BeginBreak(Timestamp now, std::uint32_t entropy) { ReturnAt = now + (14 + entropy % 17) * DAY_SECONDS; }

void Lifecycle::StartSession(Timestamp now, std::uint32_t entropy)
{
    SessionDue = now + 3600 + entropy % (3 * 3600);
    LoginAfter = 0;
}

bool Lifecycle::CanPause(BotActivitySnapshot const& snapshot) const
{
    return Initialized && !ReturnAt && !LoginAfter && SessionDue && snapshot.Now >= SessionDue &&
           !snapshot.HasCommitment() && snapshot.Alive;
}

void Lifecycle::BeginPause(Timestamp now, std::uint32_t entropy)
{
    LoginAfter = now + 15 * 60 + entropy % (105 * 60);
    SessionDue = 0;
}

bool Lifecycle::Eligible(Timestamp now) const
{
    return (!ReturnAt || now >= ReturnAt) && (!LoginAfter || now >= LoginAfter);
}
bool Lifecycle::ShouldLeaveGuild() const { return WasPlayerGuild && GuildSatisfaction <= -30; }

std::uint64_t Lifecycle::RecommendedReserveCharacters(std::uint32_t onlineTarget)
{
    // Default long breaks and short sessions imply about 12.1 characters per online slot; retain capacity margin.
    constexpr std::uint32_t DEFAULT_DUTY_CYCLE_RESERVE = 16;
    return static_cast<std::uint64_t>(onlineTarget) * DEFAULT_DUTY_CYCLE_RESERVE;
}

bool ConversationState::CanReply(Timestamp now, BotId partner, std::string const& normalized) const
{
    if (now < NextSpeak)
        return false;
    if (Partner != partner)
        return true;
    if (Partner == partner && now < LastMessage + CONVERSATION_EXPIRY && Messages >= MAX_CONVERSATION_MESSAGES)
        return false;
    return now >= LastMessage + CONVERSATION_EXPIRY ||
           std::find(RecentIncoming.begin(), RecentIncoming.end(), normalized) == RecentIncoming.end();
}

void ConversationState::Remember(Timestamp now, BotId partner, ParsedMessage const& message, std::uint32_t variant)
{
    if (Partner != partner || now >= LastMessage + CONVERSATION_EXPIRY)
    {
        Messages = 0;
        RecentIncoming.clear();
        QuestFocus = 0;
        ItemFocus = 0;
        SpellFocus = 0;
        ProfessionFocus = 0;
    }
    Partner = partner;
    LastMessage = now;
    ++Messages;
    if (!message.Intents.empty())
        Topic = message.Intents.front();
    RecentIncoming.push_back(message.Normalized);
    if (RecentIncoming.size() > MAX_CONVERSATION_MESSAGES)
        RecentIncoming.pop_front();
    RecentVariants.push_back(variant);
    if (RecentVariants.size() > MAX_RECENT_LINES)
        RecentVariants.pop_front();
}

ConversationState ConversationState::ForPartner(BotId partner, Timestamp now) const
{
    auto context = *this;
    if (Partner != partner || now >= LastMessage + CONVERSATION_EXPIRY)
    {
        context.Partner = partner;
        context.Topic = Intent::Unknown;
        context.LastMessage = 0;
        context.Messages = 0;
        context.QuestFocus = 0;
        context.ItemFocus = 0;
        context.SpellFocus = 0;
        context.ProfessionFocus = 0;
        context.RecentIncoming.clear();
    }
    return context;
}

Goal GoalPlanner::Choose(BotActivitySnapshot const& s, BotPersona const& p, BotLifeGoal const& current, Goal ambition)
{
    auto available = [&](Goal kind)
    {
        return s.Now >= s.RetryAfter[static_cast<std::size_t>(kind)] &&
               !(current.Kind == kind && current.State == Outcome::Blocked && s.Now < current.RetryAfter);
    };
    if (!s.Alive)
        return Goal::Recover;
    if (s.InCombat || s.Health < 20)
        return Goal::Survive;
    if (s.Trading)
        return Goal::Trade;
    if (s.InInstance || s.HumanGroup || s.HumanControlled || s.InTransit || s.ActionInProgress || s.InQueue ||
        s.InBattleground || s.InArena)
        return current.Kind;
    auto restHealth = 55 + (100 - p.RiskTolerance) / 5;
    auto restMana = 25 + (100 - p.Skill) / 5;
    if ((s.Health < restHealth || (s.UsesMana && s.Mana < restMana)) && available(Goal::Rest))
        return Goal::Rest;
    if (s.Durability < 25 && available(Goal::Repair))
        return Goal::Repair;
    if (s.FreeBagSlots < 2 && available(Goal::Sell))
        return Goal::Sell;
    if ((s.NeedsFood || s.NeedsDrink || s.NeedsAmmo || s.NeedsReagent || s.NeedsPetFood || s.NeedsTool || s.NeedsMount) &&
        available(Goal::Restock))
    {
        if (s.Money || s.HasBankSupplies)
            return Goal::Restock;
        if (s.HasVendorStock && available(Goal::Sell))
            return Goal::Sell;
        if (available(Goal::Quest))
            return Goal::Quest;
        if (s.CanGather && available(Goal::Gather))
            return Goal::Gather;
        return Goal::Rest;
    }
    if (current.Kind != Goal::None && s.Now < current.CommittedUntil && !s.AwaitingDelivery &&
        (current.State == Outcome::Proposed || current.State == Outcome::Started))
        return current.Kind;
    if ((s.HasMail || s.AwaitingDelivery) && available(Goal::Mail))
        return Goal::Mail;
    if (s.NeedsTraining && s.Money && available(Goal::Train))
        return Goal::Train;
    if (s.NeedsRecipe && available(Goal::Restock))
        return Goal::Restock;
    if (s.QuestComplete)
        return Goal::Quest;
    if (s.BankNeeded && available(Goal::Bank))
        return Goal::Bank;
    if (ambition != Goal::None && available(ambition))
        return ambition;
    if (p.Preference == Goal::Gather && s.CanGather && available(Goal::Gather))
        return Goal::Gather;
    if (p.Preference == Goal::Craft && s.CanCraft && available(Goal::Craft))
        return Goal::Craft;
    if (p.Preference == Goal::Trade && s.HasAuctionStock && available(Goal::Auction))
        return Goal::Auction;
    if (p.Preference == Goal::Trade && s.HasTradeStock && available(Goal::Trade))
        return Goal::Trade;
    if (p.Preference == Goal::Dungeon && s.Level >= 15 && available(Goal::Dungeon))
        return Goal::Dungeon;
    if (p.Preference == Goal::Raid && s.Level >= 80 && available(Goal::Raid))
        return Goal::Raid;
    if (p.Preference == Goal::Arena && s.Level >= 80 && available(Goal::Arena))
        return Goal::Arena;
    if (p.Preference == Goal::Battleground && s.Level >= 10 && available(Goal::Battleground))
        return Goal::Battleground;
    if (p.Preference == Goal::Socialize && p.Sociability > 65 && (s.Now / 300 + p.Seed) % 4 == 0)
        return Goal::Socialize;
    return available(Goal::Quest) ? Goal::Quest : Goal::Rest;
}

void GoalPlanner::ApplyResult(BotLifeGoal& goal, BotActionResult const& result, Timestamp now)
{
    goal.State = result.State;
    goal.Reason = result.Reason;
    if (result.State == Outcome::Blocked)
    {
        goal.Failures = std::min<std::uint8_t>(goal.Failures + 1, 8);
        goal.RetryAfter = now + std::min<Timestamp>(600, 5ULL << goal.Failures);
        goal.CommittedUntil = 0;
    }
    else if (result.State == Outcome::Succeeded || result.State == Outcome::Cancelled)
    {
        goal.Failures = 0;
        goal.CommittedUntil = 0;
    }
}

char const* GoalPlanner::Name(Goal goal)
{
    constexpr std::array names = {"none",    "survive", "recover",      "repair", "sell",      "restock", "train",
                                  "quest",   "gather",  "craft",        "trade",  "auction",   "bank",    "mail",
                                  "dungeon", "raid",    "battleground", "arena",  "socialize", "rest",    "travel"};
    return static_cast<std::size_t>(goal) < names.size() ? names[static_cast<std::size_t>(goal)] : "invalid";
}

std::vector<std::string> ChatRules::Requests(std::string const& input)
{
    if (input.empty() || input.size() > 1024 || Parse(input).IsCommand)
        return {input};
    std::vector<std::string> result;
    std::size_t begin = 0;
    std::string lowered = input;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c); });
    auto append = [&](std::size_t end)
    {
        auto first = input.find_first_not_of(" \t\r\n,;.!?", begin);
        if (first != std::string::npos && first < end)
        {
            auto last = input.find_last_not_of(" \t\r\n,;.!?", end - 1);
            auto request = input.substr(first, last - first + 1);
            bool question = end < input.size() && input[end] == '?';
            if (!question)
                question = input.find('?', last + 1) < end;
            if (question)
                request += '?';
            result.push_back(std::move(request));
        }
    };
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        if (input.compare(i, 2, "|H") == 0)
        {
            auto label = input.find("|h", i + 2);
            auto end = label == std::string::npos ? label : input.find("|h", label + 2);
            if (end == std::string::npos)
                return {input};
            i = end + 1;
            continue;
        }
        // Decimal quantities and punctuation inside links are part of the entity.
        bool boundary = input[i] == '?' || input[i] == ';' || input[i] == '!';
        if (input[i] == '.' && i + 1 < input.size() && std::isspace(static_cast<unsigned char>(input[i + 1])))
            boundary = true;
        std::size_t conjunction = 0;
        for (auto word : {" and ", " but "})
            if (lowered.compare(i, std::char_traits<char>::length(word), word) == 0)
            {
                auto start = i + std::char_traits<char>::length(word);
                auto next = input.substr(start, input.find(' ', start) - start);
                std::transform(next.begin(), next.end(), next.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (Any({next}, {"where", "what", "how", "can", "could", "would", "do", "are", "please", "actually"}))
                    conjunction = std::char_traits<char>::length(word);
            }
        if (boundary || conjunction)
        {
            append(i);
            begin = i + (conjunction ? conjunction : 1);
            if (conjunction)
                i = begin - 1;
        }
    }
    append(input.size());
    // Keep punctuation on a single utterance for question recognition and command isolation.
    if (result.size() < 2)
        return {input};
    if (result.size() > 3)
        return {input};
    for (auto& request : result)
    {
        auto parsed = Parse(request);
        if (parsed.IsCommand)
            return {input};
        if (parsed.Intents.empty() && !parsed.Focus)
            return {input};
        // The delimiter was a question mark in ordinary multi-question chat. Preserve its semantics.
        if (!parsed.Question &&
            (request.starts_with("where ") || request.starts_with("what ") || request.starts_with("how ") ||
             request.starts_with("can ") || request.starts_with("could ") || request.starts_with("do ")))
            request += '?';
    }
    return result;
}

ParsedMessage ChatRules::Parse(std::string const& input)
{
    ParsedMessage result;
    if (input.size() > 1024 || input.empty())
        return result;
    ExtractLinks(input, "|Hitem:", result.Items);
    ExtractLinks(input, "|Hquest:", result.Quests);
    ExtractLinks(input, "|Hspell:", result.Spells);
    ExtractLinks(input, "|Henchant:", result.Spells);
    std::string plain;
    std::string quantities;
    bool linkedName = false;
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        if (input.compare(i, 2, "|c") == 0 && i + 10 <= input.size())
            i += 9;
        else if (input.compare(i, 2, "|H") == 0)
        {
            auto end = input.find("|h", i + 2);
            if (end == std::string::npos)
                break;
            i = end + 1;
            linkedName = true;
            quantities += ' ';
        }
        else if (input.compare(i, 2, "|h") == 0 || input.compare(i, 2, "|r") == 0)
        {
            if (input.compare(i, 2, "|h") == 0)
                linkedName = false;
            ++i;
        }
        else
        {
            unsigned char c = static_cast<unsigned char>(input[i]);
            if (c == '?')
                result.Question = true;
            plain += std::isalnum(c) || c >= 128 ? static_cast<char>(std::tolower(c)) : ' ';
            if (!linkedName)
                quantities += std::isalnum(c) || c >= 128 || c == '-' ||
                                      (c == '.' && i > 0 && i + 1 < input.size() &&
                                       std::isdigit(static_cast<unsigned char>(input[i - 1])) &&
                                       std::isdigit(static_cast<unsigned char>(input[i + 1])))
                                  ? static_cast<char>(std::tolower(c))
                                  : ' ';
        }
    }
    static std::unordered_map<std::string, std::string> const aliases = {{"hiya", "hi"},
                                                                         {"helo", "hello"},
                                                                         {"heya", "hey"},
                                                                         {"thx", "thanks"},
                                                                         {"ty", "thanks"},
                                                                         {"tyvm", "thanks"},
                                                                         {"sry", "sorry"},
                                                                         {"pls", "please"},
                                                                         {"plz", "please"},
                                                                         {"u", "you"},
                                                                         {"prof", "profession"},
                                                                         {"profs", "professions"},
                                                                         {"ur", "your"},
                                                                         {"r", "are"},
                                                                         {"grats", "congratulations"},
                                                                         {"gz", "congratulations"},
                                                                         {"ding", "congratulations"},
                                                                         {"lfm", "recruit"},
                                                                         {"lfg", "group"},
                                                                         {"lf1m", "recruit"},
                                                                         {"brb", "wait"},
                                                                         {"afk", "wait"},
                                                                         {"inv", "invite"},
                                                                         {"dung", "dungeon"},
                                                                         {"bg", "battleground"},
                                                                         {"bgs", "battleground"},
                                                                         {"dont", "not"},
                                                                         {"cant", "not"},
                                                                         {"cannot", "not"},
                                                                         {"wont", "not"},
                                                                         {"wouldnt", "not"},
                                                                         {"isnt", "not"},
                                                                         {"isn", "not"},
                                                                         {"wasn", "not"},
                                                                         {"won", "not"},
                                                                         {"wouldn", "not"},
                                                                         {"didn", "not"},
                                                                         {"doesn", "not"},
                                                                         {"nah", "no"},
                                                                         {"nvm", "cancel"},
                                                                         {"tysm", "thanks"},
                                                                         {"thnx", "thanks"},
                                                                         {"soz", "sorry"},
                                                                         {"mb", "sorry"},
                                                                         {"grp", "group"},
                                                                         {"rdy", "ready"},
                                                                         {"mats", "materials"},
                                                                         {"recipies", "recipes"},
                                                                         {"wanna", "want"}};
    std::istringstream stream(plain);
    std::string token;
    while (stream >> token && result.Tokens.size() < 64)
    {
        auto alias = aliases.find(token);
        if (alias != aliases.end())
            token = alias->second;
        if (!result.Normalized.empty())
            result.Normalized += ' ';
        result.Normalized += token;
        result.Tokens.push_back(token);
    }
    std::istringstream quantityStream(quantities);
    while (quantityStream >> token && result.QuantityTokens.size() < 64)
        result.QuantityTokens.push_back(token);
    auto language = result.QuantityTokens;
    for (auto& word : language)
        if (auto alias = aliases.find(word); alias != aliases.end())
            word = alias->second;
    auto const& t = language;
    result.Negated = Any(language, {"not", "no", "never", "cancel"});
    for (std::size_t i = 1; i < language.size(); ++i)
        if (language[i] == "t" && (language[i - 1] == "don" || language[i - 1] == "can"))
            result.Negated = true;
    result.Correction = Any(t, {"actually", "instead", "meant", "correction"});
    result.TopicChange = Any(t, {"anyway", "anyways"});
    if (t.empty())
        return result;
    if (Any({t.front()}, {"where", "what", "which", "who", "when", "why", "how"}))
        result.Question = true;

    static std::unordered_set<std::string> const exactCommands = {
        "follow", "stay",  "flee",        "guard",    "attack", "pull",    "repair",   "trainer",
        "stats",  "who",   "help",        "leave",    "logout", "summon",  "teleport", "reset",
        "quests", "items", "maintenance", "position", "los",    "release", "bank",     "taxi"};
    static std::unordered_set<std::string> const commandPrefixes = {"co",       "nc",       "de",      "do",     "cast",
                                                                    "autogear", "accept",   "destroy", "debug",  "rpg",
                                                                    "log",      "sendmail", "equip",   "unequip"};
    bool actionCommand = (t[0] == "do" || t[0] == "d") && t.size() > 1 &&
                         !Any({t[1]}, {"you", "we", "they", "i", "he", "she", "not", "n", "the", "a", "an"});
    bool prefixedCommand = commandPrefixes.count(t[0]) && t[0] != "do";
    static std::unordered_set<std::string> const controlTags = {
        "tank",        "dps",      "heal",    "ranged",  "melee",  "rangeddps", "meleedps",
        "combat",      "nocombat", "warrior", "paladin", "hunter", "rogue",     "priest",
        "deathknight", "shaman",   "mage",    "warlock", "druid", "cultist", "necromancer"};
    bool addressedCommand =
        input[0] == '@' && (controlTags.count(t[0]) || std::isdigit(static_cast<unsigned char>(t[0][0])));
    result.IsCommand = (t.size() == 1 && exactCommands.count(t[0])) || actionCommand || prefixedCommand ||
                       addressedCommand || input[0] == '.' || input[0] == '#' || input[0] == '+' || input[0] == '-';
    if (result.IsCommand)
        return result;

    auto add = [&](Intent intent, bool condition)
    {
        if (condition)
            result.Intents.push_back(intent);
    };
    add(Intent::Greeting, Any(t, {"hello", "hi", "hey", "sup", "howdy", "yo"}));
    add(Intent::Farewell, Any(t, {"bye", "goodbye", "night", "cya", "later"}));
    add(Intent::Thanks, Contains(t, "thanks"));
    add(Intent::Apology, Any(t, {"sorry", "apologies"}));
    add(Intent::Congratulations, Contains(t, "congratulations"));
    add(Intent::Location, Any(t, {"where", "location", "directions", "lost"}));
    add(Intent::Activity, Any(t, {"doing", "plan", "plans", "busy", "progress"}));
    add(Intent::Introduction,
        Any(t, {"name", "yourself"}) || result.Normalized.find("who are you") != std::string::npos);
    add(Intent::Buy, Any(t, {"wtb", "buy", "buying"}));
    add(Intent::Sell, Any(t, {"wts", "sell", "selling"}));
    add(Intent::Trade, Any(t, {"trade", "price", "offer", "gold", "cost"}));
    add(Intent::Profession,
        Any(t, {"profession", "professions", "mining", "herbalism", "alchemy", "blacksmithing", "tailoring",
                "leatherworking", "engineering", "enchanting", "jewelcrafting", "inscription", "craft", "crafting",
                "cooking", "fishing", "riding", "firstaid", "making", "ingredients", "materials", "reagents"}) ||
        result.Normalized.find("first aid") != std::string::npos);
    add(Intent::Equipment, Any(t, {"gear", "equipment", "weapon", "armor", "upgrade"}));
    add(Intent::Class, Any(t, {"class", "spec", "talents", "rotation", "tank", "healer", "dps", "level", "lvl",
                               "cultist", "necromancer", "influence", "corruption", "godblade", "bulwark", "rime"}));
    add(Intent::Quest, !result.Quests.empty() || Any(t, {"quest", "quests", "questing", "objective"}));
    add(Intent::Raid, Any(t, {"raid", "icc", "naxx", "ulduar", "toc", "onyxia"}));
    add(Intent::Dungeon, Any(t, {"dungeon", "heroic", "instance", "deadmines", "stockades"}));
    add(Intent::Battleground, Any(t, {"battleground", "pvp", "warsong", "arathi", "alterac", "flag"}));
    add(Intent::Arena, Any(t, {"arena", "2v2", "3v3", "5v5", "rating"}));
    add(Intent::Group, Any(t, {"group", "party", "invite", "recruit", "join"}));
    add(Intent::Guild, Any(t, {"guild", "guildies", "recruiting"}));
    add(Intent::Friend, Any(t, {"friend", "friends", "remember"}));
    add(Intent::Ready, Any(t, {"ready", "rdy"}));
    add(Intent::Wait, Any(t, {"wait", "minute", "moment"}));
    add(Intent::Follow, Any(t, {"follow", "coming", "meet"}));
    add(Intent::Stop, Any(t, {"stop", "cancel"}));
    add(Intent::Combat, Any(t, {"fight", "aggro", "pull", "mob", "mobs", "oom"}));
    add(Intent::Death, Any(t, {"died", "dead", "death", "corpse", "res", "rez"}));
    add(Intent::Wipe, Any(t, {"wipe", "wiped", "wiping"}));
    add(Intent::Joke, Any(t, {"joke", "funny", "laugh", "lol", "lmao"}));
    add(Intent::SmallTalk,
        Any(t, {"day", "weekend", "food", "coffee", "music", "weather", "work", "hobby", "hobbies", "tired", "bored",
                "how", "session", "favourite", "favorite", "enjoy", "interesting", "today"}));
    add(Intent::Help, Any(t, {"help", "assist", "please"}));
    add(Intent::Disagree, Any(t, {"wrong", "disagree", "nope"}));
    add(Intent::Acknowledge, result.Normalized == "got it" || result.Normalized == "okay" ||
                                 result.Normalized == "ok" || result.Normalized == "all good" ||
                                 result.Normalized == "no worries" || result.Normalized == "no problem" ||
                                 result.Normalized == "you re welcome" || result.Normalized == "sure thing");
    if (result.Intents.empty() && Any(t, {"why", "what", "that", "it", "yes", "sure"}))
        result.Intents.push_back(Intent::Clarify);
    std::string languageText = " ";
    for (auto const& word : t)
        languageText += word + ' ';
    auto phrase = [&](std::initializer_list<char const*> phrases)
    {
        return std::any_of(phrases.begin(), phrases.end(), [&](auto text)
                           { return languageText.find(std::string(" ") + text + ' ') != std::string::npos; });
    };
    result.TopicChange = result.TopicChange || phrase({"new topic", "something else", "never mind"});
    // Courtesy and enthusiasm do not negate a request; a separate refusal still does.
    result.Negated = false;
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        bool courtesy = (t[i] == "can" && i + 2 < t.size() && t[i + 1] == "t" && t[i + 2] == "wait") ||
                        (t[i] == "no" && i + 1 < t.size() &&
                         Any({t[i + 1]}, {"problem", "worries", "rush"})) ||
                        (t[i] == "not" && i + 1 < t.size() && Any({t[i + 1]}, {"only", "just"})) ||
                        (t[i] == "not" && i + 2 < t.size() && t[i + 1] == "a" && t[i + 2] == "problem");
        if (!courtesy && (Any({t[i]}, {"not", "no", "never", "cancel"}) ||
            (i + 1 < t.size() && t[i + 1] == "t" && Any({t[i]}, {"don", "can"}))))
            result.Negated = true;
    }
    result.Act = result.Correction                      ? SpeechAct::Correction
                 : result.Negated                       ? SpeechAct::Refusal
                 : AcceptsInvitation(result.Normalized) ? SpeechAct::Agreement
                 : phrase({"can you", "could you", "would you", "will you", "please invite", "please help",
                           "would you mind", "any chance", "can i get", "could i get", "mind if i",
                           "let s group", "let s quest", "let s go"})
                     ? SpeechAct::Request
                 : result.Question ? SpeechAct::Question
                                   : SpeechAct::Statement;
    auto focus = [&](Intent intent, std::uint8_t confidence, bool matches)
    {
        if (matches && confidence > result.Confidence)
        {
            result.Focus = intent;
            result.Confidence = confidence;
        }
    };
    focus(Intent::Introduction, 95,
          phrase({"who are you", "what s your name", "what is your name", "introduce yourself"}));
    focus(Intent::Location, 95, phrase({"where are you", "where you at", "where are we", "your location"}));
    focus(Intent::Location, 95, phrase({"whereabouts are you", "where can i find you", "where should we meet"}));
    focus(Intent::Activity, 95,
          phrase({"what are you doing", "what you doing", "what s your plan", "what are your plans",
                  "what are you up to", "what you up to", "are you busy", "got a minute", "have a minute",
                  "what s next", "what are you working on"}));
    focus(Intent::Class, 90,
          phrase({"what class", "which class", "your class", "what level", "what lvl", "your level", "your spec",
                  "which spec", "what spec", "your talents", "your rotation"}));
    focus(Intent::Profession, 96,
          Any(t, {"profession", "professions", "mining", "herbalism", "alchemy", "blacksmithing", "tailoring",
                  "leatherworking", "engineering", "enchanting", "jewelcrafting", "inscription", "cooking", "fishing",
                  "riding", "firstaid"}) || phrase({"first aid", "what are you crafting", "what are you making",
                      "what you crafting", "what you making", "what ingredients", "which ingredients",
                      "what materials", "which materials", "what reagents"}));
    focus(Intent::Guild, 96,
          phrase({"which guild", "what guild", "your guild", "guild recruiting", "join your guild"}));
    focus(Intent::Equipment, 96,
          phrase({"your gear", "your equipment", "your armor", "your weapon", "your durability",
                  "what are you wearing", "what weapon are you using", "how s your gear"}));
    focus(Intent::SmallTalkAnswer, 95,
          phrase({"how are you", "how s it going", "how is it going", "how was your day", "how s your day",
                  "how is your day", "you doing okay", "you doing alright", "everything alright",
                  "having fun", "how s the leveling", "how s the grind"}));
    focus(Intent::Friend, 97, phrase({"remember me", "met before", "know me", "seen me before"}));
    focus(Intent::Group, 97,
          phrase({"invite me", "group with me", "party with me", "join my group", "join our group",
                  "got room for me", "got a spot", "need a hand", "want some company", "team up",
                  "can i tag along", "could i tag along", "want to group"}));
    focus(Intent::Stop, 99, phrase({"leave me alone", "stop talking", "stop chatting", "stop messaging",
                                   "please be quiet", "give me some space"}));
    focus(Intent::Farewell, 98, phrase({"catch you later", "see you around", "see you later", "got to log",
                                       "going to log off", "logging off", "take care"}));
    focus(Intent::Wipe, 98, Any(t, {"wipe", "wiped", "wiping"}));
    focus(Intent::PvpFocus, 98,
          phrase({"who should we focus", "who are you focusing", "who are you targeting", "which target first",
                  "focus their healer", "focus the healer", "same target", "switch targets", "swap targets",
                  "go on their healer", "hit the same target", "who do we hit", "who we hitting"}));
    focus(Intent::Arena, 97,
          phrase({"your rating", "your arena rating", "what rating", "which arena team", "your arena team",
                  "play twos", "play threes", "play fives", "2v2 rating", "3v3 rating", "5v5 rating"}));
    focus(Intent::Battleground, 97,
          phrase({"which battleground", "what battleground", "which bg", "what bg", "your battleground"}));
    focus(Intent::PvpRegroup, 98,
          phrase({"should we regroup", "we need to regroup", "let s regroup", "we re outnumbered",
                  "we are outnumbered", "stop going in alone", "wait for our team", "fall back together",
                  "back up together", "reset this fight", "wait for rez", "wait for res", "regroup please"}));
    focus(Intent::PvpPeel, 98,
          phrase({"peel for me", "peel for our healer", "peel for the healer", "help our healer",
                  "get them off me", "get him off me", "get her off me", "protect our healer",
                  "need peels", "need a peel", "peel please", "peels please", "peel me"}));
    // Everyday requests and questions the rules answer without a language model.
    focus(Intent::Advice, 93,
          phrase({"any tips", "got any tips", "any advice", "got any advice", "what should i do", "best way to level",
                  "how do i level", "how should i level", "where should i level", "where should i go",
                  "give me some advice", "any suggestions", "what do you recommend", "do you recommend",
                  "tips for", "advice for", "what would you do"}));
    focus(Intent::Duel, 98, Any(t, {"duel", "duels", "dueling"}));
    focus(Intent::Gold, 94,
          phrase({"how much gold", "got any gold", "got some gold", "spare some gold", "spare any gold", "spare gold",
                  "lend me", "can i borrow", "give me gold", "got any money", "how rich are you", "how much money",
                  "how much do you have", "gold do you have"}));
    focus(Intent::Opinion, 92,
          phrase({"your favorite", "your favourite", "what s your favorite", "what s your favourite", "do you like",
                  "what do you think of", "what do you think about", "best class", "best zone", "best dungeon",
                  "worst zone", "worst class", "which is better", "do you prefer", "would you rather"}));
    focus(Intent::Playtime, 95,
          phrase({"how long have you been playing", "how long you been playing", "how long have you played",
                  "how long you played", "how long have you been here", "how long have you been online",
                  "how long you been online", "when did you start playing", "been playing long",
                  "how long have you played this"}));
    focus(Intent::Summon, 97,
          phrase({"summon me", "summon us", "can you summon", "could you summon", "need a summon", "need summon",
                  "ritual of summoning", "summoning ritual", "summoning stone", "meeting stone", "click the portal",
                  "click the ritual", "click the stone", "help me summon", "help summon", "help with the summon",
                  "help with the ritual", "need a third", "need one more for", "join the ritual", "join the summon",
                  "come to the stone", "summon the group", "summon my friend", "warlock summon", "lock summon",
                  "summon please", "summon pls", "summon plz", "clicky", "clickers"}));
    bool const emoteWord = Any(t, {"dance", "wave", "bow", "cheer", "salute", "kneel", "applaud", "clap", "flex",
                                   "sit", "point"});
    focus(Intent::Emote, 96,
          emoteWord &&
              (result.Act == SpeechAct::Request || t.size() <= 6 || phrase({"do a", "show me", "give me a"})));
    if (!result.Focus && !result.Intents.empty())
        result.Confidence = result.Intents.size() == 1 ? 70 : 45;
    return result;
}

char const* ChatRules::Name(Intent intent)
{
    constexpr std::array names = {"unknown",
                                  "greeting",
                                  "farewell",
                                  "thanks",
                                  "apology",
                                  "congratulations",
                                  "introduction",
                                  "activity",
                                  "location",
                                  "quest",
                                  "help",
                                  "group",
                                  "dungeon",
                                  "raid",
                                  "battleground",
                                  "arena",
                                  "trade",
                                  "buy",
                                  "sell",
                                  "profession",
                                  "equipment",
                                  "class",
                                  "guild",
                                  "friend",
                                  "ready",
                                  "wait",
                                  "follow",
                                  "stop",
                                  "combat",
                                  "death",
                                  "wipe",
                                  "joke",
                                  "smalltalk",
                                  "disagree",
                                  "clarify",
                                  "smalltalk_answer",
                                  "acknowledge",
                                  "group_offer",
                                  "progress",
                                  "trade_offer",
                                  "group_accept",
                                  "trade_cancel",
                                  "friend_remembered",
                                  "quest_known",
                                  "event_death",
                                  "event_wipe",
                                  "event_victory",
                                  "event_quest",
                                  "party_wait",
                                  "party_ready",
                                  "item_known",
                                  "spell_known",
                                  "pvp_focus",
                                  "pvp_regroup",
                                  "pvp_peel",
                                  "advice",
                                  "emote",
                                  "duel",
                                  "gold",
                                  "opinion",
                                  "playtime",
                                  "summon"};
    return static_cast<std::size_t>(intent) < names.size() ? names[static_cast<std::size_t>(intent)] : "invalid";
}

Intent ChatRules::Resolve(ParsedMessage const& message, ConversationState const& conversation, Timestamp now)
{
    if (message.IsCommand)
        return Intent::Unknown;
    if (!message.Quests.empty())
        return Intent::Quest;
    if (!message.Items.empty())
    {
        for (auto intent : {Intent::Buy, Intent::Sell, Intent::Trade})
            if (std::find(message.Intents.begin(), message.Intents.end(), intent) != message.Intents.end())
                return intent;
        if (message.Question && message.Spells.empty() &&
            (!message.Negated || Any(message.Tokens, {"have", "own", "carry", "wear", "know", "learned"})))
            return Intent::ItemKnown;
    }
    if (!message.Spells.empty() && message.Question && message.Items.empty() &&
        (!message.Negated || Any(message.Tokens, {"know", "learned", "have"})))
        return Intent::SpellKnown;
    if (message.Focus)
        return *message.Focus;
    if (message.Intents.empty())
        return Intent::Unknown;
    if (!message.Correction && !message.TopicChange && conversation.LastMessage &&
        now < conversation.LastMessage + CONVERSATION_EXPIRY && message.Question &&
        (message.Intents.front() == Intent::Unknown || message.Intents.front() == Intent::Clarify ||
         Any(message.Tokens, {"it", "that", "those"})) &&
        message.Quests.empty() && message.Items.empty() && message.Spells.empty() &&
        (Any(message.Tokens, {"it", "that", "those", "which", "why"}) ||
         message.Normalized == "how much" || message.Normalized == "where" ||
         message.Normalized == "what about you"))
    {
        // Context can recover a subject, never repeat an invitation or financial instruction.
        switch (conversation.Topic)
        {
            case Intent::Quest:
            case Intent::QuestKnown:
            case Intent::ItemKnown:
            case Intent::SpellKnown:
            case Intent::Profession:
            case Intent::Equipment:
            case Intent::Activity:
            case Intent::Location:
            case Intent::Class:
                return conversation.Topic;
            default:
                break;
        }
    }
    if (message.Confidence < 60)
    {
        auto family = [](Intent intent)
        {
            switch (intent)
            {
                case Intent::Greeting:
                case Intent::Thanks:
                case Intent::Help:
                case Intent::Apology:
                    return 0;
                case Intent::Group:
                case Intent::Dungeon:
                case Intent::Raid:
                    return 1;
                case Intent::Buy:
                case Intent::Sell:
                case Intent::Trade:
                    return 2;
                case Intent::Combat:
                case Intent::Death:
                case Intent::Wipe:
                    return 3;
                default:
                    return 10 + static_cast<int>(intent);
            }
        };
        int subject = 0;
        for (auto intent : message.Intents)
            if (auto current = family(intent); current)
            {
                if (subject && subject != current)
                    return Intent::Clarify;
                subject = current;
            }
    }
    for (Intent intent : message.Intents)
        if (intent != Intent::Greeting && intent != Intent::Thanks && intent != Intent::Help)
            return intent == Intent::SmallTalk && message.Question ? Intent::SmallTalkAnswer : intent;
    return message.Intents.front();
}

QuestSubject ChatRules::QuestTopic(ParsedMessage const& message)
{
    if (Any(message.QuantityTokens, {"where", "directions", "location", "find"}))
        return QuestSubject::Directions;
    if (Any(message.QuantityTokens, {"level", "lvl", "difficulty"}))
        return QuestSubject::Level;
    return QuestSubject::Progress;
}

std::string ChatRules::Facts(Intent intent, BotActivitySnapshot const& s, BotLifeGoal const& goal)
{
    switch (intent)
    {
        case Intent::Progress:
            return std::to_string(s.Level);
        case Intent::GroupOffer:
            return s.HasQuest ? s.QuestName : s.ZoneName;
        case Intent::Introduction:
            return s.Name + ", level " + std::to_string(s.Level) + " " + s.ClassName + ".";
        case Intent::Location:
            return s.ZoneName.empty() ? "" : s.ZoneName + ".";
        case Intent::Class:
            return s.ClassName + ", level " + std::to_string(s.Level) + ".";
        case Intent::Activity:
        {
            if (!s.Alive)
                return "recovering from a death.";
            if (s.InArena)
                return "playing an arena match.";
            if (s.InBattleground)
                return "playing a battleground.";
            if (s.InCombat)
                return "in a fight.";
            if (s.Trading)
                return "finishing a trade.";
            if (s.InInstance)
                return "inside an instance.";
            if (s.InQueue)
                return "waiting for a queued activity.";
            if (goal.State == Outcome::Blocked)
                return "working out another plan after a setback.";
            if (goal.State == Outcome::Succeeded || goal.State == Outcome::Cancelled)
                return "figuring out my next plan.";
            switch (goal.Kind)
            {
                case Goal::Quest:
                    return s.HasQuest ? "questing." : "looking for suitable quests.";
                case Goal::Gather:
                    return "gathering materials.";
                case Goal::Repair:
                    return "getting my equipment repaired.";
                case Goal::Restock:
                    return "restocking supplies.";
                case Goal::Train:
                    return "getting some training.";
                case Goal::Sell:
                    return "clearing out my bags.";
                case Goal::Dungeon:
                    return "finding a dungeon run.";
                case Goal::Raid:
                    return "looking into a raid.";
                case Goal::Battleground:
                    return s.InBattleground ? "playing a battleground." : "trying to join a battleground.";
                case Goal::Arena:
                    return s.InArena ? "playing an arena match." : "getting ready for arena.";
                case Goal::Craft:
                    return "working on a crafting project.";
                case Goal::Auction:
                    return "selling some goods.";
                case Goal::Trade:
                    return "arranging a trade.";
                case Goal::Mail:
                    return "checking my mail.";
                case Goal::Bank:
                    return "sorting out storage.";
                case Goal::Rest:
                    return "taking a breather.";
                case Goal::Travel:
                    return "travelling.";
                default:
                    return "figuring out my next plan.";
            }
        }
        case Intent::SmallTalkAnswer:
        {
            switch (goal.Kind)
            {
                case Goal::Quest:
                    return "I've got questing on my list.";
                case Goal::Gather:
                    return "I'm looking to gather some materials.";
                case Goal::Repair:
                    return "I need to get my equipment repaired.";
                case Goal::Restock:
                    return "I need to sort out some supplies.";
                case Goal::Train:
                    return "I've got some training to take care of.";
                case Goal::Sell:
                    return "My bags could use a clear-out.";
                case Goal::Dungeon:
                    return "I'm interested in a dungeon run.";
                case Goal::Raid:
                    return "I'm looking into raiding.";
                case Goal::Battleground:
                    return "I'm thinking about battlegrounds.";
                case Goal::Arena:
                    return "I've got arena on my mind.";
                case Goal::Craft:
                    return "I'm looking at some crafting.";
                case Goal::Auction:
                    return "I've got some goods I want to sell.";
                case Goal::Mail:
                    return "I need to deal with my mail.";
                case Goal::Bank:
                    return "I need to organize some storage.";
                case Goal::Recover:
                    return "I need to recover before much else.";
                case Goal::Survive:
                    return "I've got a fight to deal with.";
                case Goal::Rest:
                    return "I could use a moment to recover.";
                default:
                    return "I'm seeing where the session takes me.";
            }
        }
        case Intent::Quest:
            return s.HasQuest && !s.QuestName.empty() ? s.QuestName + "." : "";
        case Intent::Equipment:
            return "Durability: " + std::to_string(s.Durability) + "%.";
        default:
            return "";
    }
}

Timestamp ChatRules::ReplyDelay(BotPersona const& persona, std::size_t incomingBytes, std::size_t outgoingBytes)
{
    return 2 + incomingBytes / 45 + outgoingBytes / (3 + persona.SpeechStyle / 15) + (100 - persona.Sociability) / 30;
}

std::string ChatRules::Style(std::string text, BotPersona const& persona, std::uint32_t entropy)
{
    if (persona.SpeechStyle < 35)
        for (char& character : text)
            if (static_cast<unsigned char>(character) < 128)
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    if (persona.SpeechStyle < 20 && entropy % 25 == 0)
    {
        auto position = text.find(" you ");
        if (position != std::string::npos)
            text.replace(position, 5, " u ");
    }
    return text;
}

std::uint64_t ActivityRoster::ZoneKey(std::uint32_t map, std::uint32_t zone)
{
    return static_cast<std::uint64_t>(map) << 32 | zone;
}

std::unordered_set<BotId> ActivityRoster::Select(std::vector<ActivityMember> const& members,
                                                 std::unordered_set<std::uint64_t> const& humanZones,
                                                 std::size_t budget, std::uint64_t epoch)
{
    std::unordered_set<BotId> active;
    std::unordered_map<std::uint64_t, std::vector<BotId>> groups;
    std::unordered_set<std::uint64_t> forcedGroups;
    for (auto const& member : members)
    {
        std::uint64_t group = member.Group ? member.Group : (1ULL << 32) | member.Id;
        groups[group].push_back(member.Id);
        if (member.Committed || humanZones.count(member.Zone))
            forcedGroups.insert(group);
    }
    std::vector<std::uint64_t> candidates;
    for (auto const& [group, ids] : groups)
    {
        if (forcedGroups.count(group))
            active.insert(ids.begin(), ids.end());
        else
            candidates.push_back(group);
    }
    if (!budget)
        return active;
    std::sort(candidates.begin(), candidates.end());
    if (!candidates.empty())
    {
        std::size_t population = 0;
        for (auto group : candidates)
            population += groups.at(group).size();
        auto slot = static_cast<std::size_t>((epoch * std::max<std::size_t>(budget, 1)) % population);
        std::size_t offset = 0;
        while (slot >= groups.at(candidates[offset]).size())
        {
            slot -= groups.at(candidates[offset]).size();
            ++offset;
        }
        std::rotate(candidates.begin(), candidates.begin() + offset, candidates.end());
    }
    std::size_t selected = 0;
    for (auto group : candidates)
    {
        if (selected >= budget)
            break;
        auto const& ids = groups.at(group);
        active.insert(ids.begin(), ids.end());
        selected += ids.size();
    }
    return active;
}

std::unordered_set<BotId> ActivityRoster::Update(std::vector<ActivityMember> const& members,
                                                 std::unordered_set<std::uint64_t> const& humanZones,
                                                 std::size_t budget, std::uint64_t epoch, Timestamp elapsed)
{
    auto active = Select(members, humanZones, 0, epoch);
    if (!_epoch || *_epoch != epoch || _budget != budget)
    {
        auto selected = Select(members, humanZones, budget, epoch);
        for (auto id : active)
            selected.erase(id);
        _previous = _epoch && *_epoch + 1 == epoch && _budget == budget ? std::move(_background) : selected;
        _background = std::move(selected);
        _epoch = epoch;
        _budget = budget;
    }
    auto present = std::count_if(members.begin(), members.end(),
                                 [&](auto const& member) { return _background.contains(member.Id); });
    if (static_cast<std::size_t>(present) < budget)
    {
        std::unordered_set<BotId> online;
        for (auto const& member : members)
            online.insert(member.Id);
        std::erase_if(_background, [&](BotId id) { return !online.contains(id); });
        auto candidates = Select(members, humanZones, budget + _background.size(), epoch);
        for (auto id : candidates)
        {
            if (_background.size() >= budget)
                break;
            if (!active.contains(id))
            {
                _background.insert(id);
                _previous.insert(id);
            }
        }
    }
    // Commitments change every tick; they must not move the background rotation's starting slot.
    std::unordered_set<std::uint64_t> selectedGroups;
    for (auto const& member : members)
    {
        auto unit = member.Group ? member.Group : (1ULL << 32) | member.Id;
        auto const& background = elapsed < (unit * 2654435761ULL) % 30 ? _previous : _background;
        if (background.contains(member.Id))
        {
            active.insert(member.Id);
            if (member.Group)
                selectedGroups.insert(member.Group);
        }
    }
    for (auto const& member : members)
        if (member.Group && selectedGroups.contains(member.Group))
            active.insert(member.Id);
    return active;
}
}  // namespace Advai
