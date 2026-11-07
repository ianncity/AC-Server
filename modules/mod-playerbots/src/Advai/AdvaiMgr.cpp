/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiBehavior.h"
#include "AdvaiMgr.h"
#include "GameTime.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>

#include "AdvaiBackport.h"
#include "AdvaiConsent.h"
#include "AdvaiDialogue.h"
#include "AdvaiEconomy.h"
#include "AdvaiGuild.h"
#include "AdvaiKnowledge.h"
#include "AdvaiLlm.h"
#include "AdvaiMarket.h"
#include "AdvaiMovement.h"
#include "AdvaiProfession.h"
#include "AdvaiPvp.h"
#include "AdvaiTravel.h"
#include "AdvaiMetrics.h"
#include "AdvaiSocial.h"
#include "AdvaiStateCodec.h"
#include "AdvaiSupplies.h"
#include "AiObjectContext.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "ChatHelper.h"
#include "ChatTriggerContext.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Event.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GuildPackets.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "LFGMgr.h"
#include "Mail.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PetitionMgr.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotOperations.h"
#include "PlayerbotTextMgr.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "PlayerbotsDatabase.h"
#include "QueryCallback.h"
#include "RBAC.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "RandomPlayerbotFactory.h"
#include "SocialMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "Transport.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace Advai
{
namespace
{
std::uint64_t NowMs() { return static_cast<std::uint64_t>(GameTime::GetGameTimeMS().count()); }
// Rule-based replies take one to two seconds to "type".
std::uint32_t TypingDelayMs() { return 1000 + urand(0, 1000); }

// Which emote a request such as "dance for me" or "can you wave" asks for (0 = none recognised).
std::uint32_t EmoteFromTokens(std::vector<std::string> const& tokens)
{
    static std::unordered_map<std::string, std::uint32_t> const emotes = {
        {"dance", EMOTE_STATE_DANCE},      {"wave", EMOTE_ONESHOT_WAVE},       {"bow", EMOTE_ONESHOT_BOW},
        {"cheer", EMOTE_ONESHOT_CHEER},    {"salute", EMOTE_ONESHOT_SALUTE},   {"kneel", EMOTE_ONESHOT_KNEEL},
        {"applaud", EMOTE_ONESHOT_APPLAUD}, {"clap", EMOTE_ONESHOT_APPLAUD},   {"flex", EMOTE_ONESHOT_FLEX},
        {"sit", EMOTE_STATE_SIT},          {"point", EMOTE_ONESHOT_POINT}};
    for (auto const& token : tokens)
        if (auto found = emotes.find(token); found != emotes.end())
            return found->second;
    return 0;
}

void PerformEmote(PlayerbotAI* ai, std::uint32_t emote)
{
    Player* bot = ai->GetBot();
    if (!emote || !bot->IsAlive() || bot->IsInCombat() || bot->isMoving() || bot->IsMounted() ||
        bot->IsNonMeleeSpellCast(false))
        return;
    if (emote == EMOTE_STATE_SIT)
    {
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        ai->AddTimedEvent([ai] { if (ai->GetBot()->IsSitState() && !ai->GetBot()->IsInCombat()) ai->GetBot()->SetStandState(UNIT_STAND_STATE_STAND); }, 8000);
    }
    else if (emote == EMOTE_STATE_DANCE)
    {
        bot->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_DANCE);
        ai->AddTimedEvent([ai] { ai->GetBot()->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE); }, 7000 + urand(0, 3000));
    }
    else
        bot->HandleEmoteCommand(emote);
}

// True when the rule-based parser cannot answer on its own: it did not recognise the message, the conversation has run
// past what canned lines carry well, or the player asked for something only the model can interpret.
bool NeedsLlm(ParsedMessage const& message, Intent intent, ConversationState const& conversation)
{
    if (intent == Intent::Unknown || intent == Intent::Clarify)
        return true;
    bool const openEnded = intent == Intent::SmallTalk || intent == Intent::SmallTalkAnswer || intent == Intent::Joke ||
                           intent == Intent::Disagree || intent == Intent::Opinion || intent == Intent::Advice ||
                           intent == Intent::Help;
    if (openEnded && conversation.Messages >= 2)
        return true;
    bool const handledByRules =
        intent == Intent::Greeting || intent == Intent::Farewell || intent == Intent::Thanks ||
        intent == Intent::Apology || intent == Intent::Congratulations || intent == Intent::Introduction ||
        intent == Intent::Activity || intent == Intent::Location || intent == Intent::Quest ||
        intent == Intent::QuestKnown || intent == Intent::Group || intent == Intent::GroupOffer ||
        intent == Intent::Dungeon || intent == Intent::Raid || intent == Intent::Battleground ||
        intent == Intent::Arena || intent == Intent::Trade || intent == Intent::Buy || intent == Intent::Sell ||
        intent == Intent::TradeOffer || intent == Intent::TradeCancel || intent == Intent::Guild ||
        intent == Intent::Friend || intent == Intent::FriendRemembered || intent == Intent::Ready ||
        intent == Intent::Wait || intent == Intent::Follow || intent == Intent::Stop || intent == Intent::Acknowledge ||
        intent == Intent::Emote || intent == Intent::Duel || intent == Intent::Gold || intent == Intent::Playtime ||
        intent == Intent::Summon ||
        intent == Intent::Class || intent == Intent::Profession || intent == Intent::Equipment ||
        intent == Intent::ItemKnown || intent == Intent::SpellKnown;
    // A polite request for something the rules cannot carry out themselves (craft it, cast it, give it, repair ...).
    if (message.Act == SpeechAct::Request && (!handledByRules || !message.Items.empty() || !message.Spells.empty()) &&
        intent != Intent::Buy && intent != Intent::Sell && intent != Intent::Trade)
        return true;
    // A question the parser only half understood.
    return message.Question && message.Confidence < 60 && !handledByRules;
}
}  // namespace
namespace
{
constexpr Timestamp MESSAGE_LIFETIME = 90;
constexpr Timestamp CLAIM_LIFETIME = 20;

Timestamp Now() { return static_cast<Timestamp>(std::time(nullptr)); }

bool TraceDrop(Player const* sender, Player const* bot, int line)
{
    if (sender && bot && sender->GetSession() && !sender->GetSession()->IsHeadless())
        LOG_INFO("playerbots", "ADVAI chat trace: {} dropped message from {} at AdvaiMgr.cpp:{}", bot->GetName(),
                 sender->GetName(), line);
    return true;
}

bool RegisteredCommand(std::string text)
{
    static std::unordered_set<std::string> const commands = []
    {
        ChatTriggerContext context;
        auto supported = context.supports();
        return std::unordered_set<std::string>(supported.begin(), supported.end());
    }();
    auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return false;
    auto last = text.find_last_not_of(" \t\r\n");
    text = text.substr(first, last - first + 1);
    if (commands.count(text))
        return true;
    for (auto split = text.find(' '); split != std::string::npos; split = text.find(' ', split + 1))
    {
        auto command = text.substr(0, split);
        auto argument = text.find_first_not_of(' ', split);
        if (argument == std::string::npos || !commands.count(command))
            continue;
        auto start = static_cast<unsigned char>(text[argument]);
        if (command.size() <= 2 || std::isdigit(start) || start == '|' || start == '+' || start == '-' || start == '?')
            return true;
    }
    return false;
}

bool HumanGroup(Player* player)
{
    if (Group* group = player->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource();
                member && member->GetSession() && !member->GetSession()->IsHeadless())
                return true;
    return false;
}

bool AllowedChannel(Player* bot, Player* sender, std::uint32_t channel)
{
    if (!sender || !sender->IsInWorld() || sender == bot || !sender->GetSession() || !bot->GetSession() ||
        !bot->CanSpeak() || bot->GetTeamId() != sender->GetTeamId())
        return false;
    if ((bot->GetSocial() && bot->GetSocial()->HasIgnore(sender->GetGUID())) ||
        (sender->GetSocial() && sender->GetSocial()->HasIgnore(bot->GetGUID())))
        return false;
    if (bot->HasAura(1852) && (channel != CHAT_MSG_WHISPER || !sender->IsGameMaster()))
        return false;
    bool privateGroup = channel == CHAT_MSG_GUILD || channel == CHAT_MSG_PARTY || channel == CHAT_MSG_RAID ||
                        channel == CHAT_MSG_RAID_LEADER || channel == CHAT_MSG_PARTY_LEADER;
    if (!privateGroup && sWorld->getBoolConfig(CONFIG_CHAT_MUTE_FIRST_LOGIN) &&
        !bot->GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_CHAT_CHANNEL_REQ) &&
        bot->GetTotalPlayedTime() < sWorld->getIntConfig(CONFIG_CHAT_TIME_MUTE_FIRST_LOGIN) * MINUTE)
        return false;
    if ((channel == CHAT_MSG_SAY || channel == CHAT_MSG_YELL) &&
        (!bot->IsAlive() || bot->IsSpectator() || bot->GetLevel() < sWorld->getIntConfig(CONFIG_CHAT_SAY_LEVEL_REQ)))
        return false;
    switch (channel)
    {
        case CHAT_MSG_WHISPER:
            return (bot->GetLevel() >= sWorld->getIntConfig(CONFIG_CHAT_WHISPER_LEVEL_REQ) || sender->IsGameMaster()) &&
                   (sender->isAcceptWhispers() ||
                    !sender->GetSession()->HasPermission(rbac::RBAC_PERM_CAN_FILTER_WHISPERS) ||
                    sender->IsInWhisperWhiteList(bot->GetGUID()));
        case CHAT_MSG_GUILD:
            if (auto* guild = bot->GetGuild(); guild && guild->GetId() == sender->GetGuildId())
                return guild->HasRankRight(bot, GR_RIGHT_GCHATSPEAK) &&
                       guild->HasRankRight(bot, GR_RIGHT_GCHATLISTEN) &&
                       guild->HasRankRight(sender, GR_RIGHT_GCHATLISTEN);
            return false;
        case CHAT_MSG_PARTY:
            return bot->GetGroup() && bot->GetGroup() == sender->GetGroup() &&
                   bot->GetGroup()->GetMemberGroup(bot->GetGUID()) ==
                       bot->GetGroup()->GetMemberGroup(sender->GetGUID());
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_PARTY_LEADER:
            return bot->GetGroup() && bot->GetGroup() == sender->GetGroup();
        case CHAT_MSG_SAY:
        case CHAT_MSG_YELL:
            return bot->GetMap() == sender->GetMap() && bot->InSamePhase(sender) &&
                   bot->IsWithinDistInMap(sender, channel == CHAT_MSG_SAY ? 40.0f : 300.0f);
        case CHAT_MSG_CHANNEL:
            return true;
        default:
            return false;
    }
}
}  // namespace

struct Manager::SocialPersistence
{
    struct ProfileWrite
    {
        BotId Id = 0;
        std::uint32_t Seed = 0;
        std::uint64_t Revision = 0;
        bool OriginPending = false;
        std::string State;
        std::string Cognition;
        std::unordered_map<BotId, Relationship> Contacts;
    };
    std::mutex Mutex;
    std::deque<std::pair<BotId, BotId>> Requests;
    std::list<QueryCallback> Queries;
    std::list<TransactionCallback> Transactions;
    std::deque<ProfileWrite> Profiles;
    std::atomic<std::size_t> PendingProfiles{0};
};

Manager::Manager() : _socialPersistence(std::make_unique<SocialPersistence>()) {}
Manager::~Manager() = default;

void BotState::RetainActivityBefore(Goal next)
{
    constexpr std::array activities = {Goal::Quest, Goal::Gather, Goal::Craft, Goal::Auction, Goal::Dungeon,
                                       Goal::Raid, Goal::Battleground, Goal::Arena, Goal::Socialize, Goal::Travel};
    if (next == Goal::None || next == LifeGoal.Kind ||
        (LifeGoal.State != Outcome::Proposed && LifeGoal.State != Outcome::Started && LifeGoal.State != Outcome::Blocked) ||
        std::find(activities.begin(), activities.end(), LifeGoal.Kind) == activities.end())
        return;
    std::erase_if(InterruptedGoals, [&](auto const& retained) { return retained.Kind == LifeGoal.Kind; });
    if (InterruptedGoals.size() >= MAX_INTERRUPTED_GOALS)
        throw std::runtime_error("ADVAI interrupted goal capacity exceeded; refusing to discard a commitment");
    InterruptedGoals.push_back(LifeGoal);
    Dirty = true;
}

void BotState::TrimContacts()
{
    while (Relationships.size() > MAX_RESIDENT_CONTACTS)
    {
        auto oldest = std::min_element(Relationships.begin(), Relationships.end(),
                                       [](auto const& left, auto const& right)
                                       {
                                           auto rank = [](auto const& contact)
                                           {
                                               return std::tuple(std::abs(contact.second.Affinity) >= 20,
                                                                 contact.second.LastEvent, contact.first);
                                           };
                                           return rank(left) < rank(right);
                                       });
        Relationships.erase(oldest);
    }
}

bool BotState::EnsureContact(BotId peer)
{
    if (!peer)
        return true;
    if (Relationships.contains(peer))
        return true;
    if (auto dirty = DirtyRelationships.find(peer); dirty != DirtyRelationships.end())
    {
        PutContact(peer, dirty->second);
        return true;
    }
    if (Now() >= ContactRetryAfter && !LoadingContacts.contains(peer) && LoadingContacts.size() < 16 &&
        Manager::Instance().RequestContact(Snapshot.Id, peer))
        LoadingContacts.insert(peer);
    return false;
}

void BotState::PutContact(BotId peer, Relationship const& relationship)
{
    if (!Relationships.contains(peer) && Relationships.size() >= MAX_RESIDENT_CONTACTS)
    {
        auto oldest = std::min_element(
            Relationships.begin(), Relationships.end(),
            [](auto const& left, auto const& right)
            {
                return std::tuple(std::abs(left.second.Affinity) >= 20, left.second.LastEvent, left.first) <
                       std::tuple(std::abs(right.second.Affinity) >= 20, right.second.LastEvent, right.first);
            });
        Relationships.erase(oldest);
    }
    Relationships[peer] = relationship;
}

bool Manager::RequestContact(BotId id, BotId peer)
{
    std::lock_guard lock(_socialPersistence->Mutex);
    if (!id || !peer || _socialPersistence->Requests.size() >= 4096)
    {
        LOG_ERROR("playerbots", "ADVAI cannot queue relationship load for {} and {}", id, peer);
        ++_blocked;
        return false;
    }
    _socialPersistence->Requests.emplace_back(id, peer);
    return true;
}

void Manager::ProcessSocialPersistence()
{
    std::list<QueryCallback> queries;
    std::list<TransactionCallback> transactions;
    std::vector<std::pair<BotId, BotId>> requests;
    std::vector<SocialPersistence::ProfileWrite> profiles;
    {
        std::lock_guard lock(_socialPersistence->Mutex);
        queries.splice(queries.end(), _socialPersistence->Queries);
        transactions.splice(transactions.end(), _socialPersistence->Transactions);
        while (requests.size() < 256 && queries.size() < 32 && !_socialPersistence->Requests.empty())
        {
            requests.push_back(_socialPersistence->Requests.front());
            _socialPersistence->Requests.pop_front();
        }
        while (profiles.size() < 16 && !_socialPersistence->Profiles.empty())
        {
            profiles.push_back(std::move(_socialPersistence->Profiles.front()));
            _socialPersistence->Profiles.pop_front();
        }
    }
    for (std::size_t offset = 0; offset < requests.size(); offset += 64)
    {
        auto end = std::min(requests.size(), offset + 64);
        std::vector<std::pair<BotId, BotId>> batch(requests.begin() + offset, requests.begin() + end);
        auto* statement = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_ADVAI_CONTACT_BATCH);
        for (std::size_t index = 0; index < 64; ++index)
        {
            statement->SetData(static_cast<std::uint8_t>(index * 2), index < batch.size() ? batch[index].first : 0U);
            statement->SetData(static_cast<std::uint8_t>(index * 2 + 1), index < batch.size() ? batch[index].second : 0U);
        }
        queries.push_back(PlayerbotsDatabase.AsyncQuery(statement).WithPreparedCallback(
            [this, batch = std::move(batch)](PreparedQueryResult result)
            {
                if (!result)
                {
                    for (auto const& [id, peer] : batch)
                        if (auto state = Get(id))
                        {
                            std::lock_guard lock(state->Mutex);
                            state->LoadingContacts.erase(peer);
                            state->ContactRetryAfter = Now() + 30;
                        }
                    LOG_ERROR("playerbots", "ADVAI relationship batch failed; {} observations remain deferred", batch.size());
                    ++_blocked;
                    return;
                }
                std::unordered_map<std::uint64_t, Relationship> loaded;
                do
                {
                    Field* fields = result->Fetch();
                    auto id = fields[0].Get<BotId>();
                    auto peer = fields[1].Get<BotId>();
                    if (!id || !peer)
                        continue;
                    Relationship relationship;
                    if (fields[2].Get<bool>())
                    {
                        relationship.Affinity = fields[3].Get<int16>();
                        relationship.Encounters = fields[4].Get<uint32>();
                        relationship.LastEvent = fields[5].Get<uint64>();
                        relationship.LastReward = fields[6].Get<uint64>();
                        relationship.Revision = fields[7].Get<uint64>();
                        if (relationship.Affinity < -100 || relationship.Affinity > 100)
                            throw std::runtime_error("ADVAI invalid durable relationship affinity");
                    }
                    loaded.emplace((static_cast<std::uint64_t>(id) << 32) | peer, relationship);
                } while (result->NextRow());
                for (auto const& [id, peer] : batch)
                    if (!loaded.contains((static_cast<std::uint64_t>(id) << 32) | peer))
                        throw std::runtime_error("ADVAI relationship batch omitted a requested contact");
                for (auto const& [id, peer] : batch)
                    if (auto state = Get(id))
                    {
                        std::lock_guard lock(state->Mutex);
                        state->LoadingContacts.erase(peer);
                        auto relationship = loaded.at((static_cast<std::uint64_t>(id) << 32) | peer);
                        if (auto dirty = state->DirtyRelationships.find(peer); dirty != state->DirtyRelationships.end())
                            relationship = dirty->second;
                        state->PutContact(peer, relationship);
                        state->Dirty = true;
                    }
            }));
    }
    if (!profiles.empty())
    {
        auto transaction = PlayerbotsDatabase.BeginTransaction();
        for (auto const& write : profiles)
        {
            auto* state = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPSERT_ADVAI_STATE);
            state->SetData(0, write.Id);
            state->SetData(1, write.Seed);
            state->SetData(2, write.State);
            state->SetData(3, write.Revision);
            transaction->Append(state);
            auto* cognition = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPSERT_ADVAI_COGNITION);
            cognition->SetData(0, write.Id);
            cognition->SetData(1, write.Cognition);
            cognition->SetData(2, write.Revision);
            transaction->Append(cognition);
            for (auto const& [peer, contact] : write.Contacts)
            {
                auto* update = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_UPSERT_ADVAI_CONTACT);
                update->SetData(0, write.Id);
                update->SetData(1, peer);
                update->SetData(2, contact.Affinity);
                update->SetData(3, contact.Encounters);
                update->SetData(4, contact.LastEvent);
                update->SetData(5, contact.LastReward);
                update->SetData(6, contact.Revision);
                transaction->Append(update);
            }
        }
        auto callback = PlayerbotsDatabase.AsyncCommitTransaction(transaction);
        callback.AfterComplete(
            [this, profiles = std::move(profiles)](bool success)
            {
                _socialPersistence->PendingProfiles.fetch_sub(profiles.size());
                for (auto const& write : profiles)
                {
                    auto current = Get(write.Id);
                    if (!current)
                    {
                        // Runs inside a DB completion callback; an exception here would escape the world update.
                        LOG_ERROR("playerbots", "ADVAI lost a profile with an unacknowledged durable save: {}", write.Id);
                        ++_blocked;
                        continue;
                    }
                    std::lock_guard lock(current->Mutex);
                    --current->PendingSaves;
                    if (!success)
                    {
                        LOG_ERROR("playerbots", "ADVAI profile/relationship batch failed for {}", write.Id);
                        current->Dirty = true;
                        if (!current->Snapshot.Now)
                            _identityWrites.push_back(write.Id);
                        ++_blocked;
                        continue;
                    }
                    if (write.OriginPending && current->InitialSetupPending)
                        current->InitialOriginDurable = true;
                    for (auto const& [peer, saved] : write.Contacts)
                        if (auto dirty = current->DirtyRelationships.find(peer);
                            dirty != current->DirtyRelationships.end() && dirty->second.Revision == saved.Revision)
                            current->DirtyRelationships.erase(dirty);
                    if (current->Dirty)
                        Save(write.Id, *current);
                }
            });
        transactions.push_back(std::move(callback));
    }
    for (auto it = queries.begin(); it != queries.end();)
        if (it->InvokeIfReady())
            it = queries.erase(it);
        else
            ++it;
    for (auto it = transactions.begin(); it != transactions.end();)
        if (it->InvokeIfReady())
            it = transactions.erase(it);
        else
            ++it;
    {
        std::lock_guard lock(_socialPersistence->Mutex);
        _socialPersistence->Queries.splice(_socialPersistence->Queries.end(), queries);
        _socialPersistence->Transactions.splice(_socialPersistence->Transactions.end(), transactions);
    }
}

bool BotState::ApplyRelationship(BotSocialEvent const& event)
{
    if (!event.Other)
        return true;
    if (DirtyRelationships.size() >= MAX_DIRTY_CONTACTS && !DirtyRelationships.contains(event.Other))
        return false;
    if (!EnsureContact(event.Other))
        return false;
    auto& relationship = Relationships.at(event.Other);
    auto previous = relationship.LastReward;
    relationship.Observe(event);
    DirtyRelationships[event.Other] = relationship;
    if (Snapshot.PlayerGuild && event.Kind == SocialEventKind::GuildParticipation &&
        relationship.LastReward != previous)
        Life.GuildSatisfaction =
            static_cast<std::int16_t>(std::clamp<int>(Life.GuildSatisfaction + (event.Positive ? 3 : -5), -100, 100));
    Dirty = true;
    return true;
}

void BotState::ObserveExperience(BotSocialEvent const& event)
{
    if (event.Other && PendingRelationshipEvents.size() >= MAX_PENDING_RELATIONSHIP_EVENTS &&
        ((!Relationships.contains(event.Other) && !DirtyRelationships.contains(event.Other)) ||
         (DirtyRelationships.size() >= MAX_DIRTY_CONTACTS && !DirtyRelationships.contains(event.Other))))
        throw std::runtime_error("ADVAI relationship persistence backlog exhausted for bot " +
                                 std::to_string(Snapshot.Id) + "; social observations cannot be discarded");
    if (!Cognition.Observe(event, Persona))
        return;
    if (!ApplyRelationship(event))
    {
        PendingRelationshipEvents.push_back(event);
    }
    switch (event.Kind)
    {
        case SocialEventKind::Death:
            if (!event.Other)
                QueueReaction(Intent::EventDeath, event);
            break;
        case SocialEventKind::Wipe:
            QueueReaction(Intent::EventWipe, event);
            break;
        case SocialEventKind::Victory:
            QueueReaction(Intent::EventVictory, event);
            PartyWipes = 0;
            break;
        case SocialEventKind::SharedQuest:
            QueueReaction(Intent::EventQuest, event);
            break;
        case SocialEventKind::Trade:
        case SocialEventKind::Assistance:
        case SocialEventKind::GuildParticipation:
            if (event.Positive && event.Other)
                QueueReaction(Intent::Thanks, event);
            break;
        default:
            break;
    }
    Dirty = true;
}

void BotState::QueueReaction(Intent topic, BotSocialEvent const& event)
{
    if (std::any_of(Reactions.begin(), Reactions.end(), [&](auto const& reaction)
                    { return reaction.Topic == topic && reaction.Event.Entity == event.Entity; }))
        return;
    if (Reactions.size() == 8)
        Reactions.pop_front();
    Reactions.push_back({topic, event});
}

Manager& Manager::Instance()
{
    static Manager manager;
    return manager;
}

bool Manager::Applies(Player const* player) const
{
    if (!sPlayerbotAIConfig.AdvaiEnabled || !player || !player->GetSession() || !player->GetSession()->IsHeadless())
        return false;
    auto accounts = _accounts.load();
    return accounts && accounts->count(player->GetSession()->GetAccountId());
}

std::shared_ptr<BotState> Manager::Get(BotId id) const
{
    {
        // Common case: a resident bot. Readers share the lock across map threads.
        std::shared_lock lock(_statesMutex);
        if (auto found = _states.find(id); found != _states.end())
            return found->second;
    }
    // Hydration mutates the maps, so take the exclusive lock and re-check for a concurrent hydration.
    std::lock_guard lock(_statesMutex);
    auto found = _states.find(id);
    if (found != _states.end())
        return found->second;
    auto archived = _archived.find(id);
    if (archived == _archived.end())
        return nullptr;
    // Hydration uses the last acknowledged profile, without a database read on a map thread.
    auto state = std::make_shared<BotState>();
    state->Snapshot.Id = id;
    state->Persona = BotPersona::Create(archived->second.Seed);
    StateCodec::Deserialize(archived->second.State, *state, Now());
    if (!archived->second.Cognition.empty())
        state->Cognition = BotCognition::Deserialize(archived->second.Cognition);
    state->PersistenceRevision = archived->second.Revision;
    state->SavedAt = archived->second.SavedAt;
    _states.emplace(id, state);
    _archiveCandidates.push_back(id);
    _archived.erase(archived);
    return state;
}

std::shared_ptr<BotState> Manager::Ensure(BotId id)
{
    for (;;)
    {
        if (auto existing = Get(id))
            return existing;
        std::lock_guard lock(_statesMutex);
        if (_archived.contains(id))
            continue;
        auto& state = _states[id];
        if (!state)
        {
            state = std::make_shared<BotState>();
            state->Persona = BotPersona::ForCharacter(id);
            state->Snapshot.Id = id;
            state->Dirty = true;
            _archiveCandidates.push_back(id);
        }
        return state;
    }
}

void Manager::ArchiveOfflineProfiles(std::size_t limit)
{
    std::lock_guard lock(_statesMutex);
    auto count = std::min<std::size_t>(limit, _archiveCandidates.size());
    auto now = Now();
    for (std::size_t index = 0; index < count; ++index)
    {
        auto id = _archiveCandidates.front();
        _archiveCandidates.pop_front();
        auto found = _states.find(id);
        if (found == _states.end())
            continue;
        if (found->second.use_count() != 1 || sRandomPlayerbotMgr.GetPlayerBot(id))
        {
            _archiveCandidates.push_back(id);
            continue;
        }
        auto& state = *found->second;
        std::unique_lock stateLock(state.Mutex, std::try_to_lock);
        if (!stateLock || state.Dirty || state.PendingSaves || state.InitialSetupStarted ||
            state.InitialCharacterSavePending || state.InitialCommitRequested || !state.LoadingContacts.empty() ||
            !state.Messages.empty() || !state.Requests.empty() || !state.DeferredMessages.empty() ||
            state.LegacyRelationships || now < state.SavedAt + 60)
        {
            _archiveCandidates.push_back(id);
            continue;
        }
        ArchivedProfile profile;
        profile.Life = state.Life;
        profile.Seed = state.Persona.Seed;
        profile.Revision = state.PersistenceRevision;
        profile.SavedAt = state.SavedAt;
        profile.State = StateCodec::Serialize(state);
        profile.Cognition = state.Cognition.Serialize();
        _archived.emplace(id, std::move(profile));
        stateLock.unlock();
        _states.erase(found);
    }
}

void Manager::Initialize()
{
    if (_initialized)
        return;
    _accountCount = sPlayerbotAIConfig.RandomBotAccounts.size();
    _accounts.store(std::make_shared<std::unordered_set<std::uint32_t> const>(
        sPlayerbotAIConfig.RandomBotAccounts.begin(), sPlayerbotAIConfig.RandomBotAccounts.end()));
    PlayerbotTextMgr::instance().ValidateAdvaiTexts();
    InitializeProfessionTools();
    InitializeTravelNetwork();
    InitializeMarket(Now());
    if (sPlayerbotAIConfig.AdvaiLlmEnabled)
        InitializeLlm();
    auto entrances = WorldDatabase.Query("SELECT ID FROM areatrigger_teleport");
    if (!entrances)
        throw std::runtime_error("ADVAI cannot load the native instance entrance index");
    do
    {
        auto id = entrances->Fetch()[0].Get<std::uint32_t>();
        auto* portal = sObjectMgr->GetAreaTriggerTeleport(id);
        auto* trigger = sObjectMgr->GetAreaTrigger(id);
        auto* map = portal ? sMapStore.LookupEntry(portal->target_mapId) : nullptr;
        if (trigger && map && map->IsRaid())
            _raidEntrances[map->MapID].push_back({id, trigger->map, 0, 0, trigger->x, trigger->y, trigger->z});
    } while (entrances->NextRow());
    for (auto const& [id, data] : sObjectMgr->GetAllCreatureData())
        if (auto* creature = sObjectMgr->GetCreatureTemplate(data.id))
            if (creature->npcflag & (UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_REPAIR | UNIT_NPC_FLAG_TRAINER |
                                     UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_AUCTIONEER | UNIT_NPC_FLAG_INNKEEPER |
                                     UNIT_NPC_FLAG_PETITIONER))
                _services.push_back(
                    {data.id, data.mapid, creature->npcflag, creature->faction, data.posX, data.posY, data.posZ});
    std::unordered_set<std::uint32_t> trainers;
    for (auto const& service : _services)
        if (service.Flags & UNIT_NPC_FLAG_TRAINER)
            if (sObjectMgr->GetTrainer(service.Entry))
                trainers.insert(service.Entry);
    _trainers.assign(trainers.begin(), trainers.end());
    for (auto const& [id, data] : sObjectMgr->GetAllGOData())
        if (auto* object = sObjectMgr->GetGameObjectTemplate(data.id);
            object && object->type == GAMEOBJECT_TYPE_MAILBOX)
            _services.push_back({data.id, data.mapid, SERVICE_MAILBOX, 0, data.posX, data.posY, data.posZ});
    std::unordered_set<BotId> quarantined;
    std::vector<BotId> legacy;
    if (PreparedQueryResult rows =
            PlayerbotsDatabase.Query(PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_ADVAI_STATE)))
        do
        {
            Field* fields = rows->Fetch();
            // RAM: a resident BotState is ~90 KB while its serialized profile is ~330 bytes, and the realm has
            // ~32,000 stored profiles. Validate each one through a short-lived object and keep only the compact
            // archived form; a profile is hydrated on demand when its character is actually used.
            auto statePtr = std::make_unique<BotState>();
            BotState* state = statePtr.get();
            state->Snapshot.Id = fields[0].Get<BotId>();
            state->Persona = BotPersona::Create(fields[1].Get<std::uint32_t>());
            std::string const stored = fields[2].Get<std::string>();
            try
            {
                StateCodec::Deserialize(stored, *state, Now());
            }
            catch (std::exception const& error)
            {
                // One corrupt profile must not stop the realm from booting: quarantine it (the character restarts
                // with a fresh personality state) and say which one.
                LOG_ERROR("playerbots", "ADVAI quarantined unreadable profile for character {}: {}", fields[0].Get<BotId>(),
                          error.what());
                quarantined.insert(fields[0].Get<BotId>());
                continue;
            }
            state->PersistenceRevision = fields[3].Get<std::uint64_t>();
            ArchivedProfile profile;
            profile.Life = state->Life;
            profile.Seed = state->Persona.Seed;
            profile.Revision = state->PersistenceRevision;
            profile.SavedAt = 0;
            profile.State = stored;
            if (state->LegacyRelationships)
                legacy.push_back(fields[0].Get<BotId>());
            std::lock_guard lock(_statesMutex);
            _archived.emplace(fields[0].Get<BotId>(), std::move(profile));
        } while (rows->NextRow());
    if (PreparedQueryResult rows =
            PlayerbotsDatabase.Query(PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_ADVAI_COGNITION)))
        do
        {
            Field* fields = rows->Fetch();
            if (quarantined.count(fields[0].Get<BotId>()))
                continue;
            std::lock_guard lock(_statesMutex);
            auto archived = _archived.find(fields[0].Get<BotId>());
            if (archived == _archived.end())
                throw std::runtime_error("ADVAI cognition has no persistent identity");
            if (archived->second.Revision != fields[2].Get<std::uint64_t>())
                throw std::runtime_error("ADVAI state and cognition revisions disagree");
            std::string cognition = fields[1].Get<std::string>();
            (void)BotCognition::Deserialize(cognition);  // validate now; keep the compact text
            archived->second.Cognition = std::move(cognition);
        } while (rows->NextRow());
    // Convert old embedded histories into durable indexed rows together with the versioned profile.
    for (BotId id : legacy)
        if (auto state = Get(id); state && state->LegacyRelationships)
            Save(id, *state);
    _active.store(std::make_shared<std::unordered_set<BotId> const>());
    _initialized = true;
    LOG_INFO("playerbots", "ADVAI loaded persistent personalities and validated conversation catalogs");
}

bool Manager::Active(BotId id) const
{
    auto active = _active.load();
    return active && active->count(id);
}

void Manager::RegisterGeneratedCharacters(std::uint32_t accountOnly)
{
    std::size_t hydrated = 0;
    if (!_initialized)
        throw std::runtime_error("ADVAI identities cannot be registered before persistent state is loaded");
    if (!CharacterDatabase.Query("SELECT COUNT(*) FROM playerbots_advai_initialization"))
        throw std::runtime_error("ADVAI initialization journal is unavailable; apply the characters migration");
    for (auto account : sPlayerbotAIConfig.RandomBotAccounts)
    {
        if (accountOnly && account != accountOnly)
            continue;
        // Startup runs before map threads. Use stored play time, before login can advance it.
        if (QueryResult characters = CharacterDatabase.Query(
                "SELECT c.guid,c.totaltime,c.level,c.class,c.xp,r.guid IS NOT "
                "NULL,r.seed,r.completed_at,r.level,r.profile "
                "FROM characters c LEFT JOIN playerbots_advai_initialization r ON r.guid=c.guid "
                "WHERE c.account={} AND c.deleteDate IS NULL",
                account))
            do
            {
                Field* fields = characters->Fetch();
                BotId id = fields[0].Get<BotId>();
                auto state = Ensure(id);
                if (fields[5].Get<bool>())
                {
                    if (fields[6].Get<uint32>() != state->Persona.Seed)
                        throw std::runtime_error("ADVAI character initialization receipt has a different identity");
                    auto profile = fields[9].Get<uint8>();
                    if (profile >= static_cast<unsigned>(ClassProfile::Count) ||
                        (state->Life.Initialized && state->Profile != static_cast<ClassProfile>(profile)))
                        throw std::runtime_error(
                            "ADVAI character initialization receipt has a different class profile");
                    state->Profile = static_cast<ClassProfile>(profile);
                    if (!state->Life.Initialized)
                    {
                        state->Life.Initialized = true;
                        state->InitialSetupPending = false;
                        state->Life.Start(fields[7].Get<uint64>(), false, state->Persona.Seed);
                        state->Snapshot.Level = fields[8].Get<uint8>();
                        state->Dirty = true;
                    }
                }
                else if (!state->Life.Initialized && !state->InitialSetupPending)
                {
                    uint8 startingLevel = fields[3].Get<uint8>() == CLASS_DEATH_KNIGHT
                                              ? sWorld->getIntConfig(CONFIG_START_HEROIC_PLAYER_LEVEL)
                                              : sWorld->getIntConfig(CONFIG_START_PLAYER_LEVEL);
                    if (!fields[1].Get<uint32>() && !fields[4].Get<uint32>() && fields[2].Get<uint8>() == startingLevel)
                    {
                        state->InitialSetupPending = true;
                        state->Dirty = true;
                    }
                }
                if (state->InitialSetupPending && !state->InitialTargetLevel)
                {
                    uint32 minimum = sPlayerbotAIConfig.RandomBotMinLevel;
                    uint32 maximum =
                        std::min(sPlayerbotAIConfig.RandomBotMaxLevel, sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
                    if (fields[3].Get<uint8>() == CLASS_DEATH_KNIGHT)
                    {
                        minimum = std::max(minimum, sWorld->getIntConfig(CONFIG_START_HEROIC_PLAYER_LEVEL));
                        maximum = std::max(maximum, minimum);
                    }
                    if (!minimum || minimum > maximum || maximum > 255)
                        throw std::runtime_error("ADVAI invalid initial level bounds");
                    auto seed = state->Persona.Seed;
                    auto roll = seed % 10000;
                    auto maxChance = static_cast<uint32>(sPlayerbotAIConfig.RandomBotMaxLevelChance * 10000);
                    auto minChance = static_cast<uint32>(sPlayerbotAIConfig.RandomBotMinLevelChance * 10000);
                    state->InitialTargetLevel = static_cast<uint8>(
                        roll < maxChance               ? maximum
                        : roll < maxChance + minChance ? minimum
                                                       : minimum + (seed / 10000) % (maximum - minimum + 1));
                    state->Profile = Backport::Assign(fields[3].Get<uint8>());
                    state->Dirty = true;
                }
                if (state->Dirty)
                    _identityWrites.push_back(id);
                state.reset();
                // RAM: Ensure() hydrates a full profile for every character on the account; hand clean ones back to
                // the compact archive as we go instead of holding thousands at once.
                if (++hydrated % 128 == 0)
                    ArchiveOfflineProfiles();
            } while (characters->NextRow());
    }
    _pendingIdentities.store(_identityWrites.size());
}

bool Manager::Initialized(BotId id) const
{
    {
        std::shared_lock lock(_statesMutex);
        if (auto archived = _archived.find(id); archived != _archived.end())
            return archived->second.Life.Initialized;
    }
    auto state = Get(id);
    if (!state)
        return false;
    std::lock_guard lock(state->Mutex);
    return state->Life.Initialized;
}

bool Manager::Eligible(BotId id, Timestamp now) const
{
    if (!Metrics::MemoryAllowsLogin())
        return false;
    {
        std::shared_lock lock(_statesMutex);
        if (auto archived = _archived.find(id); archived != _archived.end())
            return archived->second.Life.Eligible(now);
    }
    auto state = Get(id);
    if (!state)
        return true;
    std::lock_guard lock(state->Mutex);
    return state->Life.Eligible(now);
}

void Manager::MarkInitialized(Player* player)
{
    if (!Applies(player))
        return;
    auto state = Ensure(player->GetGUID().GetCounter());
    std::lock_guard lock(state->Mutex);
    if (!state->Life.Initialized && (!state->InitialSetupPending || state->InitialSetupStarted))
        state->InitialCommitRequested = true;
}

bool Manager::InitialSetupStarted(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return !state->Life.Initialized && state->InitialSetupPending && state->InitialSetupStarted;
    }
    return false;
}

std::uint8_t Manager::InitialLevel(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->InitialSetupPending && state->InitialOriginDurable && state->InitialTargetLevel)
            return state->InitialTargetLevel;
    }
    throw std::runtime_error("ADVAI initial level has no durable initialization plan");
}

void Manager::AdvanceInitialization(Player* bot)
{
    auto id = bot->GetGUID().GetCounter();
    auto state = Get(id);
    if (!state || !GET_PLAYERBOT_AI(bot) || !bot->GetSession() || bot->IsBeingTeleported())
        return;
    bool seed = false;
    {
        std::lock_guard lock(state->Mutex);
        if (state->Life.Initialized || state->InitialCharacterSavePending || Now() < state->InitialRetryAfter)
            return;
        if (state->InitialSetupPending && !state->InitialOriginDurable)
        {
            if (!state->PendingSaves)
                Save(id, *state);
            return;
        }
        if (state->InitialSetupPending && !state->InitialSetupStarted)
        {
            state->InitialSetupStarted = true;
            seed = true;
        }
        else if (!state->InitialCommitRequested)
            return;
        if (!seed)
            state->InitialCharacterSavePending = true;
    }
    if (seed)
    {
        sRandomPlayerbotMgr.RandomizeFirst(bot);
        return;
    }
    auto now = Now();
    auto transaction = CharacterDatabase.BeginTransaction();
    bot->SaveToDB(transaction, false, false);
    transaction->Append(
        "INSERT INTO playerbots_advai_initialization (guid,seed,completed_at,level,profile) VALUES ({},{},{},{},{}) "
        "ON DUPLICATE KEY UPDATE guid=guid",
        id, state->Persona.Seed, now, bot->GetLevel(), static_cast<unsigned>(state->Profile));
    bool playerGuild = GET_PLAYERBOT_AI(bot)->IsInRealGuild();
    uint8 seededLevel = bot->GetLevel();
    auto callback = CharacterDatabase.AsyncCommitTransaction(transaction);
    callback.AfterComplete(
        [this, id, now, playerGuild, seededLevel](bool success)
        {
            auto current = Get(id);
            if (!current)
                return;
            std::lock_guard lock(current->Mutex);
            current->InitialCharacterSavePending = false;
            if (!success)
            {
                current->InitialRetryAfter = Now() + 30;
                if (current->InitialSetupPending)
                {
                    current->InitialSetupStarted = false;
                    current->InitialCommitRequested = false;
                }
                LOG_ERROR("playerbots", "ADVAI initial character save/receipt failed for {}", id);
                ++_blocked;
                return;
            }
            current->InitialSetupPending = false;
            current->InitialSetupStarted = false;
            current->InitialCommitRequested = false;
            current->Life.Initialized = true;
            current->Life.Start(now, playerGuild, current->Persona.Seed);
            current->Snapshot.Level = seededLevel;
            current->Snapshot.Now = Now();
            current->NextSnapshot = 0;
            current->NextDecision = 0;
            current->Dirty = true;
            Save(id, *current);
        });
    std::lock_guard lock(_socialPersistence->Mutex);
    _socialPersistence->Transactions.push_back(std::move(callback));
}

BotActivitySnapshot Manager::Capture(PlayerbotAI* ai) const
{
    Player* bot = ai->GetBot();
    BotActivitySnapshot s;
    s.Now = Now();
    s.Id = bot->GetGUID().GetCounter();
    s.Map = bot->GetMapId();
    s.Zone = bot->GetZoneId();
    s.Level = bot->GetLevel();
    s.Name = bot->GetName();
    s.Alive = bot->IsAlive();
    s.InCombat = bot->IsInCombat();
    s.InInstance = !bot->GetMap()->IsBattlegroundOrArena() && bot->GetMap()->IsDungeon();
    s.InTransit = bot->IsInFlight() || bot->IsBeingTeleported() ||
                  (bot->GetTransport() && !bot->GetTransport()->IsStaticTransport());
    s.Trading = bot->GetTradeData() != nullptr;
    s.ActionInProgress = bot->IsNonMeleeSpellCast(false) || !bot->GetLootGUID().IsEmpty() ||
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == EFFECT_MOTION_TYPE;
    s.Moving = bot->isMoving();
    s.HumanControlled = ai->HasGameClientMaster();
    s.HumanGroup = HumanGroup(bot);
    s.PlayerGuild = ai->IsInRealGuild();
    s.Guild = bot->GetGuildId();
    if (auto* guild = sGuildMgr->GetGuildById(s.Guild))
        s.GuildName = guild->GetName();
    s.Group = bot->GetGroup() ? bot->GetGroup()->GetGUID().GetCounter() : 0;
    s.GroupLeader = bot->GetGroup() ? bot->GetGroup()->GetLeaderGUID().GetCounter() : 0;
    s.Money = bot->GetMoney();
    s.Health = static_cast<std::uint8_t>(bot->GetHealthPct());
    s.Mana = static_cast<std::uint8_t>(bot->GetPowerPct(POWER_MANA));
    s.FreeBagSlots = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, bot->GetFreeInventorySpace()));
    if (auto* area = sAreaTableStore.LookupEntry(s.Zone))
        s.ZoneName = area->area_name[TEXT_LOCALE];
    if (auto* entry = sChrClassesStore.LookupEntry(bot->getClass()))
        s.ClassName = entry->name[TEXT_LOCALE];
    if (auto profile = Profile(s.Id); profile != ClassProfile::Classic)
        s.ClassName = Backport::Name(profile);
    s.SpecName = SpecializationName(bot);
    s.AverageItemLevel = static_cast<std::uint16_t>(std::clamp(bot->GetAverageItemLevel(), 0.0f, 65535.0f));
    if (auto* weapon = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
        s.WeaponName = weapon->GetTemplate()->Name1;
    std::uint64_t durability = 0, maximum = 0;
    for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            durability += item->GetUInt32Value(ITEM_FIELD_DURABILITY);
            maximum += item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
        }
    s.Durability = static_cast<std::uint8_t>(maximum ? durability * 100 / maximum : 100);
    for (std::uint8_t slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (auto id = bot->GetQuestSlotQuestId(slot))
        {
            auto status = bot->GetQuestStatus(id);
            if (status == QUEST_STATUS_COMPLETE || !s.QuestId)
            {
                s.QuestId = id;
                s.HasQuest = true;
                s.QuestComplete = status == QUEST_STATUS_COMPLETE;
                if (auto* quest = sObjectMgr->GetQuestTemplate(id))
                    s.QuestName = quest->GetTitle();
            }
        }
    s.CanGather = bot->HasSkill(SKILL_MINING) || bot->HasSkill(SKILL_HERBALISM) || bot->HasSkill(SKILL_SKINNING);
    s.CanCraft = bot->HasSkill(SKILL_ALCHEMY) || bot->HasSkill(SKILL_BLACKSMITHING) || bot->HasSkill(SKILL_TAILORING) ||
                 bot->HasSkill(SKILL_LEATHERWORKING) || bot->HasSkill(SKILL_ENGINEERING) ||
                 bot->HasSkill(SKILL_JEWELCRAFTING) || bot->HasSkill(SKILL_ENCHANTING) ||
                 bot->HasSkill(SKILL_INSCRIPTION) || bot->HasSkill(SKILL_COOKING) || bot->HasSkill(SKILL_FIRST_AID);
    s.GroupReady = bot->GetGroup() && bot->GetGroup()->GetMembersCount() >= 5;
    s.InQueue = bot->InBattlegroundQueue();
    s.InBattleground = bot->InBattleground();
    s.InArena = bot->InArena();
    s.HasPet = bot->GetPet() != nullptr;
    auto* context = ai->GetAiObjectContext();
    if (auto state = Get(s.Id))
    {
        std::shared_ptr<SupplyPlan const> previous;
        bool refresh;
        {
            std::lock_guard lock(state->Mutex);
            previous = state->Supplies;
            refresh = !previous || s.Now >= state->NextSupplyCheck || state->Snapshot.Level != s.Level ||
                      state->KnownSpells != bot->GetSpellMap().size();
            if (refresh)
                state->NextSupplyCheck = s.Now + 60 + s.Id % 15;
        }
        auto supplies =
            std::make_shared<SupplyPlan>(CaptureSupplies(ai, refresh ? ClassReagents(bot) : previous->Reagents));
        s.NeedsFood = supplies->Food < 3;
        s.NeedsDrink = supplies->UsesMana && supplies->Drink < 3;
        s.UsesMana = supplies->UsesMana;
        s.NeedsAmmo = supplies->UsesAmmo && supplies->Ammo < 100;
        s.NeedsPetFood = supplies->HasHungryPet && supplies->PetFood < 3;
        s.NeedsReagent = supplies->NeedsReagents(bot);
        s.NeedsTool = !supplies->Tools.empty();
        s.HasBankSupplies = !supplies->BankSupply.IsEmpty();
        s.NeedsMount = supplies->NeedsMount;
        s.NeedsRecipe = supplies->RecipeItem != 0;
        std::lock_guard lock(state->Mutex);
        state->Supplies = std::move(supplies);
    }
    s.HasTradeStock =
        context->GetValue<std::uint32_t>("item count", "usage " + std::to_string(ITEM_USAGE_AH))->Get() > 0;
    s.HasAuctionStock = s.HasTradeStock;
    s.HasVendorStock =
        context->GetValue<std::uint32_t>("item count", "usage " + std::to_string(ITEM_USAGE_VENDOR))->Get() > 0;
    s.HasMail = bot->unReadMails > 0;
    s.AwaitingDelivery = Delivery(s.Id).has_value();
    for (Mail const* mail : bot->GetMails())
        if (mail && mail->state != MAIL_STATE_DELETED && mail->deliver_time <= time(nullptr) && !mail->COD &&
            (mail->money || !mail->items.empty()))
            s.HasMail = true;
    s.BankNeeded = s.FreeBagSlots < 4 && s.HasTradeStock;
    std::uint32_t known = static_cast<std::uint32_t>(bot->GetSpellMap().size());
    std::uint64_t skills = 0;
    constexpr std::array skillIds = {SKILL_ALCHEMY,   SKILL_BLACKSMITHING, SKILL_ENCHANTING,    SKILL_ENGINEERING,
                                     SKILL_HERBALISM, SKILL_INSCRIPTION,   SKILL_JEWELCRAFTING, SKILL_LEATHERWORKING,
                                     SKILL_MINING,    SKILL_SKINNING,      SKILL_TAILORING,     SKILL_COOKING,
                                     SKILL_FIRST_AID, SKILL_FISHING,       SKILL_RIDING};
    std::uint16_t gatheringSkill = 0, craftingSkill = 0;
    for (auto skill : skillIds)
    {
        auto value = bot->GetBaseSkillValue(skill);
        skills = (skills ^ value) * 1099511628211ULL;
        if (skill == SKILL_MINING || skill == SKILL_HERBALISM || skill == SKILL_SKINNING)
            gatheringSkill = std::max(gatheringSkill, value);
        else if (skill != SKILL_RIDING && skill != SKILL_COOKING && skill != SKILL_FIRST_AID && skill != SKILL_FISHING)
            craftingSkill = std::max(craftingSkill, value);
    }
    bool preferGathering = gatheringSkill && (Persona(s.Id).Preference == Goal::Gather || !craftingSkill);
    s.ProfessionSkill = preferGathering ? gatheringSkill : craftingSkill;
    if (s.ProfessionSkill)
        s.ProfessionActivity = preferGathering ? Goal::Gather : Goal::Craft;
    bool trainingChanged = true;
    if (auto state = Get(s.Id))
    {
        std::lock_guard lock(state->Mutex);
        trainingChanged = !state->Snapshot.Now || state->Snapshot.Level != s.Level || state->KnownSpells != known ||
                          state->SkillFingerprint != skills || s.Now >= state->NextTrainingCheck;
        s.InQueue = s.InQueue || state->Snapshot.InQueue;
        s.NeedsTraining = state->Snapshot.NeedsTraining;
        state->KnownSpells = known;
        state->SkillFingerprint = skills;
        if (trainingChanged)
            state->NextTrainingCheck = s.Now + 60 + s.Id % 15;
    }
    if (trainingChanged)
    {
        s.NeedsTraining = false;
        for (auto entry : _trainers)
        {
            auto* trainer = sObjectMgr->GetTrainer(entry);
            if (!trainer || !trainer->IsTrainerValidForPlayer(bot))
                continue;
            for (auto const& spell : trainer->GetSpells())
                if (spell.MoneyCost <= bot->GetMoney() && trainer->CanTeachSpell(bot, &spell) &&
                    PlayerbotFactory::IsTrainerSpellAllowedForBot(bot, trainer, &spell))
                {
                    s.NeedsTraining = true;
                    break;
                }
            if (s.NeedsTraining)
                break;
        }
    }
    return s;
}

void Manager::Save(BotId id, BotState& state)
{
    if (state.PendingSaves)
    {
        state.Dirty = true;
        return;
    }
    if (state.LegacyRelationships)
    {
        state.DirtyRelationships.insert(state.Relationships.begin(), state.Relationships.end());
        state.TrimContacts();
        state.LegacyRelationships = false;
    }
    if (state.PersistenceRevision == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("ADVAI persistent revision exhausted for character " + std::to_string(id));
    constexpr std::size_t MAX_PENDING_PROFILES = 64;
    auto pending = _socialPersistence->PendingProfiles.load();
    do
    {
        if (pending >= MAX_PENDING_PROFILES)
        {
            state.Dirty = true;
            return;
        }
    } while (!_socialPersistence->PendingProfiles.compare_exchange_weak(pending, pending + 1));
    SocialPersistence::ProfileWrite write;
    write.Id = id;
    write.Seed = state.Persona.Seed;
    write.Revision = ++state.PersistenceRevision;
    write.OriginPending = state.InitialSetupPending;
    write.State = StateCodec::Serialize(state);
    write.Cognition = state.Cognition.Serialize();
    write.Contacts = state.DirtyRelationships;
    ++state.PendingSaves;
    {
        std::lock_guard lock(_socialPersistence->Mutex);
        if (state.Snapshot.Now)
            _socialPersistence->Profiles.push_front(std::move(write));
        else
            _socialPersistence->Profiles.push_back(std::move(write));
    }
    state.SavedAt = Now();
    state.Dirty = false;
}

void Manager::OnLogout(Player* player)
{
    if (!Applies(player))
        return;
    if (auto state = Get(player->GetGUID().GetCounter()))
    {
        std::lock_guard lock(state->Mutex);
        state->Messages.clear();
        ++state->LlmRevision;
        state->LlmHistory.clear();
        Save(player->GetGUID().GetCounter(), *state);
    }
}

std::optional<AuctionDelivery> Manager::Delivery(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->Delivery;
    }
    return std::nullopt;
}

bool Manager::ExpectDelivery(BotId id, AuctionDelivery delivery)
{
    auto state = Get(id);
    if (!state || !delivery.ItemGuid || !delivery.Item || !delivery.Quantity || delivery.Deadline <= delivery.Ordered)
        return false;
    std::lock_guard lock(state->Mutex);
    if (state->Delivery)
        return false;
    state->Delivery = delivery;
    state->NextSnapshot = 0;
    state->NextDecision = 0;
    state->Dirty = true;
    Save(id, *state);
    return true;
}

void Manager::FinishDelivery(BotId id, std::uint32_t itemGuid, bool received)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (!state->Delivery || state->Delivery->ItemGuid != itemGuid)
            return;
        if (received)
            state->ObserveExperience({SocialEventKind::Trade, 0, Now(), true, state->Delivery->Item});
        state->Delivery.reset();
        state->NextSnapshot = 0;
        state->NextDecision = 0;
        state->Dirty = true;
        Save(id, *state);
    }
}

void Manager::Flush()
{
    std::vector<std::pair<BotId, std::shared_ptr<BotState>>> states;
    {
        std::shared_lock lock(_statesMutex);
        states.assign(_states.begin(), _states.end());
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    for (;;)
    {
        ProcessSocialPersistence();
        bool pending = false;
        for (auto const& [id, state] : states)
        {
            std::lock_guard stateLock(state->Mutex);
            if (state->Dirty && !state->PendingSaves && Now() > state->SavedAt)
                Save(id, *state);
            pending |= state->Dirty || state->PendingSaves || state->InitialCharacterSavePending;
        }
        if (!pending)
            return;
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("ADVAI shutdown persistence did not complete within 180 seconds");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Manager::UpdateWorld(std::uint32_t diff)
{
    if (!sPlayerbotAIConfig.AdvaiEnabled)
        return;
    Metrics::WorldTick(diff);
    _worldElapsed += diff;
    if (_worldElapsed < 500)
        return;
    _worldElapsed = 0;
    Metrics::SampleMemory();
    ProcessingSample processing(true);
    ProcessSocialPersistence();
    // Serialization runs under the exclusive states lock; keep each world pass short (startup uses the default).
    ArchiveOfflineProfiles(32);
    constexpr std::size_t IDENTITY_WRITES_PER_UPDATE = 16;
    for (std::size_t written = 0; written < IDENTITY_WRITES_PER_UPDATE && !_identityWrites.empty(); ++written)
    {
        auto id = _identityWrites.front();
        if (auto state = Get(id))
        {
            std::lock_guard lock(state->Mutex);
            if (!state->PendingSaves && state->Dirty)
                Save(id, *state);
            if (!state->PendingSaves && state->Dirty)
                break;
        }
        _identityWrites.pop_front();
    }
    _pendingIdentities.store(_identityWrites.size());
    if (_accountCount != sPlayerbotAIConfig.RandomBotAccounts.size())
    {
        _accountCount = sPlayerbotAIConfig.RandomBotAccounts.size();
        _accounts.store(std::make_shared<std::unordered_set<std::uint32_t> const>(
            sPlayerbotAIConfig.RandomBotAccounts.begin(), sPlayerbotAIConfig.RandomBotAccounts.end()));
    }
    Timestamp now = Now();
    if (auto account = RandomPlayerbotFactory::ProvisionAdvaiReplacement(
            Metrics::MemoryAllowsLogin() && _provisionRequested.exchange(false)))
    {
        sRandomPlayerbotMgr.RegisterProvisionedAccount(account);
        RegisterGeneratedCharacters(account);
        ++_provisionedCharacters;
    }
    std::unordered_set<std::uint64_t> zones;
    UpdateMarket(now);
    UpdateGuildTasks();
    if (sPlayerbotAIConfig.AdvaiLlmEnabled)
        ProcessLlm();
    for (Player* human : sRandomPlayerbotMgr.GetPlayers())
        if (human && human->IsInWorld() && human->GetSession() && !human->GetSession()->IsHeadless())
            zones.insert(ActivityRoster::ZoneKey(human->GetMapId(), human->GetZoneId()));
    std::vector<ActivityMember> members;
    std::vector<BotId> departures;
    std::vector<ObjectGuid> queueWithdrawals;
    std::vector<std::pair<BotId, BotId>> expiredOffers;
    std::optional<ObjectGuid> initialization;
    // Dirty profiles not saved this pass stay dirty and are picked up by the next pass.
    constexpr std::size_t MAX_SAVES_PER_PASS = 32;
    std::size_t savesThisPass = 0;
    for (auto it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* bot = it->second;
        if (!Applies(bot) || !bot->IsInWorld() || bot->IsBeingTeleported())
            continue;
        BotId id = bot->GetGUID().GetCounter();
        auto state = Ensure(id);
        std::lock_guard lock(state->Mutex);
        ExpireRequests(id, *state, now);
        if (now >= state->NextPetitionCheck)
        {
            state->NextPetitionCheck = now + 5 + id % 3;
            auto* charter = bot->GetItemByEntry(GUILD_CHARTER);
            auto* signatures = charter ? sPetitionMgr->GetSignature(charter->GetGUID()) : nullptr;
            state->PetitionSignatureCount = signatures
                ? static_cast<std::uint8_t>(std::min<std::size_t>(255, signatures->signatureMap.size())) : 0;
        }
        if (state->RaidRosterReady && state->RaidRosterSignature != RaidRosterSignature(bot->GetGroup()))
        {
            state->RaidRosterReady = false;
            state->RaidRosterSignature = 0;
        }
        if (!initialization && !state->Life.Initialized && !state->InitialCharacterSavePending &&
            now >= state->InitialRetryAfter &&
            (!state->InitialSetupPending || state->InitialOriginDurable || !state->PendingSaves))
            initialization = bot->GetGUID();
        auto lfgState = sLFGMgr->GetState(bot->GetGroup() ? bot->GetGroup()->GetGUID() : bot->GetGUID());
        bool lfgQueued = lfgState == lfg::LFG_STATE_QUEUED || lfgState == lfg::LFG_STATE_ROLECHECK ||
                         lfgState == lfg::LFG_STATE_PROPOSAL;
        bool readyCheck = lfgState == lfg::LFG_STATE_ROLECHECK || lfgState == lfg::LFG_STATE_PROPOSAL ||
                          lfgState == lfg::LFG_STATE_BOOT || lfgState == lfg::LFG_STATE_DUNGEON;
        for (uint32 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
            readyCheck |= bot->IsInvitedForBattlegroundQueueType(bot->GetBattlegroundQueueTypeId(slot));
        state->Snapshot.InQueue = bot->InBattlegroundQueue() || lfgQueued;
        auto const& s = state->Snapshot;
        if (state->Offer && now >= state->Offer->Expires)
            expiredOffers.emplace_back(id, state->Offer->Peer);
        auto* ai = GET_PLAYERBOT_AI(bot);
        bool committed =
            !bot->IsAlive() || s.RequiresContinuousActivity() || readyCheck || HumanGroup(bot) ||
            bot->IsNonMeleeSpellCast(false) || !bot->GetLootGUID().IsEmpty() ||
            bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == EFFECT_MOTION_TYPE ||
            bot->GetGroupInvite() || (ai && ai->HasGameClientMaster()) ||
            (bot->GetGroup() && state->LifeGoal.State == Outcome::Started && now < state->LifeGoal.CommittedUntil) ||
            (state->Offer && state->Offer->Agreed) || !state->Requests.empty() || bot->IsInCombat() ||
            bot->GetMap()->IsDungeon() || bot->GetMap()->IsBattlegroundOrArena() || bot->GetTransport();
        members.push_back({id, ActivityRoster::ZoneKey(bot->GetMapId(), bot->GetZoneId()),
                           bot->GetGroup() ? bot->GetGroup()->GetGUID().GetCounter() : 0, committed});
        bool sessionDue = state->Life.SessionDue && now >= state->Life.SessionDue;
        bool breakDue = state->Life.BreakDue && now >= state->Life.DepartureDue();
        if (!committed && s.InQueue && (sessionDue || breakDue) && now >= _queueWithdrawalRetry[id])
        {
            _queueWithdrawalRetry[id] = now + 10;
            queueWithdrawals.push_back(bot->GetGUID());
        }
        if (state->Dirty && savesThisPass < MAX_SAVES_PER_PASS &&
            (now >= state->SavedAt + sPlayerbotAIConfig.AdvaiSaveSeconds + id % 15 ||
             (!state->PendingSaves && state->DirtyRelationships.size() >= MAX_DIRTY_CONTACTS)))
        {
            ++savesThisPass;
            Save(id, *state);
        }
        if (s.Now && now < s.Now + 5 && now >= state->DepartureAfter && !committed &&
            (state->Life.CanDepart(s) || state->Life.CanPause(s)) && state->Messages.empty() &&
            !(state->Offer && state->Offer->Agreed))
        {
            if (state->Life.CanDepart(s))
                state->Life.BeginBreak(now, rand32());
            else
                state->Life.BeginPause(now, rand32());
            Save(id, *state);
            departures.push_back(id);
        }
    }
    auto roster = _activityRoster.Update(members, zones, sPlayerbotAIConfig.AdvaiBackgroundBudget,
                                         now / sPlayerbotAIConfig.AdvaiRotationSeconds,
                                         now % sPlayerbotAIConfig.AdvaiRotationSeconds);
    auto mandatory = ActivityRoster::Select(members, zones, 0, now / sPlayerbotAIConfig.AdvaiRotationSeconds);
    auto previous = _active.load();
    std::uint32_t changes = 0, humanZone = 0, background = 0;
    Timestamp longestWait = 0;
    std::unordered_set<BotId> eligible;
    for (auto const& member : members)
    {
        bool selected = roster.count(member.Id) != 0;
        if (!previous || selected != (previous->count(member.Id) != 0))
            ++changes;
        if (zones.count(member.Zone))
            ++humanZone;
        if (!mandatory.count(member.Id))
        {
            eligible.insert(member.Id);
            auto waiting = _backgroundWaiting.try_emplace(member.Id, now).first;
            if (selected)
            {
                ++background;
                waiting->second = now;
            }
            longestWait = std::max(longestWait, now >= waiting->second ? now - waiting->second : 0);
        }
    }
    std::erase_if(_backgroundWaiting, [&](auto const& entry) { return !eligible.count(entry.first); });
    Metrics::Population(static_cast<std::uint32_t>(members.size()), humanZone,
                        static_cast<std::uint32_t>(mandatory.size()), background, changes, longestWait);
    if (changes || !previous || previous->size() != roster.size())
        _active.store(std::make_shared<std::unordered_set<BotId> const>(std::move(roster)));
    for (auto const& member : members)
    {
        Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(member.Id));
        auto state = Get(member.Id);
        auto* ai = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        if (!state || !ai || !bot->IsInWorld() || bot->IsDuringRemoveFromWorld())
            continue;
        ResumeSocialWork(ai, state, now);
        ExecuteRequests(ai, state, now);
        ReactToEvents(ai, state, now);
        Deliver(ai, state, now);
        Ambient(ai, state, now);
    }
    if (initialization)
        if (Player* bot = ObjectAccessor::FindPlayer(*initialization))
            AdvanceInitialization(bot);
    static Timestamp nextReport = 0;
    if (now >= nextReport)
    {
        nextReport = now + 60;
        LOG_INFO("playerbots", "{}", Statistics());
    }
    for (auto const& [id, peer] : expiredOffers)
    {
        if (!PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<CancelTradeOperation>(ObjectGuid::Create<HighGuid::Player>(id), peer)))
            LOG_ERROR("playerbots", "ADVAI could not queue expired trade cancellation for {}", id);
        FinishTrade(id, false, "offer expired before completion");
    }
    for (ObjectGuid guid : queueWithdrawals)
        if (Player* bot = ObjectAccessor::FindPlayer(guid); bot && bot->GetSession())
        {
            sLFGMgr->LeaveAllLfgQueues(guid, true, bot->GetGroup() ? bot->GetGroup()->GetGUID() : ObjectGuid::Empty);
            for (uint32 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
            {
                auto queue = bot->GetBattlegroundQueueTypeId(slot);
                if (queue == BATTLEGROUND_QUEUE_NONE || bot->IsInvitedForBattlegroundQueueType(queue))
                    continue;
                auto* packet = new WorldPacket(CMSG_BATTLEFIELD_PORT, 9);
                *packet << uint8(BattlegroundMgr::BGArenaType(queue)) << uint8(0)
                        << uint32(BattlegroundMgr::BGTemplateId(queue)) << uint16(0x1F90) << uint8(0);
                bot->GetSession()->QueuePacket(packet);
            }
            if (auto state = Get(guid.GetCounter()))
            {
                std::lock_guard lock(state->Mutex);
                state->NextSnapshot = 0;
            }
        }
    std::unordered_set<BotId> online;
    for (auto const& member : members)
        online.insert(member.Id);
    std::erase_if(_queueWithdrawalRetry, [&](auto const& entry) { return !online.contains(entry.first); });
    for (BotId id : departures)
    {
        auto state = Get(id);
        bool leave = false;
        {
            std::lock_guard lock(state->Mutex);
            leave = state->Life.ReturnAt && state->Life.ShouldLeaveGuild();
        }
        if (Player* bot = sRandomPlayerbotMgr.GetPlayerBot(id))
        {
            if (leave)
                if (bot->GetGuildId() && bot->GetSession())
                {
                    auto guild = bot->GetGuildId();
                    WorldPackets::Guild::GuildLeave packet{WorldPacket(CMSG_GUILD_LEAVE)};
                    bot->GetSession()->HandleGuildLeaveOpcode(packet);
                    if (bot->GetGuildId() == guild)
                        LOG_WARN("playerbots", "ADVAI {} native guild departure rejected; retaining membership", id);
                }
            sRandomPlayerbotMgr.RetireAdvaiBot(id);
            ++_breaks;
        }
    }
    std::lock_guard chatLock(_chatMutex);
    std::erase_if(_claims, [&](auto const& entry) { return now > entry.second.second + CLAIM_LIFETIME; });
    std::erase_if(_threads, [&](auto const& entry) { return now > entry.second.Updated + MESSAGE_LIFETIME; });
    _utterances.Prune(now);
    std::erase_if(_channelBudgets, [&](auto const& entry) { return now > entry.second.first + 120; });
}

void Manager::Plan(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    std::lock_guard lock(state->Mutex);
    if (now < state->NextDecision || !Active(state->Snapshot.Id) || !state->Life.Initialized)
        return;
    state->NextDecision = now + 5 + state->Persona.Seed % 4;
    auto& goal = state->LifeGoal;
    std::uint32_t friends = 0;
    for (auto const& [id, relationship] : state->Relationships)
        if (id && relationship.Affinity >= 20 && relationship.Encounters >= 3)
            ++friends;
    state->Cognition.Mood.Settle(now);
    state->Cognition.Ambition.Update(state->Snapshot, state->Persona, friends);
    state->Dirty = true;
    Goal next = GoalPlanner::Choose(state->Snapshot, state->Persona, goal,
                                    state->Cognition.Ambition.Activity(state->Snapshot, state->Persona));
    if (state->PartyLeader && state->Snapshot.GroupLeader == state->PartyLeader && !state->Snapshot.HumanGroup &&
        !state->Snapshot.HumanControlled)
    {
        if (state->PartyHold && state->Snapshot.Alive && !state->Snapshot.InCombat &&
            (state->Snapshot.Health < 90 || (state->Snapshot.UsesMana && state->Snapshot.Mana < 70)))
            next = Goal::Rest;
        else if (next != Goal::Survive && next != Goal::Recover && next != Goal::Repair && next != Goal::Sell &&
                 next != Goal::Restock && next != Goal::Rest && next != Goal::Trade &&
                 now >= state->RetryAfter[static_cast<std::size_t>(state->PartyActivity)])
            next = state->PartyActivity;
    }
    if (next == Goal::Survive || next == Goal::Recover)
        return;
    auto obligation = [](Goal kind)
    {
        return kind == Goal::Repair || kind == Goal::Sell || kind == Goal::Restock || kind == Goal::Train ||
               kind == Goal::Rest || kind == Goal::Mail || kind == Goal::Bank || kind == Goal::Trade;
    };
    bool resume = false;
    bool acquiringEssentials = !state->Snapshot.Money &&
        (state->Snapshot.NeedsFood || state->Snapshot.NeedsDrink || state->Snapshot.NeedsAmmo ||
         state->Snapshot.NeedsReagent || state->Snapshot.NeedsPetFood || state->Snapshot.NeedsTool ||
         state->Snapshot.NeedsMount || state->Snapshot.Durability < 25);
    if (!obligation(next) && !acquiringEssentials && !state->Snapshot.HasCommitment() && !state->PartyLeader)
        for (auto it = state->InterruptedGoals.rbegin(); it != state->InterruptedGoals.rend(); ++it)
            if (now >= it->RetryAfter && now >= state->RetryAfter[static_cast<std::size_t>(it->Kind)])
            {
                auto restored = *it;
                state->InterruptedGoals.erase(std::next(it).base());
                restored.State = Outcome::Proposed;
                restored.CommittedUntil = std::max(restored.CommittedUntil, now + 60);
                restored.Reason = "resuming retained activity after immediate obligations";
                goal = std::move(restored);
                next = goal.Kind;
                resume = true;
                break;
            }
    if (next == Goal::None ||
        (!resume && next == goal.Kind && goal.State != Outcome::Succeeded && goal.State != Outcome::Cancelled))
        return;
    if (!resume && obligation(next))
        state->RetainActivityBefore(next);
    Timestamp commitment = 60 + state->Persona.Patience * 3;
    if (next == Goal::Battleground || next == Goal::Arena || next == Goal::Raid)
        commitment += state->Persona.Competitiveness * 2;
    if (!resume)
        goal = {next, now, now + commitment, 0, 0, 0, Outcome::Proposed, ""};
    bool retainedCraft = next == Goal::Craft || std::any_of(state->InterruptedGoals.begin(), state->InterruptedGoals.end(),
                                                          [](auto const& retained) { return retained.Kind == Goal::Craft; });
    if (!retainedCraft)
        state->CraftReservations.clear();
    goal.Failures = state->Failures[static_cast<std::size_t>(next)];
    state->Dirty = true;
    if (!state->Snapshot.HasCommitment() && ai->rpgInfo.GetStatus() != RPG_IDLE)
        ai->rpgInfo.ChangeToIdle();
    if (state->PartyLeader && state->Snapshot.GroupLeader == state->PartyLeader &&
        state->PartyLeader != state->Snapshot.Id && !state->Snapshot.HasCommitment())
    {
        bool maintenance = next == Goal::Repair || next == Goal::Sell || next == Goal::Restock || next == Goal::Train ||
                           next == Goal::Rest;
        ai->ChangeStrategy(maintenance ? "-follow" : "+follow", BOT_STATE_NON_COMBAT);
    }
}

void Manager::UpdateBot(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    if (!Applies(bot) || !bot->IsInWorld() || bot->IsDuringRemoveFromWorld())
        return;
    ProcessingSample processing(false);
    Timestamp now = Now();
    auto state = Ensure(bot->GetGUID().GetCounter());
    bool capture = false;
    bool suspend = false;
    bool updateParty = false;
    {
        std::lock_guard lock(state->Mutex);
        bool active = Active(bot->GetGUID().GetCounter());
        bool activated = active && !state->WasActive;
        suspend = state->WasActive && !active && !state->Snapshot.RequiresContinuousActivity();
        state->WasActive = active;
        capture = activated || now >= state->NextSnapshot;
        if (capture)
            state->NextSnapshot = now + (active ? 5 : 60 + bot->GetGUID().GetCounter() % 30);
        if (active && now >= state->NextPartyCheck && bot->GetGroup() &&
            bot->GetGroup()->GetLeaderGUID() == bot->GetGUID())
        {
            state->NextPartyCheck = now + 3;
            updateParty = true;
        }
    }
    if (suspend && !bot->IsInCombat() && !bot->IsInFlight() && !bot->GetTradeData() && !ai->HasGameClientMaster())
        bot->StopMoving();
    if (capture)
    {
        auto snapshot = Capture(ai);
        std::lock_guard lock(state->Mutex);
        if (state->PartyLeader && snapshot.GroupLeader != state->PartyLeader)
        {
            if (!snapshot.Group && state->Snapshot.Group && now < state->LifeGoal.CommittedUntil)
                state->ObserveExperience({SocialEventKind::BrokenCommitment, state->PartyLeader, now, false});
            state->PartyLeader = 0;
            state->PartyActivity = Goal::None;
            state->PartyHold = false;
            state->PartyReason.clear();
            state->NextDecision = 0;
        }
        if (state->LifeGoal.State == Outcome::Started)
        {
            auto kind = state->LifeGoal.Kind;
            bool complete =
                (kind == Goal::Repair && snapshot.Durability >= 95) ||
                (kind == Goal::Sell && snapshot.FreeBagSlots >= 4) ||
                (kind == Goal::Restock && !snapshot.NeedsFood && !snapshot.NeedsDrink && !snapshot.NeedsAmmo &&
                 !snapshot.NeedsReagent && !snapshot.NeedsPetFood && !snapshot.NeedsTool && !snapshot.NeedsMount &&
                 !snapshot.NeedsRecipe) ||
                (kind == Goal::Train && !snapshot.NeedsTraining) || (kind == Goal::Bank && !snapshot.BankNeeded) ||
                (kind == Goal::Rest && snapshot.Health >= 95 && (!snapshot.UsesMana || snapshot.Mana >= 85)) ||
                (kind == Goal::Mail && !snapshot.HasMail && !snapshot.AwaitingDelivery);
            if (complete)
            {
                GoalPlanner::ApplyResult(state->LifeGoal, {kind, Outcome::Succeeded, "completion observed in gameplay"},
                                         now);
                state->Failures[static_cast<std::size_t>(kind)] = 0;
                state->RetryAfter[static_cast<std::size_t>(kind)] = 0;
                state->NextDecision = 0;
            }
        }
        if (state->Life.Initialized && state->Snapshot.Now && snapshot.Level > state->Snapshot.Level)
            state->AnnounceLevel = true;
        if (state->Snapshot.Now && !state->Snapshot.Alive && snapshot.Alive)
            ai->GetAiObjectContext()->GetValue<uint32>("death count")->Set(0);
        if (state->Snapshot.Now && state->Snapshot.Alive && !snapshot.Alive)
        {
            state->ObserveExperience({SocialEventKind::Death, 0, now, false});
            if (snapshot.PlayerGuild && state->Persona.Patience < 50)
                state->Life.GuildSatisfaction = std::max<std::int16_t>(-100, state->Life.GuildSatisfaction - 1);
            if (Group* group = bot->GetGroup())
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    if (Player* member = ref->GetSource(); member && member != bot)
                        state->ObserveExperience({SocialEventKind::Death, member->GetGUID().GetCounter(), now,
                                                  state->Persona.Patience >= 50});
        }
        state->Snapshot = std::move(snapshot);
        state->Snapshot.RetryAfter = state->RetryAfter;
        if (state->Life.ReturnAt && state->Life.Eligible(now))
            state->Life.Start(now, state->Snapshot.PlayerGuild, rand32());
        else if (state->Life.Initialized && state->Life.Eligible(now) &&
                 (!state->Life.SessionDue || state->Life.LoginAfter))
            state->Life.StartSession(now, rand32());
        state->Life.UpdateGuild(now, state->Snapshot.PlayerGuild, state->Persona.Seed, state->Snapshot.Guild);
        state->Dirty = true;
    }
    if (updateParty && !PlayerbotWorldThreadProcessor::instance().QueueOperation(
                           std::make_unique<PartyUpdateOperation>(bot->GetGUID())))
        LOG_ERROR("playerbots", "ADVAI party update operation queue full for {}", bot->GetName());
    Plan(ai, state, now);
    UpdatePlayerMovement(ai);
    TravelMannerisms(ai, state, now);
    SummonDuties(ai, state, now);
}

bool Manager::Claim(std::string const& key, BotId responder, Timestamp now, bool direct)
{
    std::lock_guard lock(_chatMutex);
    if (_claims.size() >= 4096)
        return false;
    auto found = _claims.find(key);
    if (!direct && found != _claims.end() && found->second.second + CLAIM_LIFETIME > now)
        return false;
    _claims[key] = {responder, now};
    return true;
}

bool Manager::IsConversation(std::string const& text) const
{
    if (!sPlayerbotAIConfig.CommandPrefix.empty() && text.starts_with(sPlayerbotAIConfig.CommandPrefix))
        return false;
    return !RegisteredCommand(text) && !ChatRules::Parse(text).IsCommand;
}

bool Manager::IsDeliveredSpeech(BotId sender, std::string const& text) const
{
    std::lock_guard lock(_chatMutex);
    return _utterances.Find(sender, text, Now()).has_value();
}

void Manager::ResumeSocialWork(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    std::vector<DeferredChat> ready;
    {
        std::lock_guard lock(state->Mutex);
        for (auto it = state->PendingRelationshipEvents.begin(); it != state->PendingRelationshipEvents.end();)
            if (state->ApplyRelationship(*it))
            {
                it = state->PendingRelationshipEvents.erase(it);
                state->Dirty = true;
            }
            else
                ++it;
        for (auto it = state->DeferredMessages.begin(); it != state->DeferredMessages.end();)
        {
            if (now >= it->Expires)
            {
                LOG_ERROR("playerbots", "ADVAI conversation expired awaiting relationship load for {} and {}",
                          state->Snapshot.Id, it->Sender);
                ++_expiredMessages;
                it = state->DeferredMessages.erase(it);
            }
            else if (state->EnsureContact(it->Sender))
            {
                ready.push_back(std::move(*it));
                it = state->DeferredMessages.erase(it);
            }
            else
                ++it;
        }
    }
    for (auto const& message : ready)
        ReceiveUtterance(ai, message.Channel, message.Sender, message.Text, message.ChannelName, true);
}

bool Manager::QueueLlm(PlayerbotAI* ai, Player* sender, BotState& state, std::uint32_t channel,
                       std::string const& text, std::string const& channelName, std::uint64_t thread, bool initiating)
{
    auto now = Now();
    if (!thread)
        thread = (static_cast<std::uint64_t>(ai->GetBot()->GetGUID().GetCounter()) << 32) ^ state.LlmRevision ^ now;
    {
        std::lock_guard lock(_chatMutex);
        if (_threads.size() >= 4096 && !_threads.contains(thread))
        {
            ++_suppressed;
            return false;
        }
        auto& exchange = _threads[thread];
        auto id = ai->GetBot()->GetGUID().GetCounter();
        if (exchange.Messages >= MAX_CONVERSATION_MESSAGES ||
            (exchange.Participants.size() >= 5 && !exchange.Participants.contains(id)))
        {
            ++_suppressed;
            return false;
        }
        exchange.Participants.insert(id);
        exchange.Participants.insert(sender->GetGUID().GetCounter());
        exchange.Updated = now;
    }
    auto parsed = ChatRules::Parse(text);
    if (state.LlmPartner != sender->GetGUID().GetCounter() || state.Conversation.LastMessage + 120 < now ||
        parsed.TopicChange)
    {
        state.LlmHistory.clear();
        state.LlmItemFocus = state.LlmSpellFocus = 0;
        state.LlmFocusAt = 0;
    }
    auto request = CaptureLlmRequest(ai, sender, state, channel, text, channelName, thread, initiating);
    request.SubmittedMs = NowMs();
    request.TypingMs = TypingDelayMs();
    std::uint32_t const typingMs = request.TypingMs;
    if (!SubmitLlm(std::move(request)))
    {
        ++_suppressed;
        LOG_WARN("playerbots", "ADVAI llama.cpp request rejected by queue/rate budget for {}", state.Snapshot.Id);
        return false;
    }
    StopForTyping(ai, typingMs);  // the bot stands still while it composes its answer
    state.LlmPartner = sender->GetGUID().GetCounter();
    state.LlmThread = thread;
    if (!initiating)
        state.LlmHistory.emplace_back("speaker", text);
    while (state.LlmHistory.size() > 6)
        state.LlmHistory.pop_front();
    state.Conversation.NextSpeak = Now() + 2;
    ++_accepted;
    return true;
}

void Manager::ProcessLlm()
{
    auto now = Now();
    for (auto& result : TakeLlmResults())
    {
        auto const& request = result.Request;
        if (!result.Error.empty())
        {
            LOG_ERROR("playerbots", "ADVAI llama.cpp bot={} speaker={} failed: {}",
                      request.Bot, request.Sender, result.Error);
            continue;
        }
        auto state = Get(request.Bot);
        auto* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(request.Bot));
        auto* sender = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(request.Sender));
        auto* ai = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        if (!state || !ai || !Applies(bot) || !bot->IsInWorld() || !bot->IsAlive() ||
            request.Expires <= now || !AllowedChannel(bot, sender, request.Channel) ||
            bot->GetMapId() != request.Map || bot->GetZoneId() != request.Zone)
        {
            ++_expiredMessages;
            continue;
        }
        BotPersona persona;
        int affinity = 0;
        bool busy = false;
        {
            std::lock_guard lock(state->Mutex);
            if (state->LlmRevision != request.Revision || state->LifeGoal.Kind != request.Activity)
            {
                ++_expiredMessages;
                continue;
            }
            persona = state->Persona;
            busy = state->Snapshot.HasCommitment() || state->Offer.has_value();
            if (auto contact = state->Relationships.find(request.Sender); contact != state->Relationships.end())
                affinity = contact->second.Affinity;
            if (result.Action == "buy" || result.Action == "sell" || result.Action == "give" || result.Action == "craft")
            {
                state->LlmItemFocus = result.Entity;
                state->LlmSpellFocus = 0;
                state->LlmFocusAt = now;
            }
            else if (result.Action == "cast")
            {
                state->LlmSpellFocus = result.Entity;
                state->LlmItemFocus = 0;
                state->LlmFocusAt = now;
            }
        }
        auto reply = [&](std::string text, bool inventory = false)
        {
            std::lock_guard lock(state->Mutex);
            if (state->LlmRevision != request.Revision || state->Messages.size() >= 3)
                return;
            if (request.Initiating && request.Channel == CHAT_MSG_SAY)
                text = sender->GetName() + ", " + text;
            PendingMessage pending;
            pending.Sender = request.Sender;
            pending.Channel = request.Channel;
            pending.ChannelName = request.ChannelName;
            pending.Text = text;
            pending.Source = std::move(text);
            pending.Incoming = ChatRules::Parse(request.Incoming);
            // The typing time runs from when the request was sent: a reply that arrives early waits out the rest, one
            // that took longer than the typing time goes out immediately.
            pending.Due = now;
            pending.DueMs = std::max<std::uint64_t>(NowMs(), request.SubmittedMs + request.TypingMs);
            pending.Expires = request.Expires;
            pending.Thread = request.Thread ? request.Thread :
                (static_cast<std::uint64_t>(request.Bot) << 32) ^ request.Revision ^ now;
            pending.LlmRevision = request.Revision;
            pending.LlmInventory = inventory;
            state->Messages.push_back(std::move(pending));
        };
        if (result.Action == "chat")
        {
            reply(std::move(result.Reply));
            continue;
        }
        if (result.Action == "inventory")
        {
            reply(DescribeLlmInventory(ai), true);
            continue;
        }
        if (result.Action == "spells")
        {
            reply(DescribeLlmSpells(ai));
            continue;
        }
        if (result.Action == "goals" || result.Action == "needs")
        {
            std::string facts;
            {
                std::lock_guard lock(state->Mutex);
                if (result.Action == "goals")
                    facts = "Right now: " + std::string(GoalPlanner::Name(state->LifeGoal.Kind)) +
                        ". Longer term: " + BotAmbition::Name(state->Cognition.Ambition.Kind) + " (" +
                        std::to_string(state->Cognition.Ambition.Progress) + "/" +
                        std::to_string(state->Cognition.Ambition.Target) + ").";
                else
                {
                    auto const& s = state->Snapshot;
                    std::vector<std::string> needs;
                    if (s.NeedsFood) needs.push_back("food");
                    if (s.NeedsDrink) needs.push_back("drink");
                    if (s.NeedsAmmo) needs.push_back("ammunition");
                    if (s.NeedsReagent) needs.push_back("reagents");
                    if (s.NeedsTool) needs.push_back("profession tools");
                    if (s.Durability < 25) needs.push_back("repairs");
                    if (s.FreeBagSlots < 2) needs.push_back("bag space");
                    if (s.NeedsTraining) needs.push_back("training");
                    facts = needs.empty() ? "I'm set for the basics right now." : "I could use ";
                    for (std::size_t i = 0; i < needs.size(); ++i)
                        facts += (i ? ", " : "") + needs[i];
                    if (!needs.empty()) facts += '.';
                }
            }
            reply(std::move(facts));
            continue;
        }
        std::string informational = result.Action == "quest" ? "what quest are you on?" :
            result.Action == "profession" ? "what professions do you have?" :
            result.Action == "location" ? "where are you?" :
            result.Action == "ready" ? "are you ready?" : "";
        if (!informational.empty())
        {
            ReceiveUtterance(ai, request.Channel, request.Sender, informational, request.ChannelName, true, true);
            continue;
        }
        if (result.Action == "trade_cancel")
        {
            auto offer = Offer(request.Bot);
            if (offer && offer->Peer == request.Sender)
            {
                FinishTrade(request.Bot, false, "conversation participant cancelled the transaction", &*offer);
                reply("Okay, let's leave the trade.");
            }
            else
                reply("We don't have an agreed trade to cancel.");
            continue;
        }
        if (ChatRules::Parse(request.Incoming).Negated)
        {
            reply("I'll leave that alone unless you clearly ask me to do it.");
            continue;
        }
        bool sameGroup = bot->GetGroup() && bot->GetGroup() == sender->GetGroup();
        bool sameGuild = bot->GetGuildId() && bot->GetGuildId() == sender->GetGuildId();
        bool willing = affinity > -20 && (affinity >= 10 || sameGroup || sameGuild ||
                                          persona.Generosity + persona.Sociability >= 100);
        if (result.Action == "focus" || result.Action == "peel" || result.Action == "regroup")
        {
            if (!willing || !sameGroup || !bot->InBattleground() || ai->HasGameClientMaster())
                reply("We need to be in the same PvP group for that.");
            else if (result.Action == "regroup")
                reply(ai->DoSpecificAction("bg regroup", Event("advai", ""), true)
                    ? "Moving toward safer teammates." : "I don't see a safer regroup point right now.");
            else
                ReceiveUtterance(ai, request.Channel, request.Sender,
                    (result.Action == "peel" ? "peel me " : "focus ") + request.Incoming,
                    request.ChannelName, true, true);
            continue;
        }
        if (result.Action == "cast")
        {
            if (!willing || bot->IsInCombat() || bot->IsNonMeleeSpellCast(false) || bot->IsInFlight() ||
                bot->GetTradeData() || bot->IsBeingTeleported() || bot->GetVehicle() ||
                (ai->HasGameClientMaster() && ai->GetMaster() != sender) ||
                !sender->IsAlive() || sender->GetMap() != bot->GetMap() || !bot->InSamePhase(sender) ||
                !bot->IsWithinDistInMap(sender, 40.0f) || !bot->CanSeeOrDetect(sender) ||
                !LlmFriendlySpell(bot, result.Entity) || !ai->CanCastSpell(result.Entity, sender, true))
            {
                reply("I can't cast that for you right now.");
                continue;
            }
            bool cast = ai->CastSpell(result.Entity, sender);
            reply(cast ? "Casting " + ChatHelper::FormatSpell(sSpellMgr->GetSpellInfo(result.Entity)) + " on you."
                       : "That cast didn't start.");
            continue;
        }
        if ((busy && !(result.Action == "give" && sameGroup) && result.Action != "guild_invite") ||
            bot->IsInCombat() || bot->IsInFlight() || ai->HasGameClientMaster())
        {
            reply("I'm tied up with something right now.");
            continue;
        }
        if (result.Action == "repair" || result.Action == "restock" || result.Action == "train" || result.Action == "craft")
        {
            Goal goal = result.Action == "repair" ? Goal::Repair : result.Action == "restock" ? Goal::Restock :
                result.Action == "train" ? Goal::Train : Goal::Craft;
            auto recipe = goal == Goal::Craft ? RecipeForItem(bot, result.Entity) : std::optional<RecipePlan>{};
            bool accepted = false;
            {
                std::lock_guard lock(state->Mutex);
                auto const& s = state->Snapshot;
                bool needed = goal == Goal::Craft ? recipe.has_value() : goal == Goal::Repair ? s.Durability < 95 :
                    goal == Goal::Train ? s.NeedsTraining :
                    s.NeedsFood || s.NeedsDrink || s.NeedsAmmo || s.NeedsReagent || s.NeedsTool || s.NeedsPetFood || s.NeedsMount;
                if (willing && needed && now >= state->RetryAfter[static_cast<std::size_t>(goal)] &&
                    !(state->LifeGoal.Kind == Goal::Craft && state->LifeGoal.Target && recipe &&
                      state->LifeGoal.Target != recipe->Spell))
                {
                    state->RetainActivityBefore(goal);
                    state->LifeGoal = {goal, now, now + 120, 0, recipe ? recipe->Spell : 0, 0,
                        Outcome::Started, "accepted a conversational activity request"};
                    state->NextDecision = 0;
                    state->Dirty = true;
                    accepted = true;
                }
            }
            reply(accepted ? "I'll work on " + std::string(GoalPlanner::Name(goal)) +
                "; I'll need the usual money, materials and access."
                : "I can't take that on right now, or I don't have the required need or recipe.");
            continue;
        }
        if (result.Action == "guild_invite")
        {
            bool invited = willing && GuildInviteOperation(bot->GetGUID(), sender->GetGUID()).Execute();
            reply(invited ? "I've sent a guild invitation." : "I can't send you a guild invitation right now.");
            continue;
        }
        if (result.Action == "give")
        {
            Item* gift = nullptr;
            if (sameGroup && affinity >= 20 && persona.Generosity >= 40 && willing)
                for (auto* item : ai->GetInventoryAndEquippedItems())
                    if (item && item->GetEntry() == result.Entity && !item->IsEquipped() &&
                        !item->IsInTrade() && item->CanBeTraded() && !ItemReserved(request.Bot, result.Entity) &&
                        item->GetTemplate()->Class == ITEM_CLASS_CONSUMABLE &&
                        bot->GetItemCount(result.Entity, false) >= item->GetCount() + 10)
                    {
                        gift = item;
                        break;
                    }
            bool started = gift && GiftOperation(bot->GetGUID(), sender->GetGUID(), gift->GetGUID()).Execute();
            reply(started ? "I've opened a trade with some spare supplies for you."
                          : "I can't spare that as a gift right now.");
            continue;
        }
        if (result.Action == "invite")
        {
            bool invited = willing && InviteOperation(bot->GetGUID(), sender->GetGUID()).Execute();
            reply(invited ? "I've sent you a group invitation." : "I can't invite you right now.");
            continue;
        }
        auto* proto = sObjectMgr->GetItemTemplate(result.Entity);
        if (!proto)
        {
            LOG_ERROR("playerbots", "ADVAI llama.cpp item vanished for {}", request.Bot);
            continue;
        }
        auto canonical = std::string(result.Action == "buy" ? "wtb " : "wts ") + ai->GetChatHelper()->FormatItem(proto);
        ReceiveUtterance(ai, request.Channel, request.Sender, canonical, request.ChannelName, true, true);
        if (!Offer(request.Bot))
            reply("That isn't a trade I can offer right now.");
    }
}

bool Manager::ReceiveChat(PlayerbotAI* ai, std::uint32_t channel, BotId senderId, std::string const& text,
                          std::string const& channelName)
{
    Player* traceSender = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(senderId));
    if (!Applies(ai->GetBot()))
    {
        TraceDrop(traceSender, ai->GetBot(), -1);
        return false;
    }
    if (traceSender && traceSender->GetSession() && !traceSender->GetSession()->IsHeadless())
        LOG_INFO("playerbots", "ADVAI chat trace: {} received channel={} from {}", ai->GetBot()->GetName(), channel,
                 traceSender->GetName());
    Player* sender = traceSender;
    if (!sPlayerbotAIConfig.AdvaiLlmEnabled && channel == CHAT_MSG_WHISPER &&
        sender && sender->GetSession() && !sender->GetSession()->IsHeadless() &&
        IsConversation(text))
    {
        auto requests = ChatRules::Requests(text);
        if (requests.size() > 1)
        {
            // A correction replaces earlier clauses before any invitation or quote is prepared.
            auto last = std::find_if(requests.rbegin(), requests.rend(),
                                     [](auto const& request) { return ChatRules::Parse(request).Correction; });
            auto begin = last == requests.rend() ? requests.begin() : std::prev(last.base());
            for (auto request = begin; request != requests.end(); ++request)
                ReceiveUtterance(ai, channel, senderId, *request, channelName);
            return true;
        }
    }
    return ReceiveUtterance(ai, channel, senderId, text, channelName);
}

bool Manager::ReceiveUtterance(PlayerbotAI* ai, std::uint32_t channel, BotId senderId, std::string const& text,
                               std::string const& channelName, bool resumed, bool llmResolved)
{
    Player* bot = ai->GetBot();
    if (!Applies(bot))
        return false;
    if (channel == CHAT_MSG_PARTY_LEADER)
        channel = CHAT_MSG_PARTY;
    else if (channel == CHAT_MSG_RAID_LEADER)
        channel = CHAT_MSG_RAID;
    Player* sender = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(senderId));
    if (!AllowedChannel(bot, sender, channel))
        return TraceDrop(sender, bot, __LINE__);
    Intent generatedTopic = Intent::Unknown;
    std::string source;
    std::uint64_t deliveredThread = 0;
    {
        std::lock_guard chatLock(_chatMutex);
        if (auto previous = _utterances.Find(senderId, text, Now()))
        {
            generatedTopic = previous->Topic;
            source = previous->Source;
            deliveredThread = previous->Thread;
        }
    }
    bool directedExchange = false;
    // Peer exchanges wind down probabilistically as well as having a hard turn limit.
    if (deliveredThread && sender && sender->GetSession() && sender->GetSession()->IsHeadless())
    {
        std::lock_guard chatLock(_chatMutex);
        auto const thread = _threads.find(deliveredThread);
        if (thread != _threads.end())
        {
            // A directed exchange has one intended listener, even when spoken in public.
            if (thread->second.Participants.size() == 2)
            {
                if (!thread->second.Participants.contains(bot->GetGUID().GetCounter()))
                    return true;
                directedExchange = true;
            }
            if (urand(0, 99) < PeerReplyStopChance(thread->second.Messages))
            {
                ++_suppressed;
                return true;
            }
        }
    }
    if (source.empty() && !IsConversation(text))
        return TraceDrop(sender, bot, __LINE__);
    // Canonical annotations are accepted only for an exact, actually delivered message from this sender.
    auto message = ChatRules::Parse(source.empty() ? text : source);
    if (!source.empty())
        message.IsCommand = false;
    auto received = ChatRules::Parse(text).Normalized;
    if (message.Tokens.empty())
        return TraceDrop(sender, bot, __LINE__);
    std::uint32_t tradeUsage = ITEM_USAGE_NONE;
    if (!message.Items.empty())
        tradeUsage =
            ai->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(message.Items.front()))->Get();
    Timestamp now = Now();
    BotId id = bot->GetGUID().GetCounter();
    auto state = Ensure(id);
    std::string botName = bot->GetName();
    std::transform(botName.begin(), botName.end(), botName.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool mentioned = std::find(message.Tokens.begin(), message.Tokens.end(), botName) != message.Tokens.end();
    bool direct = channel == CHAT_MSG_WHISPER || mentioned || directedExchange;
    if (direct && source.empty() && message.Question && text.size() > 90 && state->Persona.Seed % 4 == 1)
        PausePlayerTravel(ai, 1 + state->Persona.Patience / 40);
    if (!direct && (!Active(id) || urand(0, 99) > state->Persona.Sociability / 3))
        return TraceDrop(sender, bot, __LINE__);
    std::string scope = std::to_string(channel) + ":" + channelName + ":";
    if (channel == CHAT_MSG_GUILD)
        scope += std::to_string(bot->GetGuildId());
    else if (bot->GetGroup() && (channel == CHAT_MSG_PARTY || channel == CHAT_MSG_RAID))
        scope += bot->GetGroup()->GetGUID().ToString();
    else if (channel == CHAT_MSG_WHISPER)
        scope += std::to_string(id);
    else
        scope += std::to_string(ActivityRoster::ZoneKey(bot->GetMapId(), bot->GetZoneId()));
    if (!resumed && !Claim(scope + ":" + std::to_string(senderId) + ":" + received, id, now, direct))
    {
        ++_suppressed;
        return TraceDrop(sender, bot, __LINE__);
    }
    std::lock_guard lock(state->Mutex);
    if (!state->EnsureContact(senderId))
    {
        if (state->DeferredMessages.size() >= 3)
        {
            ++_suppressed;
            LOG_ERROR("playerbots", "ADVAI deferred conversation queue full for {}", id);
            return TraceDrop(sender, bot, __LINE__);
        }
        auto existing =
            std::find_if(state->DeferredMessages.begin(), state->DeferredMessages.end(),
                         [&](auto const& pending) { return pending.Sender == senderId && pending.Text == text; });
        if (existing == state->DeferredMessages.end())
            state->DeferredMessages.push_back({senderId, channel, text, channelName, now + 30});
        return TraceDrop(sender, bot, __LINE__);
    }
    if (!llmResolved)
        ++state->LlmRevision;
    if (message.Correction || message.TopicChange)
    {
        std::erase_if(state->Messages, [senderId](auto const& pending) { return pending.Sender == senderId; });
        std::erase_if(state->Requests, [senderId](auto const& request)
                      { return request.Kind == Goal::Socialize && request.Requester == senderId; });
        if (state->Offer && state->Offer->Peer == senderId && !state->Offer->Agreed)
        {
            state->Offer.reset();
            state->Dirty = true;
        }
    }
    std::optional<Intent> negotiatedIntent;
    auto conversation = state->Conversation.ForPartner(senderId, now);
    struct UnsentQuote
    {
        BotState& State;
        std::optional<TradeOffer> Previous;
        bool Queued = false;
        ~UnsentQuote()
        {
            if (!Queued && State.Offer && !State.Offer->Agreed)
                State.Offer = Previous;
        }
    } unsent{*state, state->Offer};
    if (state->Offer && state->Offer->Peer == senderId && direct)
    {
        Intent proposed = ChatRules::Resolve(message, conversation, now);
        Negotiate(ai, sender, message, proposed, *state, now, tradeUsage, direct);
        if (proposed == Intent::TradeOffer || proposed == Intent::TradeCancel)
            negotiatedIntent = proposed;
    }
    auto replyWindow = conversation;
    replyWindow.NextSpeak = now;
    auto pendingForPartner = std::count_if(state->Messages.begin(), state->Messages.end(),
                                           [senderId](auto const& pending) { return pending.Sender == senderId; });
    bool turnLimit = replyWindow.LastMessage && now < replyWindow.LastMessage + 120 &&
                     replyWindow.Messages + pendingForPartner >= MAX_CONVERSATION_MESSAGES;
    if (state->Messages.size() >= 3 || turnLimit || !replyWindow.CanReply(now, senderId, received))
    {
        if (std::find(replyWindow.RecentIncoming.begin(), replyWindow.RecentIncoming.end(), received) !=
            replyWindow.RecentIncoming.end())
            ++_repetitions;
        ++_suppressed;
        return TraceDrop(sender, bot, __LINE__);
    }
    Intent intent = negotiatedIntent ? *negotiatedIntent : ChatRules::Resolve(message, conversation, now);
    bool humanSpeaker = sender->GetSession() && !sender->GetSession()->IsHeadless();
    bool tacticalSpeech = IsPvpIntent(generatedTopic) ||
        (generatedTopic >= Intent::EventDeath && generatedTopic <= Intent::PartyReady);
    if (sPlayerbotAIConfig.AdvaiLlmEnabled && !llmResolved && !state->Offer && !tacticalSpeech &&
        intent != Intent::Stop && generatedTopic != Intent::Farewell &&
        generatedTopic != Intent::Acknowledge &&
        ((humanSpeaker && direct && NeedsLlm(message, intent, conversation)) ||
         (!humanSpeaker && sPlayerbotAIConfig.AdvaiLlmBotChat && Active(id))))
    {
        QueueLlm(ai, sender, *state, channel, text, channelName, deliveredThread);
        return TraceDrop(sender, bot, __LINE__);
    }
    bool entityFollowup = !message.Correction && !message.TopicChange && message.Question &&
                          (intent == Intent::ItemKnown || intent == Intent::SpellKnown ||
                           std::find(message.Tokens.begin(), message.Tokens.end(), "it") != message.Tokens.end() ||
                           std::find(message.Tokens.begin(), message.Tokens.end(), "that") != message.Tokens.end());
    if (!negotiatedIntent && message.Quests.empty() &&
        intent != Intent::Buy && intent != Intent::Sell && intent != Intent::Trade &&
        intent != Intent::TradeCancel && intent != Intent::Stop && !message.Negated)
    {
        if (!message.Items.empty() && message.Spells.empty() && message.Question)
            intent = Intent::ItemKnown;
        else if (!message.Spells.empty() && message.Items.empty() && message.Question)
            intent = Intent::SpellKnown;
        else if (entityFollowup && message.Items.empty() && message.Spells.empty())
        {
            if (conversation.Topic == Intent::ItemKnown && conversation.ItemFocus)
            {
                message.Items.push_back(conversation.ItemFocus);
                intent = Intent::ItemKnown;
            }
            else if (conversation.Topic == Intent::SpellKnown && conversation.SpellFocus)
            {
                message.Spells.push_back(conversation.SpellFocus);
                intent = Intent::SpellKnown;
            }
        }
    }
    if (intent == Intent::ItemKnown || intent == Intent::SpellKnown)
    {
        auto const& entities = intent == Intent::ItemKnown ? message.Items : message.Spells;
        if (entities.size() != 1 || !EntityFacts(CurrentCatalog(), TEXT_LOCALE, bot, intent, entities.front(), &message))
            intent = Intent::Clarify;
    }
    auto questQuestion = ChatRules::QuestTopic(message);
    if (!message.Quests.empty() && (intent == Intent::Class || intent == Intent::Location))
        intent = Intent::Quest;
    if ((intent == Intent::Location || (intent == Intent::Class && questQuestion == QuestSubject::Level)) &&
        conversation.Topic == Intent::QuestKnown && conversation.QuestFocus &&
        (std::find(message.Tokens.begin(), message.Tokens.end(), "that") != message.Tokens.end() ||
         std::find(message.Tokens.begin(), message.Tokens.end(), "it") != message.Tokens.end()))
        intent = Intent::QuestKnown;
    std::uint32_t questFocus = 0;
    std::uint32_t professionFocus = ProfessionSubject(message);
    if (!message.Correction && !message.TopicChange && conversation.Topic == Intent::Profession &&
        conversation.ProfessionFocus && message.Question &&
        (entityFollowup || message.Normalized.find("how high") != std::string::npos ||
         message.Normalized.find("what level") != std::string::npos))
    {
        intent = Intent::Profession;
        if (!professionFocus)
            professionFocus = conversation.ProfessionFocus;
    }
    if (intent == Intent::Quest || intent == Intent::QuestKnown)
    {
        if (!message.Quests.empty())
            questFocus = message.Quests.front();
        else if (intent == Intent::QuestKnown && !message.Correction && !message.TopicChange &&
                 now < conversation.LastMessage + 120)
            questFocus = conversation.QuestFocus;
        else if (message.Normalized.find("your quest") != std::string::npos ||
                 message.Normalized.find("what quest") != std::string::npos ||
                 message.Normalized.find("which quest") != std::string::npos)
            questFocus = state->Snapshot.QuestId;
        if (questFocus)
            intent = QuestFacts(bot, questFocus, questQuestion) ? Intent::QuestKnown : Intent::Clarify;
        else if (!message.Quests.empty())
            intent = Intent::Clarify;
    }
    if (intent == Intent::Friend)
        if (auto contact = state->Relationships.find(senderId);
            contact != state->Relationships.end() && contact->second.Encounters && contact->second.Affinity >= 0)
            intent = Intent::FriendRemembered;
    bool acknowledgeGenerated = false;
    if (IsPvpIntent(generatedTopic))
        return TraceDrop(sender, bot, __LINE__); // Tactical calls are observations, not invitations to a reply storm.
    if (IsPvpIntent(intent) && (!bot->InBattleground() || message.Negated))
        intent = message.Negated ? Intent::Acknowledge : Intent::Battleground;
    if (generatedTopic >= Intent::EventDeath && generatedTopic <= Intent::PartyReady)
    {
        if (urand(0, 99) >= 20 || generatedTopic == Intent::PartyReady || generatedTopic == Intent::PartyWait)
            return TraceDrop(sender, bot, __LINE__);
        intent = generatedTopic == Intent::EventVictory || generatedTopic == Intent::EventQuest
                     ? Intent::Congratulations
                     : Intent::Acknowledge;
        acknowledgeGenerated = true;
    }
    bool socialFollowup = !message.Question && !message.Negated && !message.Correction &&
        conversation.Messages < 2 && !state->Snapshot.HasCommitment() &&
        state->Cognition.Mood.Stress < 60 && state->Persona.Sociability > 50 &&
        (generatedTopic == Intent::SmallTalkAnswer || generatedTopic == Intent::Introduction) &&
        urand(0, 99) < state->Persona.Sociability / 2;
    if (socialFollowup)
        intent = Intent::SmallTalk;
    if (!socialFollowup && !message.Question && (generatedTopic == Intent::SmallTalkAnswer || generatedTopic == Intent::Class ||
                              generatedTopic == Intent::Location || generatedTopic == Intent::Activity ||
                              generatedTopic == Intent::Equipment || generatedTopic == Intent::QuestKnown ||
                              generatedTopic == Intent::Introduction || generatedTopic == Intent::Progress ||
                              generatedTopic == Intent::FriendRemembered || generatedTopic == Intent::Thanks))
    {
        if (urand(0, 99) >= 20)
            return TraceDrop(sender, bot, __LINE__);
        intent = Intent::Acknowledge;
        acknowledgeGenerated = true;
    }
    if (generatedTopic == Intent::GroupAccept || generatedTopic == Intent::Acknowledge ||
        generatedTopic == Intent::TradeCancel || generatedTopic == Intent::Farewell)
        return TraceDrop(sender, bot, __LINE__);
    auto inviter = state->Relationships.find(senderId);
    bool willingPartner = inviter == state->Relationships.end() || inviter->second.Affinity > -20;
    if (generatedTopic == Intent::GroupOffer && willingPartner && !state->Snapshot.HasCommitment() && !bot->GetGroup() &&
        state->LifeGoal.Kind == Goal::Quest && now >= state->NextInvite && state->Persona.Sociability > 40 &&
        std::abs(static_cast<int>(bot->GetLevel()) - static_cast<int>(sender->GetLevel())) <= 5 &&
        state->Requests.size() < 2)
    {
        intent = Intent::GroupAccept;
        state->Requests.push_back({Goal::Socialize, senderId, 0, now + 60});
        state->NextInvite = now + 600;
    }
    else if (generatedTopic == Intent::GroupOffer)
        intent = Intent::Activity;
    if (direct && generatedTopic == Intent::Unknown && conversation.Topic == Intent::GroupOffer)
    {
        bool accepted = AcceptsInvitation(message.Normalized);
        bool declined = DeclinesInvitation(message.Normalized);
        if (accepted || declined)
        {
            intent = Intent::Acknowledge;
            acknowledgeGenerated = true;
            state->NextInvite = now + 600;
            if (accepted)
            {
                if (!state->Snapshot.HasCommitment() && state->LifeGoal.Kind == Goal::Quest && !bot->GetGroup() &&
                    !bot->GetGroupInvite() && state->Requests.size() < 2 &&
                    std::abs(static_cast<int>(bot->GetLevel()) - static_cast<int>(sender->GetLevel())) <= 10)
                    state->Requests.push_back({Goal::Socialize, senderId, 0, now + 60});
                else
                    intent = Intent::Activity;
            }
        }
    }
    if (!negotiatedIntent)
        Negotiate(ai, sender, message, intent, *state, now, tradeUsage, direct);
    if ((intent == Intent::Acknowledge && !acknowledgeGenerated) ||
        (intent == Intent::Unknown && !message.Question && !direct))
        return TraceDrop(sender, bot, __LINE__);
    if (message.Negated && (intent == Intent::Buy || intent == Intent::Sell || intent == Intent::Group ||
                            intent == Intent::Follow || intent == Intent::Help))
        intent = Intent::Clarify;
    if (!message.Negated && !state->Snapshot.HasCommitment() && state->Requests.size() < 2)
    {
        auto has = [&](char const* token)
        { return std::find(message.Tokens.begin(), message.Tokens.end(), token) != message.Tokens.end(); };
        bool guildRequest = direct && generatedTopic == Intent::Unknown && has("guild") &&
                            ((has("invite") && has("me")) ||
                             (has("join") && has("your") && (has("i") || has("can"))));
        if (guildRequest && bot->GetGuildId() && !sender->GetGuildId() && now >= state->NextInvite)
        {
            state->Requests.push_back({Goal::Socialize, senderId, 0, now + 60, SocialRequest::GuildInvite});
            state->NextInvite = now + 600;
            intent = Intent::Guild;
        }
        if ((intent == Intent::Group || intent == Intent::Dungeon || intent == Intent::Quest) &&
            (has("invite") || has("together")) && now >= state->NextInvite && !bot->GetGroup() &&
            (sender->GetLevel() > bot->GetLevel() ? sender->GetLevel() - bot->GetLevel()
                                                  : bot->GetLevel() - sender->GetLevel()) <= 10)
        {
            state->Requests.push_back(
                {Goal::Socialize, senderId, message.Quests.empty() ? 0 : message.Quests.front(), now + 60});
            state->NextInvite = now + 600;
        }
    }
    if (IsPvpIntent(intent) && !message.Negated && !ai->HasGameClientMaster() &&
        (!message.Question || message.Act == SpeechAct::Request))
        if (auto* target = PvpRequestTarget(ai, sender, message, intent))
        {
            state->SuggestedPvpTarget = target->GetGUID().GetCounter();
            state->SuggestedPvpUntil = now + 12;
        }
    if (intent == Intent::Emote && (message.Negated || !direct))
        intent = message.Negated ? Intent::Acknowledge : Intent::Unknown;
    if (intent == Intent::Summon && !message.Negated && (direct || bot->GetGroup()))
    {
        // Remember who asked: the bot joins their ritual, or answers a summon request, for the next few minutes.
        state->SummonFor = senderId;
        state->SummonUntil = now + 240;
        state->NextSummonScan = 0;
    }
    state->LastUnderstood = intent;
    state->LastConfidence = message.Confidence;
    std::string key = std::string("advai_") + ChatRules::Name(intent);
    DialogueContext dialogue;
    dialogue.PublicAudience = channel != CHAT_MSG_WHISPER;
    dialogue.PreferStatement = message.Question || conversation.Messages >= 3;
    dialogue.RequireQuestion = socialFollowup;
    if (auto contact = state->Relationships.find(senderId); contact != state->Relationships.end())
    {
        dialogue.KnownPartner = contact->second.Encounters > 0;
        dialogue.Affinity = contact->second.Affinity;
    }
    auto locale = TEXT_LOCALE;
    auto speech = SelectCatalogSpeech(CurrentCatalog(), key, locale, rand32(), state->Conversation.RecentVariants,
                                      state->Persona, state->Cognition.Mood, dialogue);
    std::string responseTemplate = ChatRules::Style(std::move(speech.Text), state->Persona, rand32());
    std::uint64_t threadId = std::hash<std::string>{}(scope + std::to_string(senderId) + received);
    if (llmResolved && state->LlmThread)
        threadId = state->LlmThread;
    {
        std::lock_guard chatLock(_chatMutex);
        if (deliveredThread && _threads.count(deliveredThread))
            threadId = deliveredThread;
        if (_threads.size() >= 4096 && !_threads.count(threadId))
            return TraceDrop(sender, bot, __LINE__);
        auto& thread = _threads[threadId];
        if (thread.Messages >= MAX_CONVERSATION_MESSAGES ||
            (thread.Participants.size() >= 5 && !thread.Participants.count(id)))
        {
            ++_suppressed;
            return TraceDrop(sender, bot, __LINE__);
        }
        thread.Participants.insert(senderId);
        thread.Participants.insert(id);
        thread.Updated = now;
    }
    // Tactical PvP calls keep their own short delay. Every other rule-based reply "types" for one to two seconds.
    bool const tacticalReply = IsPvpIntent(intent);
    std::uint32_t const typingMs = tacticalReply ? 0 : TypingDelayMs();
    Timestamp delay = tacticalReply
        ? std::min<Timestamp>(6, 1 + text.size() / 70 + responseTemplate.size() / 50 +
                                   state->Persona.ReactionMilliseconds() / 1000)
        : (typingMs + 999) / 1000;
    Timestamp due = std::max(now + delay, state->Conversation.NextSpeak);
    message.Normalized = std::move(received);
    state->Messages.push_back({senderId, channel, channelName, responseTemplate, due, now + MESSAGE_LIFETIME, intent,
                               speech.Variant, threadId, message});
    state->Messages.back().QuestFocus = questFocus;
    state->Messages.back().QuestQuestion = questQuestion;
    state->Messages.back().ProfessionFocus = professionFocus;
    state->Messages.back().Source = std::move(speech.Source);
    state->Messages.back().Catalog = std::move(speech.Catalog);
    state->Messages.back().Locale = locale;
    if (llmResolved)
        state->Messages.back().LlmRevision = state->LlmRevision;
    if (!tacticalReply)
    {
        // Second-accurate gating would add up to a second to the wait; send at the exact millisecond instead.
        auto& queued = state->Messages.back();
        if (intent == Intent::Emote)
            queued.ObservedEntity = EmoteFromTokens(message.Tokens);
        queued.Due = std::max<Timestamp>(now, state->Conversation.NextSpeak);
        // NowMs() is server-uptime milliseconds while Due is wall-clock seconds: convert the remaining wait to a
        // duration instead of comparing the two clocks, or the reply never becomes due.
        std::uint64_t const waitMs = queued.Due > now ? static_cast<std::uint64_t>(queued.Due - now) * 1000 : 0;
        queued.DueMs = NowMs() + std::max<std::uint64_t>(typingMs, waitMs);
        StopForTyping(ai, typingMs);
    }
    unsent.Queued = true;
    if (direct && sender->GetGuildId() && sender->GetGuildId() == bot->GetGuildId())
    {
        auto insult = [&](char const* word)
        { return std::find(message.Tokens.begin(), message.Tokens.end(), word) != message.Tokens.end(); };
        if (insult("idiot") || insult("stupid") || insult("noob"))
        {
            state->ObserveExperience({SocialEventKind::GuildParticipation, senderId, now, false});
        }
    }
    state->Conversation.NextSpeak = due + 4;
    ++_accepted;
    return TraceDrop(sender, bot, __LINE__);
}

void Manager::Deliver(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    PendingMessage pending;
    {
        std::lock_guard lock(state->Mutex);
        // A live trade takes attention over a conversation with someone else. Keep
        // other replies queued, while allowing a reply to the current trader through.
        if (Player* trader = ai->GetBot()->GetTrader(); trader && !state->Messages.empty())
        {
            BotId peer = trader->GetGUID().GetCounter();
            auto next = std::find_if(state->Messages.begin(), state->Messages.end(),
                                     [peer](auto const& message) { return message.Sender == peer; });
            if (next == state->Messages.end())
                return;
            std::rotate(state->Messages.begin(), next, std::next(next));
        }
        if (!state->Messages.empty())
        {
            auto const& front = state->Messages.front();
            Player* traceSender = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(front.Sender));
            static std::mutex traceMutex;
            static std::unordered_map<BotId, Timestamp> lastTrace;
            std::lock_guard traceLock(traceMutex);
            auto& last = lastTrace[ai->GetBot()->GetGUID().GetCounter()];
            if (traceSender && traceSender->GetSession() && !traceSender->GetSession()->IsHeadless() && now > last)
            {
                last = now + 2;
                LOG_INFO("playerbots",
                         "ADVAI chat trace: {} queued reply for {}: due={} now={} dueMs={} nowMs={} expires={} "
                         "combat={} casting={} queue={}",
                         ai->GetBot()->GetName(), traceSender->GetName(), front.Due, now, front.DueMs, NowMs(),
                         front.Expires, ai->GetBot()->IsInCombat(), ai->GetBot()->IsNonMeleeSpellCast(true),
                         state->Messages.size());
            }
        }
        if (state->Messages.empty() || state->Messages.front().Due > now ||
            (state->Messages.front().DueMs && NowMs() < state->Messages.front().DueMs))
            return;
        Player* bot = ai->GetBot();
        if (!IsPvpIntent(state->Messages.front().Topic) &&
            (bot->IsInCombat() || bot->IsNonMeleeSpellCast(true)))
        {
            if (state->Messages.front().Expires < now)
            {
                state->Messages.pop_front();
                ++_expiredMessages;
            }
            return;
        }
        pending = std::move(state->Messages.front());
        state->Messages.pop_front();
        if (pending.LlmRevision && pending.LlmRevision != state->LlmRevision)
            return;
    }
    Player* bot = ai->GetBot();
    Player* sender = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(pending.Sender));
    bool const humanRecipient = sender && sender->GetSession() && !sender->GetSession()->IsHeadless();
    if (pending.Expires < now || !AllowedChannel(bot, sender, pending.Channel))
    {
        if (humanRecipient)
            LOG_INFO("playerbots", "ADVAI chat trace: reply from {} to {} dropped before send (expired={}, allowed={}, channel={})",
                     bot->GetName(), sender->GetName(), pending.Expires < now,
                     !(pending.Expires < now) && AllowedChannel(bot, sender, pending.Channel), pending.Channel);
        if (pending.Expires < now)
            ++_expiredMessages;
        return;
    }
    if (humanRecipient)
        LOG_INFO("playerbots", "ADVAI chat trace: delivering reply from {} to {} channel={} topic={} bytes={}",
                 bot->GetName(), sender->GetName(), pending.Channel, ChatRules::Name(pending.Topic), pending.Text.size());
    if ((pending.Topic == Intent::EventDeath && bot->IsAlive()) ||
        (pending.Topic == Intent::EventQuest && bot->GetQuestStatus(pending.ObservedEntity) != QUEST_STATUS_COMPLETE))
        return;
    if (pending.Topic == Intent::PartyReady || pending.Topic == Intent::PartyWait)
    {
        std::lock_guard lock(state->Mutex);
        if (!bot->GetGroup() || (pending.Topic == Intent::PartyWait) != state->PartyHold)
            return;
    }
    if (pending.LlmInventory)
        pending.Source = pending.Text = DescribeLlmInventory(ai);
    if (pending.Text.find("%facts") != std::string::npos)
    {
        std::string facts;
        std::string sourceFacts;
        if (IsPvpIntent(pending.Topic))
        {
            auto knowledge = PvpFacts(pending.Catalog, pending.Locale, ai, pending.Topic, pending.ObservedEntity);
            auto source = PvpFacts(pending.Catalog, 0, ai, pending.Topic, pending.ObservedEntity);
            if (!knowledge || !source)
            {
                ++_suppressed;
                return;
            }
            facts = *knowledge;
            sourceFacts = *source;
        }
        else if (pending.Topic == Intent::ItemKnown || pending.Topic == Intent::SpellKnown)
        {
            auto const& entities = pending.Topic == Intent::ItemKnown ? pending.Incoming.Items : pending.Incoming.Spells;
            if (entities.size() != 1)
                throw std::runtime_error("ADVAI entity reply lost its subject");
            auto knowledge = EntityFacts(pending.Catalog, pending.Locale, bot, pending.Topic, entities.front(), &pending.Incoming);
            auto source = EntityFacts(pending.Catalog, 0, bot, pending.Topic, entities.front(), &pending.Incoming);
            if (!knowledge || !source)
                throw std::runtime_error("ADVAI entity reply references missing gameplay content");
            facts = *knowledge;
            sourceFacts = *source;
        }
        else if (pending.Topic == Intent::Profession)
        {
            auto recipe = CraftRecipe(bot->GetGUID().GetCounter());
            facts = ProfessionFacts(pending.Catalog, pending.Locale, bot, pending.Incoming,
                                    pending.ProfessionFocus, recipe);
            sourceFacts = ProfessionFacts(pending.Catalog, 0, bot, pending.Incoming,
                                          pending.ProfessionFocus, recipe);
        }
        else if (pending.Topic == Intent::QuestKnown)
        {
            auto knowledge =
                QuestFacts(pending.Catalog, pending.Locale, bot, pending.QuestFocus, pending.QuestQuestion);
            auto source = QuestFacts(pending.Catalog, 0, bot, pending.QuestFocus, pending.QuestQuestion);
            if (!knowledge || !source)
            {
                LOG_ERROR("playerbots", "ADVAI quest knowledge disappeared before delivery: {}", pending.QuestFocus);
                ++_blocked;
                return;
            }
            facts = *knowledge;
            sourceFacts = *source;
        }
        else if (pending.Topic == Intent::TradeOffer)
        {
            auto offer = Offer(bot->GetGUID().GetCounter());
            if (!offer || offer->Peer != pending.Sender || offer->Expires <= now)
                return;
            auto* item = sObjectMgr->GetItemTemplate(offer->Item);
            if (!item || (offer->Selling && bot->GetItemCount(offer->Item, false) < offer->Quantity) ||
                (!offer->Selling && bot->GetMoney() < offer->Copper))
            {
                FinishTrade(bot->GetGUID().GetCounter(), false,
                            "quoted goods or purchasing funds changed before reply");
                return;
            }
            auto quantity = std::to_string(offer->Quantity);
            auto link = ai->GetChatHelper()->FormatItem(item);
            auto price = ChatHelper::formatMoney(offer->Copper);
            auto key = offer->Selling ? "trade_sell" : "trade_buy";
            facts = CatalogFact(pending.Catalog, key, pending.Locale, {quantity, link, price});
            sourceFacts = CatalogFact(pending.Catalog, key, 0, {quantity, link, price});
        }
        else
        {
            auto snapshot = Capture(ai);
            BotLifeGoal goal;
            {
                std::lock_guard lock(state->Mutex);
                goal = state->LifeGoal;
            }
            auto* speaker = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(pending.Sender));
            facts = LocalizedFacts(pending.Catalog, pending.Locale, pending.Topic, snapshot, goal,
                                   &pending.Incoming, bot, speaker);
            sourceFacts = LocalizedFacts(pending.Catalog, 0, pending.Topic, snapshot, goal,
                                         &pending.Incoming, bot, speaker);
        }
        PlayerbotTextMgr::replaceAll(pending.Text, "%facts", facts);
        PlayerbotTextMgr::replaceAll(pending.Source, "%facts", sourceFacts);
    }
    if (pending.Text.empty() || pending.Text.size() > MAX_CHAT_BYTES)
    {
        LOG_ERROR("playerbots", "ADVAI invalid rendered delivery for bot {} outcome {}", bot->GetName(),
                  ChatRules::Name(pending.Topic));
        ++_blocked;
        return;
    }
    {
        std::lock_guard chatLock(_chatMutex);
        auto& thread = _threads[pending.Thread];
        bool repeated = thread.Lines.count(pending.Text) != 0;
        if (thread.Messages >= MAX_CONVERSATION_MESSAGES || repeated)
        {
            if (repeated)
                ++_repetitions;
            ++_suppressed;
            return;
        }
        thread.Lines.insert(pending.Text);
        std::uint64_t budgetKey = std::hash<std::string>{}(
            std::to_string(pending.Channel) + pending.ChannelName +
            (pending.Channel == CHAT_MSG_GUILD ? std::to_string(bot->GetGuildId())
             : pending.Channel == CHAT_MSG_PARTY || pending.Channel == CHAT_MSG_RAID
                 ? (bot->GetGroup() ? bot->GetGroup()->GetGUID().ToString() : "no-group")
                 : std::to_string(ActivityRoster::ZoneKey(bot->GetMapId(), bot->GetZoneId()))));
        auto& budget = _channelBudgets[budgetKey];
        if (now >= budget.first + 60)
            budget = {now, 0};
        if ((pending.Channel != CHAT_MSG_WHISPER || !humanRecipient) &&
            budget.second >= sPlayerbotAIConfig.AdvaiChannelMessagesPerMinute)
        {
            ++_suppressed;
            return;
        }
        ++budget.second;
        ++thread.Messages;
        thread.Updated = now;
        _utterances.Record(bot->GetGUID().GetCounter(),
                           {pending.Thread, pending.Text, pending.Topic, pending.Source, now + MESSAGE_LIFETIME});
    }
    bool sent = false;
    switch (pending.Channel)
    {
        case CHAT_MSG_SAY:
            sent = ai->Say(pending.Text);
            break;
        case CHAT_MSG_YELL:
            sent = ai->Yell(pending.Text);
            break;
        case CHAT_MSG_WHISPER:
            sent = ai->Whisper(pending.Text, sender->GetName());
            break;
        case CHAT_MSG_GUILD:
            if (auto* guild = bot->GetGuild(); guild && guild->HasRankRight(bot, GR_RIGHT_GCHATSPEAK))
            {
                auto language = sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD)
                    ? LANG_UNIVERSAL : bot->GetTeamId() == TEAM_HORDE ? LANG_ORCISH : LANG_COMMON;
                guild->BroadcastToGuild(bot->GetSession(), false, pending.Text, language);
                sent = true;
            }
            break;
        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        {
            WorldPacket packet;
            ChatHandler::BuildChatPacket(packet, static_cast<ChatMsg>(pending.Channel), LANG_UNIVERSAL, bot->GetGUID(),
                                         {}, pending.Text, CHAT_TAG_NONE, bot->GetName());
            auto* group = bot->GetGroup();
            int subgroup = pending.Channel == CHAT_MSG_PARTY || pending.Channel == CHAT_MSG_PARTY_LEADER
                ? group->GetMemberGroup(bot->GetGUID()) : -1;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (auto* recipient = ref->GetSource(); recipient && recipient->IsInWorld() &&
                    recipient->GetSession() && (subgroup < 0 || ref->getSubGroup() == subgroup) &&
                    (!recipient->GetSocial() || !recipient->GetSocial()->HasIgnore(bot->GetGUID())))
                {
                    recipient->SendDirectMessage(&packet);
                    sent = true;
                }
            break;
        }
        case CHAT_MSG_CHANNEL:
        {
            switch (ai->GetChatChannelSource(bot, pending.Channel, pending.ChannelName))
            {
                case ChatChannelSource::SRC_WORLD:
                    sent = ai->SayToWorld(pending.Text);
                    break;
                case ChatChannelSource::SRC_GENERAL:
                    sent = ai->SayToChannel(pending.Text, ChatChannelId::GENERAL);
                    break;
                case ChatChannelSource::SRC_TRADE:
                    sent = ai->SayToChannel(pending.Text, ChatChannelId::TRADE);
                    break;
                case ChatChannelSource::SRC_LOOKING_FOR_GROUP:
                    sent = ai->SayToChannel(pending.Text, ChatChannelId::LOOKING_FOR_GROUP);
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
    if (humanRecipient)
        LOG_INFO("playerbots", "ADVAI chat trace: reply from {} to {} channel={} sent={}", bot->GetName(),
                 sender->GetName(), pending.Channel, sent);
    if (sent)
    {
        if (pending.Topic == Intent::Emote && pending.ObservedEntity)
            PerformEmote(ai, pending.ObservedEntity);
        AttendToSpeaker(ai, sender, state, now);
        std::lock_guard lock(state->Mutex);
        state->Conversation.Remember(now, pending.Sender, pending.Incoming, pending.Variant);
        state->Conversation.Topic = pending.Topic;
        state->Conversation.ItemFocus = pending.Topic == Intent::ItemKnown ? pending.Incoming.Items.front() : 0;
        state->Conversation.SpellFocus = pending.Topic == Intent::SpellKnown ? pending.Incoming.Spells.front() : 0;
        state->Conversation.ProfessionFocus = pending.Topic == Intent::Profession ? pending.ProfessionFocus : 0;
        if (pending.Topic == Intent::QuestKnown)
            state->Conversation.QuestFocus = pending.QuestFocus;
        state->Conversation.NextSpeak = now + 8;
        if (pending.LlmRevision && state->LlmPartner == pending.Sender)
        {
            state->LlmHistory.emplace_back("self", pending.Text);
            while (state->LlmHistory.size() > 6)
                state->LlmHistory.pop_front();
        }
        if (pending.Sender)
            state->ObserveExperience({SocialEventKind::Conversation, pending.Sender, now, true});
        state->Dirty = true;
        ++_delivered;
    }
    else
    {
        std::lock_guard chatLock(_chatMutex);
        _utterances.Revoke(bot->GetGUID().GetCounter(), pending.Thread, pending.Text);
        ++_suppressed;
    }
}

void Manager::ReactToEvents(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    Player* bot = ai->GetBot();
    std::unique_lock lock(state->Mutex);
    std::erase_if(state->Reactions, [now](auto const& reaction)
        { return now >= reaction.Event.At + (IsPvpIntent(reaction.Topic) ? 10 : 60); });
    if (state->Reactions.empty() || !Active(bot->GetGUID().GetCounter()) || !state->Life.Initialized ||
        !bot->CanSpeak())
        return;
    auto reaction = state->Reactions.front();
    auto topic = reaction.Topic;
    bool tactical = IsPvpIntent(topic);
    if ((!tactical && (!state->Messages.empty() || now < state->Conversation.NextSpeak ||
                      now < state->NextReaction || bot->IsInCombat())) ||
        (tactical && (state->Messages.size() >= 3 ||
            std::any_of(state->Messages.begin(), state->Messages.end(),
                        [](auto const& pending) { return IsPvpIntent(pending.Topic); }))))
        return;
    if ((topic == Intent::EventDeath && bot->IsAlive()) || (topic == Intent::PartyWait && !state->PartyHold) ||
        (topic == Intent::PartyReady && state->PartyHold))
    {
        state->Reactions.pop_front();
        return;
    }
    Player* peer = reaction.Event.Other ? ObjectAccessor::GetPlayer(
                                              bot->GetMap(), ObjectGuid::Create<HighGuid::Player>(reaction.Event.Other))
                                        : nullptr;
    uint32 channel = CHAT_MSG_WHISPER;
    if (Group* group = bot->GetGroup())
    {
        channel = group->isRaidGroup() ? CHAT_MSG_RAID : CHAT_MSG_PARTY;
        if (!peer || peer->GetGroup() != group)
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* candidate = ref->GetSource();
                    candidate && candidate != bot && candidate->IsInWorld() && AllowedChannel(bot, candidate, channel))
                {
                    peer = candidate;
                    break;
                }
    }
    if (!peer || peer == bot || !AllowedChannel(bot, peer, channel))
        return;
    state->Reactions.pop_front();
    auto scope = bot->GetGroup() ? bot->GetGroup()->GetGUID().GetCounter() : bot->GetGUID().GetCounter();
    auto claim =
        "event:" + std::to_string(scope) + ':' + ChatRules::Name(topic) + ':' + std::to_string(reaction.Event.Entity);
    if (!Claim(claim, bot->GetGUID().GetCounter(), now, false))
        return;
    // Events initiate ordinary delivered conversations and share the same budgets and history.
    auto catalog = CurrentCatalog();
    auto speech = SelectCatalogSpeech(catalog, std::string("advai_") + ChatRules::Name(topic), TEXT_LOCALE, rand32(),
                                      state->Conversation.RecentVariants, state->Persona, state->Cognition.Mood,
                                      {channel != CHAT_MSG_WHISPER, true, 0, tactical});
    auto text = ChatRules::Style(std::move(speech.Text), state->Persona, rand32());
    std::uint32_t const eventTypingMs = tactical ? 0 : TypingDelayMs();
    auto due = tactical ? now + 1 + state->Persona.ReactionMilliseconds() / 1000 + text.size() / 24
                        : now + (eventTypingMs + 999) / 1000;
    std::uint64_t threadId = 0;
    {
        std::lock_guard chatLock(_chatMutex);
        if (_threads.size() >= 4096)
        {
            ++_suppressed;
            return;
        }
        threadId = std::hash<std::string>{}(claim + std::to_string(now));
        auto& thread = _threads[threadId];
        thread.Updated = now;
        thread.Participants.insert(bot->GetGUID().GetCounter());
        thread.Participants.insert(peer->GetGUID().GetCounter());
    }
    PendingMessage pending;
    pending.Sender = peer->GetGUID().GetCounter();
    pending.Channel = channel;
    pending.Text = std::move(text);
    pending.Due = due;
    pending.Expires = std::min(reaction.Event.At + (tactical ? 10 : 60), due + (tactical ? 6 : 25));
    pending.Topic = topic;
    pending.ObservedEntity = reaction.Event.Entity;
    pending.Variant = speech.Variant;
    pending.Thread = threadId;
    pending.Incoming = ChatRules::Parse(speech.Source);
    pending.Source = std::move(speech.Source);
    pending.Catalog = std::move(catalog);
    pending.Locale = TEXT_LOCALE;
    if (!tactical)
    {
        pending.Due = now;
        pending.DueMs = NowMs() + eventTypingMs;
        StopForTyping(ai, eventTypingMs);
    }
    if (tactical)
        state->Messages.push_front(std::move(pending));
    else
        state->Messages.push_back(std::move(pending));
    if (!tactical)
        state->NextReaction = due + 45 + (100 - state->Persona.Sociability);
    state->Conversation.NextSpeak = due + 4;
}

void Manager::Ambient(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    std::unique_lock lock(state->Mutex);
    if (!Active(state->Snapshot.Id) || !state->Life.Initialized || now < state->NextAmbient ||
        now < state->Conversation.NextSpeak || !state->Messages.empty() || state->Snapshot.InCombat ||
        !state->Snapshot.Alive)
        return;
    state->Cognition.Mood.Settle(now);
    state->NextAmbient =
        now + 180 + (100 - state->Persona.Sociability) * 5 + state->Cognition.Mood.SocialDelay() + urand(0, 120);
    if (urand(0, 99) > state->Persona.Sociability / 2 || !ai->GetBot()->CanSpeak())
        return;
    Player* peer = nullptr;
    if (!state->Offer && !ai->HasGameClientMaster() && !state->Snapshot.Trading && !state->Snapshot.InTransit)
    {
        int bestScore = -1;
        unsigned examined = 0;
        for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
        {
            if (++examined > 24)
                break;
            auto* candidate = ObjectAccessor::GetPlayer(ai->GetBot()->GetMap(), guid);
            if (!candidate || candidate == ai->GetBot() || !Applies(candidate) ||
                !Active(candidate->GetGUID().GetCounter()) || !candidate->IsAlive() || candidate->IsInCombat() ||
                candidate->GetTradeData() || candidate->IsInFlight() || candidate->IsBeingTeleported() ||
                !AllowedChannel(ai->GetBot(), candidate, CHAT_MSG_WHISPER) || !ai->GetBot()->InSamePhase(candidate) ||
                !ai->GetBot()->CanSeeOrDetect(candidate) || !ai->GetBot()->IsWithinDistInMap(candidate, 40.0f) ||
                (candidate->GetGUID().GetCounter() == state->LastInitiativePeer && now < state->NextPeerConversation))
                continue;
            auto known = state->Relationships.find(candidate->GetGUID().GetCounter());
            int affinity = known == state->Relationships.end() ? 0 : known->second.Affinity;
            if (affinity <= -20)
                continue;
            int score = affinity +
                        (candidate->GetGuildId() && candidate->GetGuildId() == ai->GetBot()->GetGuildId() ? 20 : 0) +
                        (candidate->GetGroup() && candidate->GetGroup() == ai->GetBot()->GetGroup() ? 25 : 0) + 10 -
                        std::min(10, std::abs(int(candidate->GetLevel()) - int(ai->GetBot()->GetLevel())));
            for (auto const& experience : state->Cognition.Memory.Recent)
                if (experience.Other == candidate->GetGUID().GetCounter() && experience.Positive &&
                    (experience.Kind == SocialEventKind::SharedQuest || experience.Kind == SocialEventKind::Assistance))
                    score += 5;
            if (score > bestScore)
            {
                bestScore = score;
                peer = candidate;
            }
        }
        if (peer && sPlayerbotAIConfig.AdvaiLlmEnabled && sPlayerbotAIConfig.AdvaiLlmBotChat)
        {
            if (!state->EnsureContact(peer->GetGUID().GetCounter()))
                return;
            ++state->LlmRevision;
            state->LastInitiativePeer = peer->GetGUID().GetCounter();
            state->NextPeerConversation = now + 600;
            auto channel = ai->GetBot()->IsWithinDistInMap(peer, 18.0f) &&
                                   AllowedChannel(ai->GetBot(), peer, CHAT_MSG_SAY) && urand(0, 1)
                               ? CHAT_MSG_SAY
                               : CHAT_MSG_WHISPER;
            QueueLlm(ai, peer, *state, channel, "", "", 0, true);
            return;
        }
    }
    auto catalog = CurrentCatalog();
    auto locale = TEXT_LOCALE;
    std::string stockFacts;
    std::string sourceStockFacts;
    if (!state->Offer && state->Persona.Preference == Goal::Trade && state->Snapshot.HasTradeStock &&
        !state->Snapshot.HasCommitment())
    {
        auto frugality = state->Persona.Frugality;
        auto generosity = state->Persona.Generosity;
        lock.unlock();
        for (Item* item : ai->GetInventoryAndEquippedItems())
        {
            if (!item || item->IsEquipped() || item->IsInTrade() || !item->CanBeTraded())
                continue;
            auto* proto = item->GetTemplate();
            if (!proto || !proto->SellPrice)
                continue;
            auto usage =
                ai->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(item->GetEntry()))->Get();
            if (usage != ITEM_USAGE_AH && usage != ITEM_USAGE_VENDOR)
                continue;
            auto price = std::min<std::uint64_t>(MAX_MONEY_AMOUNT, static_cast<std::uint64_t>(proto->SellPrice) *
                                                                       item->GetCount() *
                                                                       (200 + frugality - generosity / 2) / 100);
            auto quantity = std::to_string(item->GetCount());
            auto link = ai->GetChatHelper()->FormatItem(proto);
            auto money = ChatHelper::formatMoney(static_cast<std::uint32_t>(price));
            stockFacts = CatalogFact(catalog, "trade_sell", locale, {quantity, link, money});
            sourceStockFacts = CatalogFact(catalog, "trade_sell", 0, {quantity, link, money});
            break;
        }
        lock.lock();
        if (!state->Messages.empty() || state->Offer || state->Snapshot.HasCommitment())
            return;
    }
    Intent topic = state->AnnounceLevel  ? Intent::Progress
                   : !stockFacts.empty() ? Intent::TradeOffer
                   : state->LifeGoal.Kind == Goal::Quest && state->Persona.Sociability > 65 &&
                           !state->Snapshot.HasCommitment() && !state->Snapshot.QuestComplete &&
                           !state->Snapshot.Group && now >= state->NextInvite
                       ? Intent::GroupOffer
                       : Intent::SmallTalk;
    BotId partner = 0;
    std::string partnerName;
    std::int16_t affinity = 0;
    if (peer && topic == Intent::SmallTalk)
    {
        partner = peer->GetGUID().GetCounter();
        partnerName = peer->GetName();
        auto const relationship = state->Relationships.find(partner);
        if (relationship != state->Relationships.end())
            affinity = relationship->second.Affinity;
        state->LastInitiativePeer = partner;
        state->NextPeerConversation = now + 600;
    }
    if (topic == Intent::GroupOffer && !ai->GetBot()->GetGroupInvite())
        for (auto const& [id, relationship] : state->Relationships)
            if (relationship.Affinity >= 20 && relationship.Encounters >= 3 && relationship.Affinity > affinity)
                if (Player* friendPlayer =
                        ObjectAccessor::GetPlayer(ai->GetBot()->GetMap(), ObjectGuid::Create<HighGuid::Player>(id));
                    friendPlayer && AllowedChannel(ai->GetBot(), friendPlayer, CHAT_MSG_WHISPER) &&
                    friendPlayer->IsAlive() && !friendPlayer->IsInCombat() && !friendPlayer->GetGroup() &&
                    !friendPlayer->GetGroupInvite() && !friendPlayer->GetTradeData() && !friendPlayer->IsInFlight() &&
                    !friendPlayer->InBattlegroundQueue() && ai->GetBot()->InSamePhase(friendPlayer) &&
                    ai->GetBot()->CanSeeOrDetect(friendPlayer) &&
                    AllowedChannel(ai->GetBot(), friendPlayer, CHAT_MSG_WHISPER) &&
                    ai->GetBot()->IsWithinDistInMap(friendPlayer, 60.0f) &&
                    std::abs(static_cast<int>(friendPlayer->GetLevel()) - static_cast<int>(ai->GetBot()->GetLevel())) <=
                        5)
                {
                    partner = id;
                    partnerName = friendPlayer->GetName();
                    affinity = relationship.Affinity;
                }
    std::string key = std::string("advai_") + ChatRules::Name(topic);
    auto speech = SelectCatalogSpeech(catalog, key, locale, rand32(), state->Conversation.RecentVariants,
                                      state->Persona, state->Cognition.Mood, {!partner, partner != 0, affinity});
    std::string text = ChatRules::Style(std::move(speech.Text), state->Persona, rand32());
    PlayerbotTextMgr::replaceAll(text, "%facts",
                                 topic == Intent::TradeOffer
                                     ? stockFacts
                                     : LocalizedFacts(catalog, locale, topic, state->Snapshot, state->LifeGoal));
    PlayerbotTextMgr::replaceAll(speech.Source, "%facts",
                                 topic == Intent::TradeOffer
                                     ? sourceStockFacts
                                     : LocalizedFacts(catalog, 0, topic, state->Snapshot, state->LifeGoal));
    bool const publicPeer = peer && partner == peer->GetGUID().GetCounter() &&
                            ai->GetBot()->IsWithinDistInMap(peer, 18.0f) &&
                            AllowedChannel(ai->GetBot(), peer, CHAT_MSG_SAY) && urand(0, 1);
    if (publicPeer)
    {
        text = partnerName + ", " + text;
        speech.Source = partnerName + ", " + speech.Source;
    }
    if (text.size() > MAX_CHAT_BYTES)
    {
        LOG_ERROR("playerbots", "ADVAI oversized ambient message for {}", key);
        return;
    }
    std::uint64_t threadId = 0;
    {
        std::lock_guard chatLock(_chatMutex);
        auto scope = ActivityRoster::ZoneKey(ai->GetBot()->GetMapId(), ai->GetBot()->GetZoneId());
        auto budgetKey = std::hash<std::string>{}(
            std::to_string(partner && !publicPeer ? CHAT_MSG_WHISPER : CHAT_MSG_SAY) + std::to_string(scope));
        if (!partner)
        {
            auto& budget = _channelBudgets[budgetKey];
            if (now >= budget.first + 60)
                budget = {now, 0};
            if (budget.second >= sPlayerbotAIConfig.AdvaiChannelMessagesPerMinute)
                return;
            ++budget.second;
        }
        threadId = std::hash<std::string>{}(std::to_string(state->Snapshot.Id) + text + std::to_string(now));
        if (_threads.size() >= 4096 && !_threads.count(threadId))
        {
            ++_suppressed;
            return;
        }
        auto& thread = _threads[threadId];
        thread.Updated = now;
        thread.Messages = partner ? 0 : 1;
        thread.Participants.insert(state->Snapshot.Id);
        if (partner)
            thread.Participants.insert(partner);
        else
        {
            thread.Lines.insert(text);
            _utterances.Record(state->Snapshot.Id, {threadId, text, topic, speech.Source, now + MESSAGE_LIFETIME});
        }
    }
    if (partner)
    {
        auto const typingMs = TypingDelayMs();
        PendingMessage pending;
        pending.Sender = partner;
        pending.Channel = publicPeer ? CHAT_MSG_SAY : CHAT_MSG_WHISPER;
        pending.Text = text;
        pending.Due = now;
        pending.DueMs = NowMs() + typingMs;
        pending.Expires = now + MESSAGE_LIFETIME;
        pending.Topic = topic;
        pending.Variant = speech.Variant;
        pending.Thread = threadId;
        pending.Incoming = ChatRules::Parse(text);
        pending.Source = std::move(speech.Source);
        pending.Catalog = std::move(catalog);
        pending.Locale = locale;
        state->Messages.push_back(std::move(pending));
        state->Conversation.NextSpeak = now + 4;
        StopForTyping(ai, typingMs);
        return;
    }
    // Sending packets can synchronously deliver chat to other bots. Do not hold a bot-state lock while sending.
    lock.unlock();
    bool sent = ai->Say(text);
    if (!sent)
    {
        std::lock_guard chatLock(_chatMutex);
        _threads.erase(threadId);
        _utterances.Revoke(ai->GetBot()->GetGUID().GetCounter(), threadId, text);
        ++_suppressed;
        return;
    }
    lock.lock();
    state->Conversation.RecentVariants.push_back(speech.Variant);
    if (state->Conversation.RecentVariants.size() > MAX_RECENT_LINES)
        state->Conversation.RecentVariants.pop_front();
    state->Conversation.NextSpeak = now + 4;
    state->AnnounceLevel = false;
    state->Dirty = true;
    ++_delivered;
}

void Manager::ExpireRequests(BotId id, BotState& state, Timestamp now)
{
    std::erase_if(state.Requests,
                  [&](BotActionRequest const& request)
                  {
                      if (request.Expires > now)
                          return false;
                      LOG_WARN("playerbots", "ADVAI {} {} request from {} expired before execution", id,
                               GoalPlanner::Name(request.Kind), request.Requester);
                      ++_expiredRequests;
                      state.Dirty = true;
                      return true;
                  });
}

void Manager::ExecuteRequests(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    BotActionRequest request;
    {
        std::lock_guard lock(state->Mutex);
        ExpireRequests(ai->GetBot()->GetGUID().GetCounter(), *state, now);
        if (state->Requests.empty() || state->Snapshot.HasCommitment())
            return;
        request = state->Requests.front();
        state->Requests.pop_front();
    }
    Player* bot = ai->GetBot();
    Player* requester = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(request.Requester));
    if (request.Expires < now || !requester || !requester->IsInWorld() ||
        !AllowedChannel(bot, requester, CHAT_MSG_WHISPER) || bot->IsInCombat())
    {
        Report(bot->GetGUID().GetCounter(), {request.Kind, Outcome::Blocked,
                                             "social request expired or its participant/channel is unavailable"});
        return;
    }
    if (request.Action == SocialRequest::GuildInvite)
    {
        if (!PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<GuildInviteOperation>(bot->GetGUID(), requester->GetGUID())))
            Report(bot->GetGUID().GetCounter(), {Goal::Socialize, Outcome::Blocked, "guild invite operation queue is full"});
        return;
    }
    if (bot->GetGroup())
    {
        Report(bot->GetGUID().GetCounter(), {request.Kind, Outcome::Cancelled,
                                             "party request cancelled because the bot joined another group"});
        return;
    }
    if (request.Kind == Goal::Socialize)
    {
        bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
            std::make_unique<InviteOperation>(bot->GetGUID(), requester->GetGUID()));
        if (!queued)
            Report(bot->GetGUID().GetCounter(), {Goal::Socialize, Outcome::Blocked, "world operation queue is full"});
    }
}

void Manager::ObserveCorpse(BotId id, std::uint64_t guid)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        state->LastCorpse = guid;
        state->LastCorpseUntil = Now() + 60;
    }
}

std::uint64_t Manager::RecentCorpse(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (Now() < state->LastCorpseUntil)
            return state->LastCorpse;
    }
    return 0;
}

BotId Manager::PvpSuggestedTarget(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (Now() < state->SuggestedPvpUntil)
            return state->SuggestedPvpTarget;
    }
    return 0;
}

void Manager::PvpCallout(PlayerbotAI* ai, Intent topic, BotId subject)
{
    auto* bot = ai->GetBot();
    if (!Applies(bot) || !IsPvpIntent(topic) || !bot->InBattleground() || !bot->GetGroup() ||
        !Active(bot->GetGUID().GetCounter()))
        return;
    if (auto state = Get(bot->GetGUID().GetCounter()))
    {
        std::lock_guard lock(state->Mutex);
        auto now = Now();
        if (now < state->NextPvpSpeech || state->Reactions.size() >= 8)
            return;
        state->NextPvpSpeech = now + 20 + (100 - state->Persona.Sociability) / 5;
        state->Reactions.push_front({topic, {SocialEventKind::Conversation, 0, now, true, subject}});
    }
}

void Manager::Observe(BotId id, BotSocialEvent const& event)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        state->ObserveExperience(event);
    }
}

void Manager::Report(BotId id, BotActionResult const& result)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.Kind != result.Kind)
        {
            // The blocked activity is not the planned goal (for example an idle travel leg). Apply the same per-kind
            // backoff as a matching goal, so the planner stops choosing that kind and picks another goal.
            if (result.State == Outcome::Blocked)
            {
                auto index = static_cast<std::size_t>(result.Kind);
                state->Failures[index] = std::min<std::uint8_t>(state->Failures[index] + 1, 8);
                state->RetryAfter[index] = Now() + std::min<Timestamp>(600, 5ULL << state->Failures[index]);
                state->NextDecision = 0;
                state->NextSnapshot = 0;
                state->Dirty = true;
                ++_blocked;
                LOG_DEBUG("playerbots", "ADVAI {} {} blocked: {}", id, GoalPlanner::Name(result.Kind), result.Reason);
            }
            return;
        }
        GoalPlanner::ApplyResult(state->LifeGoal, result, Now());
        if (result.State == Outcome::Started && state->LifeGoal.CommittedUntil < Now())
            state->LifeGoal.CommittedUntil = Now() + 60 + state->Persona.Patience * 3;
        state->NextSnapshot = 0;
        if (result.State == Outcome::Succeeded || result.State == Outcome::Blocked ||
            result.State == Outcome::Cancelled)
            state->NextDecision = 0;
        state->Failures[static_cast<std::size_t>(result.Kind)] = state->LifeGoal.Failures;
        if (result.State == Outcome::Blocked)
            state->RetryAfter[static_cast<std::size_t>(result.Kind)] = state->LifeGoal.RetryAfter;
        state->Dirty = true;
        if (result.State == Outcome::Blocked)
            ++_blocked;
    }
}

void Manager::ActionExecuted(PlayerbotAI* ai, std::string const& action, bool executed)
{
    if (!Applies(ai->GetBot()))
        return;
    static std::unordered_map<std::string, Goal> const effects = {
        {"repair", Goal::Repair},           {"sell", Goal::Sell}, {"buy", Goal::Restock}, {"trainer", Goal::Train},
        {"craft random item", Goal::Craft}, {"bank", Goal::Bank}};
    auto effect = effects.find(action);
    if (effect == effects.end())
        return;
    if (auto state = Get(ai->GetBot()->GetGUID().GetCounter()))
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.Kind != effect->second)
            return;
        GoalPlanner::ApplyResult(
            state->LifeGoal,
            {effect->second, executed ? Outcome::Started : Outcome::Blocked,
             executed ? "action executed; waiting for observed completion" : "action failed its gameplay checks"},
            Now());
        state->Failures[static_cast<std::size_t>(effect->second)] = state->LifeGoal.Failures;
        if (!executed)
            state->RetryAfter[static_cast<std::size_t>(effect->second)] = state->LifeGoal.RetryAfter;
        state->NextSnapshot = 0;
        state->Dirty = true;
    }
}

std::optional<Goal> Manager::PlannedGoal(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.State != Outcome::Succeeded && state->LifeGoal.State != Outcome::Cancelled &&
            (state->LifeGoal.State != Outcome::Blocked || Now() >= state->LifeGoal.RetryAfter))
            return state->LifeGoal.Kind;
    }
    return std::nullopt;
}

ClassProfile Manager::Profile(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->Profile;
    }
    return ClassProfile::Classic;
}

std::uint32_t Manager::OptionalHesitation(BotId id, std::uint32_t context)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto now = Now();
        if (now < state->NextHesitation)
            return 0;
        state->NextHesitation = now + 120 + state->Persona.Seed % 180;
        auto pause = state->Persona.OptionalHesitationMilliseconds(context);
        if (pause)
            ++_hesitations;
        return pause;
    }
    return 0;
}

BotPersona Manager::Persona(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->Persona;
    }
    throw std::runtime_error("ADVAI personality not loaded for character " + std::to_string(id));
}

std::string Manager::Describe(BotId id) const
{
    auto state = Get(id);
    if (!state)
        return "ADVAI character not loaded";
    std::lock_guard lock(state->Mutex);
    std::ostringstream out;
    out << "ADVAI " << id << " initialized=" << state->Life.Initialized << " active=" << Active(id)
        << " classProfile=" << Backport::Name(state->Profile) << " initialPending=" << state->InitialSetupPending
        << " initialOriginSaved=" << state->InitialOriginDurable
        << " initialTarget=" << unsigned(state->InitialTargetLevel) << " initialStarted=" << state->InitialSetupStarted
        << " initialCommit=" << state->InitialCommitRequested << " initialSaving=" << state->InitialCharacterSavePending
        << " initialRetry=" << state->InitialRetryAfter << " skill=" << unsigned(state->Persona.Skill)
        << " social=" << unsigned(state->Persona.Sociability) << " patience=" << unsigned(state->Persona.Patience)
        << " generosity=" << unsigned(state->Persona.Generosity)
        << " competitiveness=" << unsigned(state->Persona.Competitiveness)
        << " riskTolerance=" << unsigned(state->Persona.RiskTolerance)
        << " frugality=" << unsigned(state->Persona.Frugality)
        << " speechStyle=" << unsigned(state->Persona.SpeechStyle)
        << " preference=" << GoalPlanner::Name(state->Persona.Preference) << " map=" << state->Snapshot.Map
        << " zone=" << state->Snapshot.Zone << " commitment=" << state->Snapshot.HasCommitment()
        << " queued=" << state->Snapshot.InQueue << " goal=" << GoalPlanner::Name(state->LifeGoal.Kind)
        << " committedUntil=" << state->LifeGoal.CommittedUntil << " failures=" << unsigned(state->LifeGoal.Failures)
        << " reason=" << state->LifeGoal.Reason << " retry=" << state->LifeGoal.RetryAfter
        << " pvpSuggested=" << state->SuggestedPvpTarget << " pvpSuggestedUntil=" << state->SuggestedPvpUntil
        << " nextPvpSpeech=" << state->NextPvpSpeech
        << " travelHops=" << state->TravelHops << " nextTravelGesture=" << state->NextTravelGesture
        << DescribeMovement(state->Movement)
        << " partyLeader=" << state->PartyLeader << " partyActivity=" << GoalPlanner::Name(state->PartyActivity)
        << " partyWipes=" << unsigned(state->PartyWipes) << " partyHold=" << state->PartyHold
        << " partyReason=" << state->PartyReason << " break=" << state->Life.BreakDue
        << " return=" << state->Life.ReturnAt << " sessionDue=" << state->Life.SessionDue
        << " loginAfter=" << state->Life.LoginAfter << " guildSatisfaction=" << state->Life.GuildSatisfaction
        << " guildIdentity=" << state->Life.GuildIdentity << " guildIdentityKnown=" << state->Life.GuildIdentityKnown
        << " mood=" << state->Cognition.Mood.Valence << " stress=" << unsigned(state->Cognition.Mood.Stress)
        << " ambition=" << BotAmbition::Name(state->Cognition.Ambition.Kind)
        << " progress=" << state->Cognition.Ambition.Progress << '/' << state->Cognition.Ambition.Target
        << " completedAmbitions=" << state->Cognition.Ambition.Completed
        << " experiences=" << state->Cognition.Memory.Recent.size() << " contacts=" << state->Relationships.size()
        << " pendingChat=" << state->Messages.size();
    out << " interruptedGoals=" << state->InterruptedGoals.size()
        << " craftReservations=" << state->CraftReservations.size();
    for (auto const& retained : state->InterruptedGoals)
        out << " retained[goal=" << GoalPlanner::Name(retained.Kind) << ",target=" << retained.Target
            << ",retry=" << retained.RetryAfter << ",reason=" << retained.Reason << ']';
    out << " dirtyContacts=" << state->DirtyRelationships.size() << " loadingContacts=" << state->LoadingContacts.size()
        << " pendingSocialEvents=" << state->PendingRelationshipEvents.size()
        << " deferredChat=" << state->DeferredMessages.size() << " pendingSaves=" << state->PendingSaves;
    if (state->Delivery)
        out << " delivery[item=" << state->Delivery->Item << ",guid=" << state->Delivery->ItemGuid
            << ",quantity=" << state->Delivery->Quantity << ",deadline=" << state->Delivery->Deadline
            << ",overdue=" << (Now() >= state->Delivery->Deadline) << ']';
    if (state->Supplies)
        out << " supplies[food=" << state->Supplies->Food << ",drink=" << state->Supplies->Drink
            << ",ammo=" << state->Supplies->Ammo << ",petFood=" << state->Supplies->PetFood
            << ",missingTools=" << state->Supplies->Tools.size() << ",reagentKinds=" << state->Supplies->Reagents.size()
            << ",needsMount=" << state->Supplies->NeedsMount << ",mountItem=" << state->Supplies->MountItem
            << "] missingReagents=" << state->Snapshot.NeedsReagent;
    out << " topic=" << ChatRules::Name(state->Conversation.Topic) << " partner=" << state->Conversation.Partner
        << " understood=" << ChatRules::Name(state->LastUnderstood) << " confidence=" << unsigned(state->LastConfidence)
        << " revision=" << state->PersistenceRevision << " exchangeMessages=" << unsigned(state->Conversation.Messages)
        << " nextSpeak=" << state->Conversation.NextSpeak;
    if (!state->Messages.empty())
        out << " nextReply=" << ChatRules::Name(state->Messages.front().Topic)
            << " replyDue=" << state->Messages.front().Due << " thread=" << state->Messages.front().Thread;
    std::size_t recent = 0;
    for (auto event = state->Cognition.Memory.Recent.rbegin();
         event != state->Cognition.Memory.Recent.rend() && recent < 4; ++event, ++recent)
    {
        out << " experience[" << unsigned(event->Kind) << ',' << event->Other << ',' << event->Entity << ','
            << event->At << ',' << event->Positive << ']';
        if (auto relation = state->Relationships.find(event->Other); relation != state->Relationships.end())
            out << " affinity=" << relation->second.Affinity;
    }
    return out.str();
}

std::string Manager::Statistics() const
{
    auto active = _active.load();
    std::size_t resident = 0, archived = 0, archivedBytes = 0;
    {
        std::shared_lock lock(_statesMutex);
        resident = _states.size();
        archived = _archived.size();
        for (auto const& entry : _archived)
            archivedBytes += entry.second.State.size() + entry.second.Cognition.size();
    }
    return "ADVAI active=" + std::to_string(active ? active->size() : 0) +
           " accepted=" + std::to_string(_accepted.load()) + " delivered=" + std::to_string(_delivered.load()) +
           " suppressed=" + std::to_string(_suppressed.load()) + " blocked=" + std::to_string(_blocked.load()) +
           " departures=" + std::to_string(_breaks.load()) +
           " provisioned_characters=" + std::to_string(_provisionedCharacters.load()) +
           " repeated_messages=" + std::to_string(_repetitions.load()) +
           " expired_messages=" + std::to_string(_expiredMessages.load()) +
           " failed_trade_commitments=" + std::to_string(_failedTradeCommitments.load()) +
           " expired_requests=" + std::to_string(_expiredRequests.load()) +
           " optional_hesitations=" + std::to_string(_hesitations.load()) +
           " pending_identity_writes=" + std::to_string(_pendingIdentities.load()) +
           " pending_profile_writes=" + std::to_string(_socialPersistence->PendingProfiles.load()) +
           " background_budget=" + std::to_string(sPlayerbotAIConfig.AdvaiBackgroundBudget) + MovementTelemetry() + Metrics::Summary() +
           MarketStatistics() + GuildTaskStatistics() + " resident_profiles=" + std::to_string(resident) +
           " archived_profiles=" + std::to_string(archived) + " archived_bytes=" + std::to_string(archivedBytes) +
           (sPlayerbotAIConfig.AdvaiLlmEnabled ? LlmStatistics() : "");
}
}  // namespace Advai
