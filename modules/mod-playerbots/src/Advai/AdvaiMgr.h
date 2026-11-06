/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_MGR_H
#define PLAYERBOTS_ADVAI_MGR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "AdvaiCatalog.h"
#include "AdvaiCognition.h"
#include "AdvaiDialogue.h"
#include "AdvaiModel.h"

class Player;
class PlayerbotAI;

namespace Advai
{
struct SupplyPlan;
struct MovementState;
struct LlmRequest;
struct SocialReaction
{
    Intent Topic = Intent::Unknown;
    BotSocialEvent Event;
};
struct DeferredChat
{
    BotId Sender = 0;
    std::uint32_t Channel = 0;
    std::string Text;
    std::string ChannelName;
    Timestamp Expires = 0;
};
struct PendingMessage
{
    BotId Sender = 0;
    std::uint32_t Channel = 0;
    std::string ChannelName;
    std::string Text;
    Timestamp Due = 0;
    Timestamp Expires = 0;
    Intent Topic = Intent::Unknown;
    std::uint32_t Variant = 0;
    std::uint64_t Thread = 0;
    ParsedMessage Incoming;
    std::uint32_t QuestFocus = 0;
    QuestSubject QuestQuestion = QuestSubject::Progress;
    std::uint32_t ObservedEntity = 0;
    std::uint32_t ProfessionFocus = 0;
    std::string Source;
    CatalogHandle Catalog;
    std::uint32_t Locale = 0;
    std::uint64_t LlmRevision = 0;
    bool LlmInventory = false;
    // Millisecond send time (game clock). When set, the message goes out at exactly this moment, not at a whole second.
    std::uint64_t DueMs = 0;
};

struct BotState
{
    mutable std::mutex Mutex;
    BotPersona Persona;
    ClassProfile Profile = ClassProfile::Classic;
    BotCognition Cognition;
    BotLifeGoal LifeGoal;
    std::deque<BotLifeGoal> InterruptedGoals;
    Lifecycle Life;
    BotActivitySnapshot Snapshot;
    ConversationState Conversation;
    std::uint64_t LlmRevision = 0;
    BotId LlmPartner = 0;
    std::uint64_t LlmThread = 0;
    std::uint32_t LlmItemFocus = 0;
    std::uint32_t LlmSpellFocus = 0;
    Timestamp LlmFocusAt = 0;
    std::deque<std::pair<std::string, std::string>> LlmHistory;
    BotId LastInitiativePeer = 0;
    Timestamp NextPeerConversation = 0;
    std::unordered_map<BotId, Relationship> Relationships;
    std::unordered_map<BotId, Relationship> DirtyRelationships;
    std::unordered_set<BotId> LoadingContacts;
    std::deque<BotSocialEvent> PendingRelationshipEvents;
    std::deque<DeferredChat> DeferredMessages;
    bool LegacyRelationships = false;
    Timestamp ContactRetryAfter = 0;
    std::uint16_t PendingSaves = 0;
    bool InitialSetupPending = false;
    std::uint8_t InitialTargetLevel = 0;
    bool InitialOriginDurable = false;
    bool InitialSetupStarted = false;
    bool InitialCommitRequested = false;
    bool InitialCharacterSavePending = false;
    Timestamp InitialRetryAfter = 0;
    std::deque<PendingMessage> Messages;
    std::deque<BotActionRequest> Requests;
    std::deque<SocialReaction> Reactions;
    Timestamp NextReaction = 0;
    Timestamp NextPvpSpeech = 0;
    BotId SuggestedPvpTarget = 0;
    Timestamp SuggestedPvpUntil = 0;
    Timestamp NextHesitation = 0;
    Timestamp NextTravelGesture = 0;
    Timestamp NextGlance = 0;
    // Summoning duty: who asked the bot to help summon, until when, and the ritual/stone it already dealt with.
    std::uint32_t SummonFor = 0;
    Timestamp SummonUntil = 0;
    Timestamp NextSummonScan = 0;
    Timestamp NextStoneUse = 0;
    Timestamp NextOwnRitual = 0;
    std::uint64_t HandledRitual = 0;
    bool JoinHandledRitual = false;
    Timestamp NextAttention = 0;
    Timestamp HopExpires = 0;
    Goal HopGoal = Goal::None;
    std::uint8_t HopsRemaining = 0;
    std::optional<ServiceLocation> ResumeAfterHop;
    std::uint32_t TravelHops = 0;
    std::shared_ptr<MovementState> Movement;
    std::uint64_t LastCorpse = 0;
    Timestamp LastCorpseUntil = 0;
    std::optional<TradeOffer> Offer;
    std::optional<AuctionDelivery> Delivery;
    std::vector<std::uint32_t> CraftReservations;
    Timestamp NextSnapshot = 0;
    Timestamp NextDecision = 0;
    Timestamp NextAmbient = 0;
    Timestamp NextInvite = 0;
    Timestamp NextGift = 0;
    Timestamp NextTrainingCheck = 0;
    Timestamp NextSupplyCheck = 0;
    Timestamp NextPartyCheck = 0;
    BotId PartyLeader = 0;
    Goal PartyActivity = Goal::None;
    Timestamp PartyJoined = 0;
    std::uint8_t PartyWipes = 0;
    bool PartyHold = false;
    bool PartyWasWiped = false;
    bool RaidRosterReady = false;
    std::uint64_t RaidRosterSignature = 0;
    std::string PartyReason;
    std::shared_ptr<SupplyPlan const> Supplies;
    Timestamp SavedAt = 0;
    std::uint64_t PersistenceRevision = 0;
    Timestamp DepartureAfter = 0;
    std::array<Timestamp, static_cast<std::size_t>(Goal::Count)> RetryAfter{};
    std::array<std::uint8_t, static_cast<std::size_t>(Goal::Count)> Failures{};
    std::uint32_t KnownSpells = 0;
    std::uint64_t SkillFingerprint = 0;
    bool Dirty = false;
    bool AnnounceLevel = false;
    bool WasActive = false;
    std::uint8_t PetitionSignatureCount = 0;
    Timestamp NextPetitionCheck = 0;
    Intent LastUnderstood = Intent::Unknown;
    std::uint8_t LastConfidence = 0;

    void ObserveExperience(BotSocialEvent const& event);
    void RetainActivityBefore(Goal next);
    void QueueReaction(Intent topic, BotSocialEvent const& event);
    bool EnsureContact(BotId peer);
    bool ApplyRelationship(BotSocialEvent const& event);
    void TrimContacts();
    void PutContact(BotId peer, Relationship const& relationship);
};

class Manager
{
public:
    static Manager& Instance();
    ~Manager();
    void Initialize();
    void RegisterGeneratedCharacters(std::uint32_t accountOnly = 0);
    void RequestReplacementProvisioning() { _provisionRequested.store(true); }
    void UpdateWorld(std::uint32_t diff);
    void UpdateBot(PlayerbotAI* ai);
    bool Applies(Player const* player) const;
    bool Active(BotId id) const;
    bool Initialized(BotId id) const;
    bool Eligible(BotId id, Timestamp now) const;
    void MarkInitialized(Player* player);
    bool InitialSetupStarted(BotId id) const;
    std::uint8_t InitialLevel(BotId id) const;
    void OnLogout(Player* player);
    bool ReceiveChat(PlayerbotAI* ai, std::uint32_t channel, BotId sender, std::string const& text,
                     std::string const& channelName);
    bool IsConversation(std::string const& text) const;
    bool IsDeliveredSpeech(BotId sender, std::string const& text) const;
    void Observe(BotId id, BotSocialEvent const& event);
    void PvpCallout(PlayerbotAI* ai, Intent topic, BotId subject);
    BotId PvpSuggestedTarget(BotId id) const;
    std::shared_ptr<MovementState> MovementFor(BotId id);
    void ObserveCorpse(BotId id, std::uint64_t guid);
    std::uint64_t RecentCorpse(BotId id) const;
    void Flush();
    std::string Describe(BotId id) const;
    std::string Statistics() const;
    BotPersona Persona(BotId id) const;
    std::uint8_t PetitionSignatures(BotId id) const;
    bool WantsGuild(BotId id, BotId inviter) const;
    bool WantsArena(BotId id, BotId inviter) const;
    int SocialPreference(BotId id, BotId peer) const;
    ClassProfile Profile(BotId id) const;
    std::optional<Goal> PlannedGoal(BotId id) const;
    bool QueuedActivity(BotId id) const;
    void CommitObservedActivity(BotId id, Goal kind, std::string const& reason);
    void CommitParty(BotId id, BotId leader, Goal activity);
    bool CanInitiatePull(BotId id) const;
    void SetPartyReadiness(BotId id, bool hold, bool wiped, std::string const& reason);
    bool RequestContact(BotId id, BotId peer);
    std::optional<Goal> PartyPurpose(BotId id, BotId leader) const;
    bool PartyShouldLeave(BotId id) const;
    void Report(BotId id, BotActionResult const& result);
    void ActionExecuted(PlayerbotAI* ai, std::string const& action, bool executed);
    std::optional<TradeOffer> Offer(BotId id) const;
    void FinishTrade(BotId id, bool completed, std::string const& reason, TradeOffer const* expected = nullptr);
    void MailDispatched(BotId id, TradeOffer const& expected);
    bool PrepareGift(BotId giver, BotId receiver, std::uint32_t item, std::uint32_t quantity, bool managedReceiver);
    bool CanOfferGift(BotId id) const;
    bool ExpectDelivery(BotId id, AuctionDelivery delivery);
    std::optional<AuctionDelivery> Delivery(BotId id) const;
    void FinishDelivery(BotId id, std::uint32_t itemGuid, bool received);
    void TradeStatus(PlayerbotAI* ai, std::uint32_t status);
    bool ItemReserved(BotId id, std::uint32_t item) const;
    void ReserveCraft(BotId id, std::uint32_t recipe, std::vector<std::uint32_t> items);
    std::uint32_t CraftRecipe(BotId id) const;
    bool AvailableForActivity(BotId id, Goal activity) const;
    std::uint32_t OptionalHesitation(BotId id, std::uint32_t context);
    std::uint32_t RaidTarget(BotId id) const;
    void SelectRaidTarget(BotId id, std::uint32_t dungeon);
    bool RaidReady(BotId id) const;
    void SetRaidReady(BotId id, bool ready);
    std::vector<ServiceLocation> const& RaidEntrances(std::uint32_t map) const;
    std::shared_ptr<SupplyPlan const> Supplies(BotId id) const;
    std::vector<ServiceLocation> const& Services() const { return _services; }

private:
    Manager();
    struct SocialPersistence;
    std::unique_ptr<SocialPersistence> _socialPersistence;
    void ProcessSocialPersistence();
    void ArchiveOfflineProfiles(std::size_t limit = 128);
    void AdvanceInitialization(Player* bot);
    void ResumeSocialWork(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    std::shared_ptr<BotState> Get(BotId id) const;
    std::shared_ptr<BotState> Ensure(BotId id);
    BotActivitySnapshot Capture(PlayerbotAI* ai) const;
    void Save(BotId id, BotState& state);
    void Deliver(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void Plan(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void TravelMannerisms(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void SummonDuties(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void AttendToSpeaker(PlayerbotAI* ai, Player* speaker, std::shared_ptr<BotState> const& state, Timestamp now);
    void Ambient(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void ReactToEvents(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void ExecuteRequests(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now);
    void ProcessLlm();
    bool QueueLlm(PlayerbotAI* ai, Player* sender, BotState& state, std::uint32_t channel,
                  std::string const& text, std::string const& channelName, std::uint64_t thread,
                  bool initiating = false);
    void ExpireRequests(BotId id, BotState& state, Timestamp now);
    bool ReceiveUtterance(PlayerbotAI* ai, std::uint32_t channel, BotId sender, std::string const& text,
                          std::string const& channelName, bool resumed = false, bool llmResolved = false);
    bool Claim(std::string const& key, BotId responder, Timestamp now, bool direct);
    void Negotiate(PlayerbotAI* ai, Player* sender, ParsedMessage const& message, Intent& intent, BotState& state,
                   Timestamp now, std::uint32_t usage, bool direct);

    mutable std::shared_mutex _statesMutex;
    struct ArchivedProfile
    {
        Lifecycle Life;
        std::uint32_t Seed = 0;
        std::uint64_t Revision = 0;
        Timestamp SavedAt = 0;
        std::string State;
        std::string Cognition;
    };
    mutable std::unordered_map<BotId, std::shared_ptr<BotState>> _states;
    mutable std::unordered_map<BotId, ArchivedProfile> _archived;
    mutable std::deque<BotId> _archiveCandidates;
    std::vector<ServiceLocation> _services;
    std::unordered_map<std::uint32_t, std::vector<ServiceLocation>> _raidEntrances;
    std::vector<std::uint32_t> _trainers;
    std::atomic<std::shared_ptr<std::unordered_set<BotId> const>> _active;
    ActivityRoster _activityRoster;
    std::atomic<std::shared_ptr<std::unordered_set<std::uint32_t> const>> _accounts;
    std::size_t _accountCount = 0;
    std::atomic<bool> _provisionRequested{false};
    std::atomic<std::uint64_t> _provisionedCharacters{0};
    mutable std::mutex _chatMutex;
    std::unordered_map<std::string, std::pair<BotId, Timestamp>> _claims;
    std::unordered_map<std::uint64_t, std::pair<Timestamp, std::uint32_t>> _channelBudgets;
    struct Thread
    {
        Timestamp Updated = 0;
        std::uint8_t Messages = 0;
        std::unordered_set<BotId> Participants;
        std::unordered_set<std::string> Lines;
    };
    std::unordered_map<std::uint64_t, Thread> _threads;
    DeliveredSpeechLedger _utterances;
    std::uint32_t _worldElapsed = 0;
    std::atomic<std::uint64_t> _accepted{0};
    std::atomic<std::uint64_t> _suppressed{0};
    std::atomic<std::uint64_t> _blocked{0};
    std::atomic<std::uint64_t> _delivered{0};
    std::atomic<std::uint64_t> _breaks{0};
    std::atomic<std::uint64_t> _repetitions{0};
    std::atomic<std::uint64_t> _expiredMessages{0};
    std::atomic<std::uint64_t> _failedTradeCommitments{0};
    std::atomic<std::uint64_t> _expiredRequests{0};
    std::atomic<std::uint64_t> _hesitations{0};
    bool _initialized = false;
    std::deque<BotId> _identityWrites;
    std::atomic<std::size_t> _pendingIdentities{0};
    std::unordered_map<BotId, Timestamp> _backgroundWaiting;
    std::unordered_map<BotId, Timestamp> _queueWithdrawalRetry;
};
}  // namespace Advai

#endif
