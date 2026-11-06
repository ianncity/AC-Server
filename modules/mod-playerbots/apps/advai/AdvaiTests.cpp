/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "AdvaiBehavior.h"
#include "AdvaiCognition.h"
#include "AdvaiConsent.h"
#include "AdvaiDialogue.h"
#include "AdvaiMetrics.h"
#include "AdvaiModel.h"
#include "AdvaiMoney.h"
#include "AdvaiStateCodec.h"
#include "AdvaiTradeTerms.h"

namespace
{
std::size_t checks = 0;

void Check(bool value, char const* description)
{
    ++checks;
    if (!value)
        throw std::runtime_error(description);
}

void TestSocialMovementBehavior()
{
    using namespace Advai;
    Check(TurnEase(0.0f) == 0.0f && TurnEase(1.0f) == 1.0f, "turn easing reaches both endpoints");
    Check(TurnEase(-1.0f) == 0.0f && TurnEase(2.0f) == 1.0f, "turn easing remains bounded");
    float previous = 0.0f;
    for (unsigned i = 1; i <= 100; ++i)
    {
        float const current = TurnEase(static_cast<float>(i) / 100.0f);
        Check(current >= previous && current <= 1.0f, "turn easing never reverses or overshoots");
        previous = current;
    }
    Check(PeerReplyStopChance(0) == 0 && PeerReplyStopChance(2) == 0, "early peer exchanges can continue");
    Check(PeerReplyStopChance(3) == 20 && PeerReplyStopChance(6) == 80 && PeerReplyStopChance(20) == 100,
          "peer conversations progressively wind down with a bounded probability");
}

void TestChat()
{
    using namespace Advai;
    auto message =
        ChatRules::Parse("HEY! thx, actually I don't want to buy |cffa335ee|Hitem:19019:0|h[Thunderfury]|h|r");
    Check(message.Items == std::vector<std::uint32_t>{19019}, "item link identity must survive normalization");
    Check(message.Negated && message.Correction, "negation and correction must survive normalization");
    Check(std::find(message.Intents.begin(), message.Intents.end(), Intent::Buy) != message.Intents.end(),
          "compound intent");
    Check(message.Normalized.find("thanks") != std::string::npos, "slang normalization");
    Check(ChatRules::Parse("nc +grind,-follow").IsCommand, "strategy command isolation");
    Check(ChatRules::Parse("cast 1234").IsCommand, "cast command isolation");
    Check(!ChatRules::Parse("can you follow me to the inn?").IsCommand, "ordinary requests are not commands");
    Check(ChatRules::Parse("follow").IsCommand, "explicit follow remains authoritative");
    Check(!ChatRules::Parse("do you remember me?").IsCommand, "ordinary auxiliary do is not an action command");
    Check(ChatRules::Parse("do repair").IsCommand, "explicit action command remains authoritative");
    Check(!ChatRules::Parse("@Elanor where are you?").IsCommand, "named mentions remain ordinary conversation");
    Check(ChatRules::Parse("@tank follow").IsCommand, "existing role-addressed commands remain authoritative");
    Check(ChatRules::Parse("").Tokens.empty(), "empty input");
    Check(ChatRules::Parse(std::string(1025, 'a')).Tokens.empty(), "bounded input");
    Check(ChatRules::Parse("|Hitem:broken").Items.empty(), "malformed links");
    auto spellLink = ChatRules::Parse("do you know |Hspell:133|h[No Buy Fireball]|h?");
    Check(spellLink.Spells == std::vector<std::uint32_t>{133} && !spellLink.Negated,
          "spell subjects preserve IDs without treating linked names as speech");
    Check(ChatRules::Resolve(spellLink, {}, 100) == Intent::SpellKnown,
          "linked spell questions choose factual spell knowledge");
    Check(ChatRules::Resolve(ChatRules::Parse("do you have |Hitem:19019|h[Thunderfury]|h?"), {}, 100) == Intent::ItemKnown,
          "possession questions choose factual inventory knowledge");
    Check(ChatRules::Resolve(ChatRules::Parse("can I buy |Hitem:19019|h[Thunderfury]|h?"), {}, 100) == Intent::Buy,
          "item knowledge never intercepts an actual purchase negotiation");
    auto numbered = ChatRules::Parse("selling 4 |Hitem:1:0|h[Greater 500 Stone]|h for 2g");
    Check(std::find(numbered.QuantityTokens.begin(), numbered.QuantityTokens.end(), "500") ==
              numbered.QuantityTokens.end(),
          "linked names cannot become trade quantities");
    Check(ChatRules::Parse("I won't buy that").Negated, "contracted refusal");
    Check(ChatRules::Parse("I cannot join").Negated, "cannot is a refusal");
    Check(!ChatRules::Parse("help with |Hquest:1:1|h[No Rest For The Wicked]|h please").Negated,
          "negation inside a linked title is not a refusal");
    Check(ChatRules::Parse("what lvl are you?").Intents.front() == Intent::Class,
          "level question uses actual character facts");
    Check(ChatRules::Parse("where are you?").Intents.front() == Intent::Location, "location recognition");
    auto requests = ChatRules::Requests("Where are you? What level are you?");
    Check(requests.size() == 2 && ChatRules::Parse(requests[0]).Question && ChatRules::Parse(requests[1]).Question,
          "multiple private questions retain independent question semantics");
    requests = ChatRules::Requests("Don't invite me but where are you?");
    Check(requests.size() == 2 && ChatRules::Parse(requests[0]).Negated && !ChatRules::Parse(requests[1]).Negated,
          "negation is scoped to its own request");
    Check(ChatRules::Requests("cast 1234; where are you?").size() == 1,
          "compound chat cannot reinterpret an authoritative command");
    requests = ChatRules::Requests("Can you sell |Hitem:1:0|h[What? No! And Where]|h for 1.5g? Where are you?");
    Check(requests.size() == 2 && ChatRules::Parse(requests[0]).Items == std::vector<std::uint32_t>{1},
          "linked punctuation and decimal prices cannot split requests");
    Check(ChatRules::Parse("hello |Hitem:1:0|h[Actually No Quest]|h").Intents == std::vector<Intent>{Intent::Greeting},
          "linked names do not create unrelated conversational intents or corrections");
    ConversationState conversation;
    conversation.Topic = Intent::Quest;
    conversation.LastMessage = 100;
    Check(ChatRules::Resolve(ChatRules::Parse("what about that?"), conversation, 101) == Intent::Quest,
          "follow-up context");
    Check(ChatRules::Resolve(ChatRules::Parse("what about that?"), conversation, 221) == Intent::Clarify,
          "expired context");
    Check(ChatRules::Resolve(ChatRules::Parse("hi, where are you?"), conversation, 101) == Intent::Location,
          "specific intent wins");
    auto mining = ChatRules::Parse("What level is your mining?");
    Check(mining.Focus == Intent::Profession && mining.Confidence >= 90,
          "profession subject outranks an incidental character-level keyword");
    Check(ChatRules::Resolve(ChatRules::Parse("what level is your mining?"), conversation, 101) == Intent::Profession,
          "profession question does not produce unrelated character-level facts");
    Check(ChatRules::Resolve(ChatRules::Parse("hi how are you"), conversation, 101) == Intent::SmallTalkAnswer,
          "ordinary welfare questions work without punctuation");
    Check(ChatRules::Parse("where are you").Question, "interrogative grammar supplies omitted question punctuation");
    Check(ChatRules::Resolve(ChatRules::Parse("where is the dungeon?"), conversation, 101) == Intent::Clarify,
          "ambiguous directions do not assert the bot's own location as a destination");
    Check(ChatRules::Resolve(ChatRules::Parse("actually what about that?"), conversation, 101) == Intent::Clarify,
          "corrections do not inherit an old implicit subject");
    Check(ChatRules::Resolve(ChatRules::Parse("anyway what about that?"), conversation, 101) == Intent::Clarify,
          "topic changes clear implicit follow-up subjects");
    Check(ChatRules::Parse("Can you invite me?").Act == SpeechAct::Request,
          "requests are represented separately from topical recognition");
    Check(ChatRules::Parse("I don't want an invite").Act == SpeechAct::Refusal, "refusals retain their speech act");
    Check(!ChatRules::Parse("I can't wait to join the raid").Negated,
          "anticipation idioms are not treated as refusals");
    conversation.NextSpeak = 105;
    Check(!conversation.CanReply(104, 2, "hi"), "typing delay");
    conversation.Messages = 6;
    conversation.Partner = 2;
    Check(!conversation.CanReply(105, 2, "new"), "conversation length limit");
    Check(conversation.CanReply(221, 2, "new"), "new exchange after expiry");
    conversation.QuestFocus = 123;
    conversation.Topic = Intent::QuestKnown;
    auto stranger = conversation.ForPartner(3, 110);
    Check(stranger.Topic == Intent::Unknown && !stranger.QuestFocus,
          "another speaker cannot inherit the previous partner's quest reference");
    Check(conversation.ForPartner(2, 110).QuestFocus == 123, "same-partner follow-up retains the quest reference");
    Check(!conversation.ForPartner(2, 221).QuestFocus, "expired exchanges discard quest references");
    Check(
        ChatRules::QuestTopic(ChatRules::Parse("what level is |Hquest:1:1|h[Where Are You]|h?")) == QuestSubject::Level,
        "linked titles cannot change the requested quest subject");
    Check(ChatRules::QuestTopic(ChatRules::Parse("where is that quest?")) == QuestSubject::Directions,
          "quest directions are distinct from personal progress");
    BotActivitySnapshot snapshot;
    snapshot.Name = "Test";
    snapshot.Level = 20;
    snapshot.ClassName = "mage";
    snapshot.ZoneName = "Westfall";
    Check(ChatRules::Facts(Intent::Location, snapshot, {}) == "Westfall.", "location derived from actual snapshot");
    Check(ChatRules::Facts(Intent::Quest, snapshot, {}).empty(), "never invent a quest");
    for (std::size_t i = 0; i < static_cast<std::size_t>(Intent::Count); ++i)
        Check(std::string(ChatRules::Name(static_cast<Intent>(i))) != "invalid", "all intent names registered");
}

void TestPlanner()
{
    using namespace Advai;
    auto persona = BotPersona::Create(123);
    Check(persona.Skill == BotPersona::Create(123).Skill, "stable personality");
    auto identity = BotPersona::ForCharacter(42);
    Check(identity.Seed == BotPersona::ForCharacter(42).Seed && identity.Seed != BotPersona::ForCharacter(43).Seed,
          "offline generated identities survive restart before their first database write");
    Check(persona.ReactionMilliseconds() >= 250, "human reaction delay");
    BotActivitySnapshot s;
    s.Now = 1000;
    s.Alive = false;
    Check(s.HasCommitment(), "death recovery prevents background suspension");
    Check(s.RequiresContinuousActivity(), "death recovery requires active safety handling");
    s.Durability = 0;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Recover, "death overrides maintenance");
    s.Alive = true;
    s.InCombat = true;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Survive, "combat overrides maintenance");
    s.InCombat = false;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Repair, "broken equipment");
    s.Durability = 100;
    s.FreeBagSlots = 0;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Sell, "full bags");
    s.FreeBagSlots = 16;
    s.NeedsFood = true;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Quest, "shortage creates acquisition goal without money");
    s.HasVendorStock = true;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Sell, "cash shortages first sell owned vendor goods");
    s.HasVendorStock = false;
    s.RetryAfter[static_cast<std::size_t>(Goal::Quest)] = 2000;
    s.CanGather = true;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Gather,
          "blocked quest income respects retry state while acquiring resources");
    s.CanGather = false;
    s.RetryAfter[static_cast<std::size_t>(Goal::Quest)] = 0;
    s.Money = 100;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Restock, "supplies require purchasing capacity");
    s.NeedsFood = false;
    s.NeedsPetFood = true;
    Check(GoalPlanner::Choose(s, persona, {}) == Goal::Restock, "hunter pet food is a real supply obligation");
    s.NeedsPetFood = false;
    BotLifeGoal current;
    current.Kind = Goal::Gather;
    current.State = Outcome::Started;
    current.CommittedUntil = 1100;
    s.InBattleground = true;
    s.Durability = 0;
    Check(s.HasCommitment() && GoalPlanner::Choose(s, persona, current) == Goal::Gather,
          "battleground participation prevents service detours and suspension");
    Check(ChatRules::Facts(Intent::Activity, s, current) == "playing a battleground.",
          "actual battleground participation overrides an old discretionary goal");
    s.InBattleground = false;
    s.InArena = true;
    Check(s.HasCommitment() && ChatRules::Facts(Intent::Activity, s, current) == "playing an arena match.",
          "actual arena participation is reported truthfully");
    s.InArena = false;
    s.Durability = 100;
    Check(GoalPlanner::Choose(s, persona, current) == Goal::Gather, "goal switching costs");
    s.AwaitingDelivery = true;
    Check(GoalPlanner::Choose(s, persona, current) == Goal::Mail,
          "a purchased ingredient creates a mailbox prerequisite before discretionary crafting");
    s.AwaitingDelivery = false;
    GoalPlanner::ApplyResult(current, {Goal::Gather, Outcome::Blocked, "node unreachable"}, 1000);
    Check(current.Failures == 1 && current.RetryAfter > 1000, "bounded retry after real failure");
    Check(GoalPlanner::Choose(s, persona, current) != Goal::Gather, "failed goal not retried immediately");
    for (int i = 0; i < 300; ++i)
        GoalPlanner::ApplyResult(current, {Goal::Gather, Outcome::Blocked, "node unreachable"}, 1000);
    Check(current.Failures <= 8 && current.RetryAfter <= 1600, "retry and failure counter cannot overflow");
    s.HumanGroup = true;
    Check(GoalPlanner::Choose(s, persona, current) == Goal::Gather, "human commitment preserved");
    BotActivitySnapshot preparation;
    preparation.Now = 1000;
    preparation.Health = 65;
    auto cautious = persona;
    cautious.Preference = Goal::Quest;
    cautious.RiskTolerance = 0;
    auto daring = cautious;
    daring.RiskTolerance = 100;
    Check(GoalPlanner::Choose(preparation, cautious, {}) == Goal::Rest &&
              GoalPlanner::Choose(preparation, daring, {}) == Goal::Quest,
          "stable risk tolerance changes preparation without overriding immediate survival");
    preparation.Health = 100;
    preparation.UsesMana = true;
    preparation.NeedsDrink = false;
    preparation.Mana = 0;
    Check(GoalPlanner::Choose(preparation, cautious, {}) == Goal::Rest,
          "having drinks in inventory cannot hide an actual mana shortage");
    preparation.UsesMana = false;
    Check(GoalPlanner::Choose(preparation, cautious, {}) == Goal::Quest,
          "non-mana classes do not acquire artificial mana recovery goals");
}

void TestLifecycle()
{
    using namespace Advai;
    Check(Lifecycle::RecommendedReserveCharacters(2000) == 32000,
          "long breaks require character capacity beyond online slots");
    Check(Lifecycle::RecommendedReserveCharacters(0) == 0, "disabled population has no reserve demand");
    Lifecycle life;
    life.Initialized = true;
    life.Start(100, false, 0);
    Check(life.BreakDue == 100 + 2 * 86400, "ordinary tenure");
    life.UpdateGuild(200, true, 7);
    Check(life.BreakDue == 200 + 14 * 86400, "guild tenure extended on joining");
    auto longDeadline = life.BreakDue;
    BotActivitySnapshot pause;
    pause.Now = life.SessionDue;
    Check(life.CanPause(pause), "short session ends at safe point");
    pause.InInstance = true;
    Check(!life.CanPause(pause), "instance commitment defers short logout");
    life.BeginPause(pause.Now, 1000);
    Check(!life.Eligible(pause.Now) && life.Eligible(life.LoginAfter), "short pause persists login deadline");
    life.StartSession(life.LoginAfter, 4000);
    Check(life.BreakDue == longDeadline && !life.LoginAfter, "short session does not reset long tenure");
    BotActivitySnapshot s;
    s.Now = life.BreakDue;
    s.InCombat = true;
    Check(!life.CanDepart(s), "no mid-fight departure");
    s.InCombat = false;
    s.Trading = true;
    Check(!life.CanDepart(s), "no mid-transaction departure");
    s.Trading = false;
    Check(life.CanDepart(s), "safe departure");
    life.BeginBreak(s.Now, 16);
    Check(life.ReturnAt == s.Now + 30 * 86400, "long break upper bound");
    Check(!life.Eligible(s.Now) && life.Eligible(life.ReturnAt), "return eligibility");
    life.GuildSatisfaction = -30;
    Check(life.ShouldLeaveGuild(), "sustained dissatisfaction permits departure");
    life.GuildSatisfaction = 10;
    Check(!life.ShouldLeaveGuild(), "satisfied guild members retain membership");
    Relationship relationship;
    relationship.Observe({SocialEventKind::Assistance, 2, 1000, true});
    relationship.Observe({SocialEventKind::Assistance, 2, 1001, true});
    Check(relationship.Affinity == 5, "relationship reward rate limit");
}

void TestPopulation()
{
    using namespace Advai;
    std::vector<ActivityMember> members;
    auto occupied = ActivityRoster::ZoneKey(0, 12);
    auto empty = ActivityRoster::ZoneKey(1, 12);
    for (BotId id = 1; id <= 2000; ++id)
        members.push_back({id, id <= 200 ? occupied : empty, 0, false});
    auto first = ActivityRoster::Select(members, {occupied}, 100, 0);
    Check(first.size() == 300, "200 human-zone plus 100 distant bots");
    for (BotId id = 1; id <= 200; ++id)
        Check(first.count(id) == 1, "all bots in human zone active");
    Check(!first.count(401), "same zone number on another map does not activate bots");
    auto second = ActivityRoster::Select(members, {occupied}, 100, 1);
    Check(second.size() == 300 && second != first, "background roster rotates");
    std::unordered_set<BotId> seen;
    for (std::uint64_t epoch = 0; epoch < 18; ++epoch)
    {
        auto roster = ActivityRoster::Select(members, {occupied}, 100, epoch);
        seen.insert(roster.begin(), roster.end());
    }
    Check(seen.size() == 2000, "rotation cannot starve distant solo bots");
    members[1999].Committed = true;
    Check(ActivityRoster::Select(members, {}, 0, 0).count(2000) == 1, "commitments outlive background budget");
    members[0].Group = 1;
    members[1].Group = 1;
    members[1].Zone = empty;
    Check(ActivityRoster::Select(members, {occupied}, 0, 0).count(2) == 1, "group members scheduled as a unit");
    Check(ActivityRoster::Select({}, {}, 100, 0).empty(), "empty population");
    auto distinctUnits =
        ActivityRoster::Select({{1, occupied, 0, false}, {2, empty, 0x80000001U, false}}, {occupied}, 0, 0);
    Check(distinctUnits.size() == 1 && distinctUnits.count(1),
          "a high group GUID cannot collide with a solo character's scheduling unit");
    members.clear();
    for (BotId id = 1; id <= 500; ++id)
        members.push_back({id, empty, (id - 1) / 5 + 1, false});
    seen.clear();
    for (std::uint64_t epoch = 0; epoch < 5; ++epoch)
    {
        auto roster = ActivityRoster::Select(members, {}, 100, epoch);
        Check(roster.size() == 100, "party-unit rotation respects population budget");
        seen.insert(roster.begin(), roster.end());
    }
    Check(seen.size() == 500, "party units cannot starve when unit count divides the background budget");

    members.clear();
    for (BotId id = 1; id <= 2000; ++id)
        members.push_back({id, empty, 0, false});
    ActivityRoster stable;
    auto baseline = stable.Update(members, {}, 100, 6000000, 40);
    for (BotId id = 1; id <= 2000; ++id)
    {
        members[id - 1].Committed = true;
        auto during = stable.Update(members, {}, 100, 6000000, 41);
        auto expected = baseline;
        expected.insert(id);
        Check(during == expected, "a new commitment cannot suspend unrelated background bots");
        members[id - 1].Committed = false;
        Check(stable.Update(members, {}, 100, 6000000, 42) == baseline,
              "ending combat restores the same background roster");
    }
    Check(stable.Update(members, {}, 100, 6000001, 40) != baseline, "stable roster still rotates");
    Check(stable.Update(members, {}, 0, 6000001, 40).empty(), "budget reload takes effect immediately");
    ActivityRoster fair;
    seen.clear();
    for (std::uint64_t epoch = 0; epoch < 20; ++epoch)
    {
        auto roster = fair.Update(members, {}, 100, epoch, 40);
        seen.insert(roster.begin(), roster.end());
    }
    Check(seen.size() == members.size(), "stable rotation gives every solo bot a turn");
    ActivityRoster startup;
    Check(startup.Update({}, {}, 100, 1, 0).empty(), "startup without logged-in bots is empty");
    Check(startup.Update(members, {}, 100, 1, 1).size() == 100,
          "logins fill empty background slots without waiting for rotation");
    auto loggedIn = startup.Update(members, {}, 100, 1, 2);
    auto departed = *loggedIn.begin();
    std::erase_if(members, [departed](auto const& member) { return member.Id == departed; });
    auto replenished = startup.Update(members, {}, 100, 1, 3);
    loggedIn.erase(departed);
    Check(replenished.size() == 100 && !replenished.contains(departed), "logout vacancies are filled");
    for (auto id : loggedIn)
        Check(replenished.contains(id), "replacing a departed bot preserves the other activity slots");
}

void TestTradeTerms()
{
    using namespace Advai;
    TradeOffer offer{2, 123, 5, 1000, 5000, true, true};
    TradeContents seller, buyer;
    seller.Items[0] = {123, 5};
    buyer.Copper = 1000;
    Check(MatchesTrade(offer, seller, buyer), "exact trade terms accepted");
    seller.Items[1] = {456, 1};
    Check(!MatchesTrade(offer, seller, buyer), "never give unquoted items");
    seller.Items[1] = {};
    seller.Copper = 1;
    Check(!MatchesTrade(offer, seller, buyer), "never give unquoted money");
    seller.Copper = 0;
    buyer.Copper = 999;
    Check(!MatchesTrade(offer, seller, buyer), "never accept an underpayment");
    buyer.Copper = 1000;
    seller.Items[0].second = 4;
    Check(!MatchesTrade(offer, seller, buyer), "never accept insufficient goods");
    TradeOffer gift{2, 123, 5, 0, 5000, true, true};
    seller = {};
    buyer = {};
    seller.Items[0] = {123, 5};
    Check(MatchesTrade(gift, seller, buyer), "a supply gift completes through the same exact cart contract");
    buyer.Copper = 1;
    Check(!MatchesTrade(gift, seller, buyer), "a gift cannot silently turn into a paid transaction");
    buyer.Copper = 0;
    buyer.Items[0] = {456, 1};
    Check(!MatchesTrade(gift, seller, buyer), "a supply gift cannot take an unagreed return item");
    auto price = ParsePrice(ChatRules::Parse("selling 4 |Hitem:123|h[500 Gold Ore]|h for 2.5g 20s").QuantityTokens);
    Check(price.Found && price.Valid && price.Copper == 27000,
          "price parsed in ordinary speech without reading link names");
    Check(!ParsePrice({"2g", "or", "3g"}).Valid, "ambiguous alternative prices rejected");
    Check(!ParsePrice({"-10g"}).Valid, "negative price rejected");
    Check(!ParsePrice({"999999999999999999999g"}).Valid, "price overflow rejected");
    Check(!ParsePrice({"0.1c"}).Valid, "fractional copper rejected");
    Check(ParsePrice({"5", "gold", "10", "silver"}).Copper == 51000, "currency words supported");
}

void TestCognition()
{
    using namespace Advai;
    BotPersona persona = BotPersona::Create(71);
    persona.Preference = Goal::Quest;
    BotCognition cognition;
    cognition.Observe({SocialEventKind::Death, 0, 1000, false}, persona);
    Check(cognition.Mood.Valence < 0 && cognition.Mood.Stress == 12, "death creates temporary stress");
    auto previousMood = cognition.Mood.Valence;
    cognition.Observe({SocialEventKind::Death, 0, 1001, false}, persona);
    Check(cognition.Mood.Valence == previousMood, "duplicate events cannot amplify mood");
    cognition.Observe({SocialEventKind::Death, 25, 1002, true}, persona);
    Check(cognition.Mood.Valence == previousMood, "group death context does not multiply personal mood");
    Check(cognition.Memory.Negative[static_cast<std::size_t>(SocialEventKind::Death)] == 1,
          "durable summary counts actual distinct events");
    cognition.Observe({SocialEventKind::SharedQuest, 25, 1010, true, 1}, persona);
    cognition.Observe({SocialEventKind::SharedQuest, 25, 1010, true, 2}, persona);
    Check(cognition.Memory.Positive[static_cast<std::size_t>(SocialEventKind::SharedQuest)] == 2,
          "different quests completed together remain distinct experiences");
    cognition.Mood.Settle(100000);
    Check(cognition.Mood.Valence == 0 && cognition.Mood.Stress == 0, "mood settles after a long break");
    auto settled = cognition.Mood.Updated;
    cognition.Mood.Settle(100);
    Check(cognition.Mood.Updated == settled, "backward clock cannot amplify mood");
    for (BotId peer = 1; peer <= 100; ++peer)
        cognition.Observe({SocialEventKind::Assistance, peer, 100000 + peer * 61, true}, persona);
    Check(cognition.Memory.Recent.size() == MAX_EXPERIENCES, "recent social experiences remain bounded");
    Check(cognition.Memory.Positive[static_cast<std::size_t>(SocialEventKind::Assistance)] == 100,
          "durable summary retains experiences beyond the working window");
    BotActivitySnapshot snapshot;
    snapshot.Now = 110000;
    snapshot.Level = 31;
    cognition.Ambition.Update(snapshot, persona, 0);
    Check(cognition.Ambition.Kind == AmbitionKind::ReachLevel && cognition.Ambition.Target == 36,
          "level ambition starts from actual seeded level");
    snapshot.Now += 86400;
    cognition.Ambition.Update(snapshot, persona, 0);
    Check(cognition.Ambition.Progress == 31 && !cognition.Ambition.CompletedAt,
          "elapsed time awards no level progress");
    snapshot.Level = 36;
    cognition.Ambition.Update(snapshot, persona, 0);
    Check(cognition.Ambition.Completed == 1 && cognition.Ambition.CompletedAt == snapshot.Now,
          "ambition completion requires observed level change");
    auto encoded = cognition.Serialize();
    auto loaded = BotCognition::Deserialize(encoded);
    Check(loaded.Serialize() == encoded, "mood ambitions and experience summaries round-trip exactly");
    bool rejected = false;
    try
    {
        BotCognition::Deserialize(encoded + " invalid");
    }
    catch (std::runtime_error const&)
    {
        rejected = true;
    }
    Check(rejected, "corrupt persistent cognition fails explicitly");
    rejected = false;
    try
    {
        BotCognition::Deserialize(encoded.substr(0, encoded.size() / 2));
    }
    catch (std::runtime_error const&)
    {
        rejected = true;
    }
    Check(rejected, "truncated persistent cognition fails explicitly");
    persona.Preference = Goal::Craft;
    snapshot.ProfessionSkill = 140;
    snapshot.ProfessionActivity = Goal::Craft;
    snapshot.CanCraft = true;
    BotAmbition profession;
    profession.Update(snapshot, persona, 0);
    Check(profession.Kind == AmbitionKind::DevelopProfession && profession.Target == 165,
          "profession ambition starts from actual skill");
    auto activity = profession.Activity(snapshot, persona);
    Check(activity == Goal::Craft, "profession ambition selects normal crafting actions");
    snapshot.Durability = 1;
    Check(GoalPlanner::Choose(snapshot, persona, {}, activity) == Goal::Repair,
          "maintenance takes priority over long-term ambitions");
    snapshot.Durability = 100;
    snapshot.HasQuest = true;
    Check(GoalPlanner::Choose(snapshot, persona, {}, activity) == Goal::Craft,
          "ambitions influence discretionary activity");
    snapshot.RetryAfter[static_cast<std::size_t>(Goal::Craft)] = snapshot.Now + 100;
    Check(GoalPlanner::Choose(snapshot, persona, {}, activity) != Goal::Craft,
          "ambitions cannot bypass blocked-action backoff");
    persona.Preference = Goal::Trade;
    snapshot.Money = 5000;
    BotAmbition savings;
    savings.Update(snapshot, persona, 0);
    snapshot.Money = 4000;
    savings.Update(snapshot, persona, 0);
    Check(savings.Progress == 4000 && !savings.CompletedAt, "spending reduces actual savings progress");
    persona.Preference = Goal::Arena;
    Check(savings.Activity(snapshot, persona) == Goal::None, "savings ambitions preserve preferred endgame activities");
}

void TestDialogue()
{
    using namespace Advai;
    DeliveredSpeechLedger delivered;
    delivered.Record(7, {11, "premier message", Intent::Class, "my class", 200});
    delivered.Record(7, {12, "second message", Intent::Location, "my location", 200});
    Check(delivered.Find(7, "premier message", 100)->Source == "my class",
          "delayed packet processing keeps annotations for older delivered lines");
    Check(!delivered.Find(8, "premier message", 100), "another sender cannot reuse a speech annotation");
    Check(!delivered.Find(7, "modified premier message", 100), "annotations require the exact delivered text");
    delivered.Revoke(7, 12, "second message");
    Check(!delivered.Find(7, "second message", 100) && delivered.Find(7, "premier message", 100),
          "a failed send revokes only its own annotation");
    for (std::uint32_t i = 0; i < 12; ++i)
        delivered.Record(7, {i, std::to_string(i), Intent::Greeting, "hello", 200});
    Check(!delivered.Find(7, "0", 100) && delivered.Find(7, "11", 100),
          "delivered speech history is bounded per sender");
    Check(!delivered.Find(7, "11", 200), "expired annotations cannot reinterpret later speech");
    delivered.Prune(200);
    Check(!delivered.Find(7, "11", 100), "pruning removes expired speech rather than retaining its allocation");
    Check(AcceptsInvitation(ChatRules::Parse("yes, please").Normalized), "clear invitation consent is recognized");
    Check(AcceptsInvitation(ChatRules::Parse("why not?").Normalized), "idiomatic consent is not a refusal");
    Check(!AcceptsInvitation(ChatRules::Parse("yes but not now").Normalized),
          "qualified answers cannot authorize an invitation");
    Check(DeclinesInvitation(ChatRules::Parse("nah, thanks").Normalized), "polite refusal is retained");
    Check(DeclinesInvitation(ChatRules::Parse("maybe later").Normalized), "postponement is not consent");
    BotPersona persona = BotPersona::Create(81);
    BotMood mood;
    DialogueContext context;
    auto familiar = SpeechTraits::Analyze("advai_greeting", "Nice to see you again!");
    Check(familiar.Weight(persona, mood, context) == 0, "familiar greeting requires an actual prior encounter");
    context.KnownPartner = true;
    Check(familiar.Weight(persona, mood, context) > 0, "known partners permit familiar greeting");
    context.Affinity = -50;
    Check(familiar.Weight(persona, mood, context) == 0, "negative relationships prevent warm familiar greetings");
    context.Affinity = 20;
    mood.Stress = 90;
    Check(familiar.Weight(persona, mood, context) == 0, "high stress suppresses cheerful exclamations");
    mood.Stress = 0;
    auto collective = SpeechTraits::Analyze("advai_greeting", "Hey everyone.");
    Check(collective.Weight(persona, mood, context) == 0, "private reply cannot address an imaginary crowd");
    context.PublicAudience = true;
    Check(collective.Weight(persona, mood, context) > 0, "public speech permits a collective audience");
    auto slang = SpeechTraits::Analyze("advai_greeting", "Yo, what's going on?");
    persona.SpeechStyle = 95;
    Check(slang.Weight(persona, mood, context) == 0, "formal persona excludes casual slang");
    persona.SpeechStyle = 10;
    Check(slang.Weight(persona, mood, context) > 0, "casual persona permits slang");
    auto brief = SpeechTraits::Analyze("advai_greeting", "Hello there.");
    auto lengthy =
        SpeechTraits::Analyze("advai_greeting", "Hello there, how have things been going for you this evening?");
    Check(brief.Weight(persona, mood, context) > lengthy.Weight(persona, mood, context),
          "concise persona consistently weights short speech");
    Check(brief.Id == SpeechTraits::Analyze("advai_greeting", "Hello there.").Id,
          "speech identity persists independently of database ordering");
    Check(brief.Id != SpeechTraits::Analyze("advai_farewell", "Hello there.").Id, "speech identity includes outcome");
    std::vector<SpeechTraits> profiles;
    for (unsigned i = 0; i < 36; ++i)
        profiles.push_back(
            SpeechTraits::Analyze("advai_unknown", "Could you explain topic " + std::to_string(i) + "?"));
    std::deque<std::uint32_t> recent;
    for (unsigned i = 0; i < 100; ++i)
    {
        auto chosen = SelectSpeech(profiles, persona, mood, context, recent, i * 3517);
        Check(recent.empty() || recent.back() != profiles[chosen].Id,
              "speech excludes the previous line even after every variant has been used");
        recent.push_back(profiles[chosen].Id);
        if (recent.size() > MAX_RECENT_LINES)
            recent.pop_front();
    }
    BotActivitySnapshot snapshot;
    BotLifeGoal activity;
    activity.Kind = Goal::Battleground;
    Check(ChatRules::Facts(Intent::Activity, snapshot, activity).find("join") != std::string::npos,
          "planning PvP cannot claim participation in a match");
    snapshot.InBattleground = true;
    Check(ChatRules::Facts(Intent::Activity, snapshot, activity).find("playing") != std::string::npos,
          "actual battleground participation can be reported");
}

void TestPersistentState()
{
    using namespace Advai;
    BotState original;
    original.Persona = BotPersona::ForCharacter(42);
    original.Profile = ClassProfile::Necromancer;
    original.Life.Initialized = true;
    original.Life.Joined = 10;
    original.Life.BreakDue = 200000;
    original.Life.ReturnAt = 1600000;
    original.LifeGoal = {Goal::Craft, 100, 300, 250, 77, 2, Outcome::Blocked, "missing reagent"};
    original.Relationships.emplace(7, Relationship{25, 4, 123, 120});
    original.Conversation.Partner = 7;
    original.Conversation.Topic = Intent::QuestKnown;
    original.Conversation.QuestFocus = 123;
    original.Conversation.ItemFocus = 19019;
    original.Conversation.SpellFocus = 133;
    original.Conversation.Messages = 2;
    original.Conversation.RecentVariants = {1, 2};
    original.Delivery = AuctionDelivery{77, 88, 5, 100, 1000};
    original.PartyLeader = 7;
    original.PartyActivity = Goal::Dungeon;
    original.PartyJoined = 90;
    original.PartyWipes = 2;
    original.Relationships.at(7).Revision = 44;
    original.PendingRelationshipEvents.push_back({SocialEventKind::Assistance, 8, 120, true, 99});
    auto payload = StateCodec::Serialize(original);
    BotState restored;
    restored.Persona = original.Persona;
    StateCodec::Deserialize(payload, restored, 500);
    Check(restored.Life.ReturnAt == original.Life.ReturnAt && restored.Life.BreakDue == original.Life.BreakDue,
          "calendar lifecycle deadlines survive restart without recalculation");
    Check(restored.LifeGoal.Target == 77 && restored.LifeGoal.RetryAfter == 250 && restored.LifeGoal.Failures == 2,
          "goal targets and bounded retry state survive restart");
    Check(restored.Conversation.QuestFocus == 123 && restored.Conversation.Partner == 7,
          "durable conversational references survive restart with their original partner");
    Check(restored.Conversation.ItemFocus == 19019 && restored.Conversation.SpellFocus == 133,
          "specific item and spell references survive restart");
    Check(!restored.Conversation.ForPartner(9, 501).ItemFocus &&
              !restored.Conversation.ForPartner(9, 501).SpellFocus,
          "entity followups never leak across conversation partners");
    Check(restored.Delivery && restored.Delivery->ItemGuid == 77 && restored.Delivery->Quantity == 5,
          "outstanding auction receipts survive restart without being reordered");
    Check(restored.Relationships.at(7).Affinity == 25, "durable relationships survive restart");
    Check(restored.Profile == ClassProfile::Necromancer, "custom class identity survives returning sessions");
    Check(restored.Relationships.at(7).Revision == 44 && restored.PendingRelationshipEvents.size() == 1 &&
              restored.PendingRelationshipEvents.front().Other == 8,
          "relationship revisions and observations awaiting hydration survive restart");
    Check(restored.PartyLeader == 7 && restored.PartyActivity == Goal::Dungeon && restored.PartyJoined == 90 &&
              restored.PartyWipes == 2,
          "accepted party purpose and recovery history survive restart");
    original.Conversation.QuestFocus = 0;
    original.Delivery.reset();
    original.Relationships.clear();
    original.PendingRelationshipEvents.clear();
    auto legacy = StateCodec::Serialize(original);
    legacy.resize(legacy.rfind(" 7 "));
    auto third = legacy;
    third.replace(0, third.find(' '), "3");
    BotState previous;
    StateCodec::Deserialize(third, previous, 500);
    Check(!previous.PartyLeader && previous.LifeGoal.Target == 77,
          "version three profiles load without inventing an accepted party commitment");
    legacy.replace(0, legacy.find(' '), "2");
    legacy.resize(legacy.rfind(" 0 0"));
    BotState old;
    old.Persona = original.Persona;
    StateCodec::Deserialize(legacy, old, 500);
    Check(old.Life.ReturnAt == original.Life.ReturnAt && !old.Delivery && !old.Conversation.QuestFocus,
          "version two profiles retain progress when loading the new state format");
    bool rejected = false;
    try
    {
        BotState broken;
        StateCodec::Deserialize(payload + " unexpected", broken, 500);
    }
    catch (std::runtime_error const&)
    {
        rejected = true;
    }
    Check(rejected, "trailing persistent corruption fails explicitly");
    BotState pending;
    pending.InitialSetupPending = true;
    pending.InitialTargetLevel = 37;
    pending.Profile = ClassProfile::Cultist;
    BotState resumed;
    StateCodec::Deserialize(StateCodec::Serialize(pending), resumed, 500);
    Check(resumed.InitialOriginDurable && resumed.InitialTargetLevel == 37 &&
              resumed.Profile == ClassProfile::Cultist && !resumed.Life.Initialized,
          "unfinished initial setup retains its original level and class across a crash");
    pending.Life.Initialized = true;
    rejected = false;
    try
    {
        BotState contradictory;
        StateCodec::Deserialize(StateCodec::Serialize(pending), contradictory, 500);
    }
    catch (std::runtime_error const&)
    {
        rejected = true;
    }
    Check(rejected, "returning characters cannot carry a pending resource reseed");
}

void TestCatalogEligibility()
{
    using namespace Advai;
    std::ifstream input(ADVAI_ENGLISH_CATALOG);
    Check(input.is_open(), "authored English catalog is available");
    std::unordered_map<std::string, std::vector<SpeechTraits>> catalog;
    std::string line, outcome;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.front() == '#')
            continue;
        if (line.front() == '[' && line.back() == ']')
            outcome = line.substr(1, line.size() - 2);
        else
            catalog[outcome].push_back(SpeechTraits::Analyze("advai_" + outcome, line));
    }
    BotPersona formal;
    formal.SpeechStyle = 100;
    BotMood stressed;
    stressed.Valence = -100;
    stressed.Stress = 100;
    for (unsigned value = 0; value < unsigned(Intent::Count); ++value)
    {
        auto intent = static_cast<Intent>(value);
        auto profiles = catalog.find(ChatRules::Name(intent));
        Check(profiles != catalog.end() && profiles->second.size() >= MIN_TEXT_VARIANTS,
              "every runtime outcome has the required authored source");
        DialogueContext context;
        context.KnownPartner = intent == Intent::FriendRemembered;
        context.PublicAudience = intent == Intent::Progress || intent == Intent::GroupOffer;
        auto selected = SelectSpeech(profiles->second, formal, stressed, context, {}, 0);
        Check(selected < profiles->second.size(), "every outcome has speech for the most restricted relevant context");
        if (intent == Intent::GroupOffer)
        {
            context.PublicAudience = false;
            context.KnownPartner = true;
            selected = SelectSpeech(profiles->second, formal, stressed, context, {}, 0);
            Check(selected < profiles->second.size(), "questing proposals have eligible private phrasing for a friend");
        }
    }
}

void TestMetrics()
{
    using namespace Advai;
    for (int i = 0; i < 100; ++i)
        Metrics::WorldTick(i < 95 ? 50 : i < 99 ? 100 : 200);
    auto summary = Metrics::Summary();
    Check(summary.find("world_p95_ms=50") != std::string::npos, "world p95 uses nearest-rank percentile");
    Check(summary.find("world_p99_ms=100") != std::string::npos, "world p99 preserves tail frames");
    for (int i = 0; i < 5000; ++i)
        Metrics::WorldTick(60);
    summary = Metrics::Summary();
    Check(summary.find("world_samples=4096") != std::string::npos, "world metric window is bounded");
    Check(summary.find("world_p99_ms=60") != std::string::npos, "old startup frames leave the rolling window");
}
}  // namespace

int main()
{
    try
    {
        TestSocialMovementBehavior();
        TestChat();
        TestPlanner();
        TestLifecycle();
        TestPopulation();
        TestTradeTerms();
        TestMetrics();
        TestCognition();
        TestDialogue();
        TestPersistentState();
        TestCatalogEligibility();
        std::cout << checks << " ADVAI checks passed\n";
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << "ADVAI TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
