/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_MODEL_H
#define PLAYERBOTS_ADVAI_MODEL_H

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Advai
{
using BotId = std::uint32_t;
using Timestamp = std::uint64_t;
constexpr std::size_t MAX_RECENT_LINES = 96;
constexpr std::size_t MAX_CONVERSATION_MESSAGES = 6;
constexpr std::size_t MIN_TEXT_VARIANTS = 36;
constexpr std::size_t MAX_CHAT_BYTES = 255;
constexpr std::size_t MAX_RESIDENT_CONTACTS = 128;
constexpr std::size_t MAX_DIRTY_CONTACTS = 256;
constexpr std::size_t MAX_PENDING_RELATIONSHIP_EVENTS = 1024;
constexpr std::size_t MAX_INTERRUPTED_GOALS = 12;

enum class Goal : std::uint8_t
{
    None,
    Survive,
    Recover,
    Repair,
    Sell,
    Restock,
    Train,
    Quest,
    Gather,
    Craft,
    Trade,
    Auction,
    Bank,
    Mail,
    Dungeon,
    Raid,
    Battleground,
    Arena,
    Socialize,
    Rest,
    Travel,
    Count
};

enum class Intent : std::uint8_t
{
    Unknown,
    Greeting,
    Farewell,
    Thanks,
    Apology,
    Congratulations,
    Introduction,
    Activity,
    Location,
    Quest,
    Help,
    Group,
    Dungeon,
    Raid,
    Battleground,
    Arena,
    Trade,
    Buy,
    Sell,
    Profession,
    Equipment,
    Class,
    Guild,
    Friend,
    Ready,
    Wait,
    Follow,
    Stop,
    Combat,
    Death,
    Wipe,
    Joke,
    SmallTalk,
    Disagree,
    Clarify,
    SmallTalkAnswer,
    Acknowledge,
    GroupOffer,
    Progress,
    TradeOffer,
    GroupAccept,
    TradeCancel,
    FriendRemembered,
    QuestKnown,
    EventDeath,
    EventWipe,
    EventVictory,
    EventQuest,
    PartyWait,
    PartyReady,
    ItemKnown,
    SpellKnown,
    PvpFocus,
    PvpRegroup,
    PvpPeel,
    Advice,
    Emote,
    Duel,
    Gold,
    Opinion,
    Playtime,
    Summon,
    Count
};

enum class Outcome : std::uint8_t
{
    Proposed,
    Started,
    Succeeded,
    Blocked,
    Cancelled
};
enum class ClassProfile : std::uint8_t
{
    Classic,
    Cultist,
    Necromancer,
    Count
};
enum class QuestSubject : std::uint8_t
{
    Progress,
    Level,
    Directions
};
enum class SocialEventKind : std::uint8_t
{
    Conversation,
    Assistance,
    SharedQuest,
    Trade,
    Victory,
    Wipe,
    Death,
    BrokenCommitment,
    GuildParticipation
};

struct BotPersona
{
    std::uint32_t Seed = 0;
    std::uint8_t Sociability = 0;
    std::uint8_t Patience = 0;
    std::uint8_t Generosity = 0;
    std::uint8_t Competitiveness = 0;
    std::uint8_t RiskTolerance = 0;
    std::uint8_t Frugality = 0;
    std::uint8_t Skill = 0;
    std::uint8_t SpeechStyle = 0;
    Goal Preference = Goal::Quest;

    static BotPersona Create(std::uint32_t seed);
    static BotPersona ForCharacter(BotId id);
    std::uint32_t ReactionMilliseconds(std::uint32_t context = 0) const;
    float FormationOffset() const;
    float SoloTargetCost(std::uint32_t target, float distance, int levelDifference, bool questTarget) const;
    std::uint32_t BattlegroundRole(std::uint32_t instance) const;
    std::uint32_t OptionalHesitationMilliseconds(std::uint32_t context) const;
};

struct BotActivitySnapshot
{
    Timestamp Now = 0;
    BotId Id = 0;
    std::uint32_t Map = 0;
    std::uint32_t Zone = 0;
    std::uint32_t Group = 0;
    BotId GroupLeader = 0;
    std::uint32_t Guild = 0;
    std::uint32_t Money = 0;
    std::uint32_t QuestId = 0;
    std::uint16_t ProfessionSkill = 0;
    std::uint16_t AverageItemLevel = 0;
    Goal ProfessionActivity = Goal::None;
    std::uint8_t Level = 1;
    std::uint8_t Health = 100;
    std::uint8_t Mana = 100;
    std::uint8_t Durability = 100;
    std::uint8_t FreeBagSlots = 16;
    bool Alive = true;
    bool InCombat = false;
    bool InInstance = false;
    bool InTransit = false;
    bool ActionInProgress = false;
    bool Trading = false;
    bool Moving = false;
    bool HumanControlled = false;
    bool HumanGroup = false;
    bool PlayerGuild = false;
    bool HasQuest = false;
    bool QuestComplete = false;
    bool NeedsFood = false;
    bool NeedsDrink = false;
    bool UsesMana = false;
    bool NeedsAmmo = false;
    bool NeedsReagent = false;
    bool NeedsPetFood = false;
    bool NeedsTool = false;
    bool HasBankSupplies = false;
    bool NeedsMount = false;
    bool NeedsRecipe = false;
    bool CanGather = false;
    bool CanCraft = false;
    bool NeedsTraining = false;
    bool HasMail = false;
    bool AwaitingDelivery = false;
    bool BankNeeded = false;
    bool HasTradeStock = false;
    bool HasVendorStock = false;
    bool HasAuctionStock = false;
    bool GroupReady = false;
    bool InQueue = false;
    bool InBattleground = false;
    bool InArena = false;
    bool HasPet = false;
    std::array<Timestamp, static_cast<std::size_t>(Goal::Count)> RetryAfter{};
    std::string Name;
    std::string ZoneName;
    std::string ClassName;
    std::string SpecName;
    std::string WeaponName;
    std::string GuildName;
    std::string QuestName;

    bool HasCommitment() const;
    bool RequiresContinuousActivity() const;
};

struct BotLifeGoal
{
    Goal Kind = Goal::None;
    Timestamp Started = 0;
    Timestamp CommittedUntil = 0;
    Timestamp RetryAfter = 0;
    std::uint32_t Target = 0;
    std::uint8_t Failures = 0;
    Outcome State = Outcome::Proposed;
    std::string Reason;
};

enum class SocialRequest : std::uint8_t
{
    PartyInvite,
    GuildInvite
};

struct BotActionRequest
{
    Goal Kind = Goal::None;
    BotId Requester = 0;
    std::uint32_t Target = 0;
    Timestamp Expires = 0;
    SocialRequest Action = SocialRequest::PartyInvite;
};

struct BotActionResult
{
    Goal Kind = Goal::None;
    Outcome State = Outcome::Blocked;
    std::string Reason;
};

struct BotSocialEvent
{
    SocialEventKind Kind = SocialEventKind::Conversation;
    BotId Other = 0;
    Timestamp At = 0;
    bool Positive = true;
    std::uint32_t Entity = 0;
};

struct TradeOffer
{
    BotId Peer = 0;
    std::uint32_t Item = 0;
    std::uint32_t Quantity = 0;
    std::uint32_t Copper = 0;
    Timestamp Expires = 0;
    bool Selling = true;
    bool Agreed = false;
    bool ByMail = false;
};

struct AuctionDelivery
{
    std::uint32_t ItemGuid = 0;
    std::uint32_t Item = 0;
    std::uint32_t Quantity = 0;
    Timestamp Ordered = 0;
    Timestamp Deadline = 0;
};

struct Relationship
{
    std::int16_t Affinity = 0;
    std::uint32_t Encounters = 0;
    Timestamp LastEvent = 0;
    Timestamp LastReward = 0;
    std::uint64_t Revision = 0;
    void Observe(BotSocialEvent const& event);
};

struct Lifecycle
{
    bool Initialized = false;
    bool WasPlayerGuild = false;
    Timestamp Joined = 0;
    Timestamp BreakDue = 0;
    Timestamp ReturnAt = 0;
    Timestamp SessionDue = 0;
    Timestamp LoginAfter = 0;
    std::int16_t GuildSatisfaction = 0;
    std::uint32_t GuildIdentity = 0;
    bool GuildIdentityKnown = false;

    void Start(Timestamp now, bool playerGuild, std::uint32_t entropy);
    void UpdateGuild(Timestamp now, bool playerGuild, std::uint32_t entropy,
                     std::optional<std::uint32_t> guild = std::nullopt);
    bool CanDepart(BotActivitySnapshot const& snapshot) const;
    void BeginBreak(Timestamp now, std::uint32_t entropy);
    void StartSession(Timestamp now, std::uint32_t entropy);
    bool CanPause(BotActivitySnapshot const& snapshot) const;
    void BeginPause(Timestamp now, std::uint32_t entropy);
    bool Eligible(Timestamp now) const;
    bool ShouldLeaveGuild() const;
    Timestamp DepartureDue() const;
    static std::uint64_t RecommendedReserveCharacters(std::uint32_t onlineTarget);
};

enum class SpeechAct : std::uint8_t
{
    Unknown,
    Statement,
    Question,
    Request,
    Agreement,
    Refusal,
    Correction
};

struct ParsedMessage
{
    std::string Normalized;
    std::vector<std::string> Tokens;
    std::vector<std::string> QuantityTokens;
    std::vector<std::uint32_t> Items;
    std::vector<std::uint32_t> Quests;
    std::vector<std::uint32_t> Spells;
    std::vector<Intent> Intents;
    bool IsCommand = false;
    bool Negated = false;
    bool Question = false;
    bool Correction = false;
    bool TopicChange = false;
    SpeechAct Act = SpeechAct::Unknown;
    std::optional<Intent> Focus;
    std::uint8_t Confidence = 0;
};

struct ConversationState
{
    BotId Partner = 0;
    Intent Topic = Intent::Unknown;
    Timestamp LastMessage = 0;
    Timestamp NextSpeak = 0;
    std::uint8_t Messages = 0;
    std::deque<std::uint32_t> RecentVariants;
    std::deque<std::string> RecentIncoming;
    std::uint32_t QuestFocus = 0;
    std::uint32_t ItemFocus = 0;
    std::uint32_t SpellFocus = 0;
    std::uint32_t ProfessionFocus = 0;

    bool CanReply(Timestamp now, BotId partner, std::string const& normalized) const;
    void Remember(Timestamp now, BotId partner, ParsedMessage const& message, std::uint32_t variant);
    ConversationState ForPartner(BotId partner, Timestamp now) const;
};

class GoalPlanner
{
public:
    static Goal Choose(BotActivitySnapshot const& snapshot, BotPersona const& persona, BotLifeGoal const& current,
                       Goal ambition = Goal::None);
    static void ApplyResult(BotLifeGoal& goal, BotActionResult const& result, Timestamp now);
    static char const* Name(Goal goal);
};

class ChatRules
{
public:
    static ParsedMessage Parse(std::string const& input);
    static std::vector<std::string> Requests(std::string const& input);
    static char const* Name(Intent intent);
    static Intent Resolve(ParsedMessage const& message, ConversationState const& conversation, Timestamp now);
    static QuestSubject QuestTopic(ParsedMessage const& message);
    static std::string Facts(Intent intent, BotActivitySnapshot const& snapshot, BotLifeGoal const& goal);
    static Timestamp ReplyDelay(BotPersona const& persona, std::size_t incomingBytes, std::size_t outgoingBytes);
    static std::string Style(std::string text, BotPersona const& persona, std::uint32_t entropy);
};

struct ActivityMember
{
    BotId Id = 0;
    std::uint64_t Zone = 0;
    std::uint32_t Group = 0;
    bool Committed = false;
};

struct ServiceLocation
{
    std::uint32_t Entry = 0;
    std::uint32_t Map = 0;
    std::uint32_t Flags = 0;
    std::uint32_t Faction = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

class ActivityRoster
{
public:
    std::unordered_set<BotId> Update(std::vector<ActivityMember> const& members,
                                     std::unordered_set<std::uint64_t> const& humanZones, std::size_t backgroundBudget,
                                     std::uint64_t epoch, Timestamp elapsed);
    static std::unordered_set<BotId> Select(std::vector<ActivityMember> const& members,
                                            std::unordered_set<std::uint64_t> const& humanZones,
                                            std::size_t backgroundBudget, std::uint64_t epoch);
    static std::uint64_t ZoneKey(std::uint32_t map, std::uint32_t zone);

private:
    std::optional<std::uint64_t> _epoch;
    std::size_t _budget = 0;
    std::unordered_set<BotId> _background;
    std::unordered_set<BotId> _previous;
};
}  // namespace Advai

#endif
