/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiSocial.h"

#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

#include "AdvaiMgr.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "DBCStores.h"
#include "Group.h"
#include "Guild.h"
#include "GuildPackets.h"
#include "GameObject.h"
#include "Item.h"
#include "InstancePackets.h"
#include "LFGMgr.h"
#include "LFGPackets.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PetitionMgr.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "PlayerbotSecurity.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SocialMgr.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "WorldSession.h"
#include "World.h"

namespace Advai
{
int Manager::SocialPreference(BotId id, BotId peer) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto contact = state->Relationships.find(peer);
        if (contact != state->Relationships.end())
            return contact->second.Affinity;
    }
    return 0;
}

std::uint8_t Manager::PetitionSignatures(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->PetitionSignatureCount;
    }
    return 0;
}

bool Manager::WantsGuild(BotId id, BotId inviter) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto contact = state->Relationships.find(inviter);
        int affinity = contact == state->Relationships.end() ? 0 : contact->second.Affinity;
        return affinity > -20 && state->Persona.Sociability + state->Persona.Generosity / 3 + affinity >= 55;
    }
    return false;
}

bool Manager::WantsArena(BotId id, BotId inviter) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto contact = state->Relationships.find(inviter);
        int affinity = contact == state->Relationships.end() ? 0 : contact->second.Affinity;
        return affinity > -20 && state->Persona.Competitiveness + affinity / 2 >= 40;
    }
    return false;
}

std::uint64_t RaidRosterSignature(Group* group)
{
    if (!group || !group->isRaidGroup())
        return 0;
    std::uint64_t signature = group->GetGUID().GetRawValue() ^
                              (group->GetLeaderGUID().GetRawValue() * 1099511628211ULL);
    signature ^= static_cast<std::uint64_t>(group->GetMembersCount()) << 32;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (auto* member = ref->GetSource(); member && member->IsInWorld())
        {
            std::uint64_t role = PlayerbotAI::IsTank(member) ? 1 : PlayerbotAI::IsHeal(member) ? 2 : 3;
            signature ^= (member->GetGUID().GetRawValue() * 11400714819323198485ULL) ^
                         (role * (member->GetGUID().GetCounter() + 1ULL));
        }
    return signature;
}

bool GuildInviteOperation::Execute()
{
    auto& manager = Manager::Instance();
    auto* bot = ObjectAccessor::FindPlayer(_bot);
    auto* peer = ObjectAccessor::FindPlayer(_peer);
    auto blocked = [&](char const* reason)
    {
        manager.Report(_bot.GetCounter(), {Goal::Socialize, Outcome::Blocked, reason});
        return false;
    };
    if (!bot || !peer || !bot->IsInWorld() || !peer->IsInWorld() || !manager.Applies(bot) ||
        !manager.Active(_bot.GetCounter()) || !bot->GetSession() || !peer->GetSession() ||
        !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData() || !bot->GetGuild() ||
        bot->GetTeamId() != peer->GetTeamId() || peer->GetGuildId() || peer->GetGuildIdInvited())
        return blocked("requested guild invitation is no longer available");
    if ((bot->GetSocial() && bot->GetSocial()->HasIgnore(_peer)) ||
        (peer->GetSocial() && peer->GetSocial()->HasIgnore(_bot)))
        return blocked("requested guild invitation is blocked by ignore settings");
    WorldPackets::Guild::GuildInviteByName packet{WorldPacket(CMSG_GUILD_INVITE)};
    static_cast<std::string&>(packet.Name) = peer->GetName();
    bot->GetSession()->HandleGuildInviteOpcode(packet);
    if (peer->GetGuildIdInvited() != bot->GetGuildId())
        return blocked("native guild invitation rejected permissions or membership eligibility");
    manager.Report(_bot.GetCounter(), {Goal::Socialize, Outcome::Started,
                                       "requested guild invitation delivered; recipient decides whether to accept"});
    return true;
}

bool FlightOperation::Execute()
{
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    auto& manager = Manager::Instance();
    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() ||
        bot->IsBeingTeleported() || bot->GetTradeData() || _nodes.size() < 2 || _nodes.size() > 32)
        return false;
    if (!manager.Applies(bot) || !manager.Active(_bot.GetCounter()))
        return false;
    Creature* master = bot->GetNPCIfCanInteractWith(_master, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!master)
        return false;
    bot->GetSession()->SendLearnNewTaxiNode(master);
    auto from = sObjectMgr->GetNearestTaxiNode(*master, bot->GetTeamId());
    bool discovered = _nodes.front() == from && std::all_of(_nodes.begin(), _nodes.end(), [bot](uint32 node)
                                                            { return bot->m_taxi.IsTaximaskNodeKnown(node); });
    if (!discovered)
    {
        if (manager.Applies(bot))
            manager.Report(_bot.GetCounter(),
                           {_purpose, Outcome::Blocked, "requested flight route has undiscovered nodes"});
        return false;
    }
    if (auto* ai = GET_PLAYERBOT_AI(bot))
        ai->RemoveShapeshift();
    if (bot->IsMounted())
        bot->Dismount();
    bool started = bot->ActivateTaxiPathTo(_nodes, master, 0);
    if (manager.Applies(bot))
        manager.Report(_bot.GetCounter(), {_purpose, started ? Outcome::Started : Outcome::Blocked,
                                           started ? "paid discovered flight started through native taxi checks"
                                                   : "native flight rejected cost, route, mount or availability"});
    return started;
}

bool TrainOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) || !bot->IsAlive() ||
        bot->IsInCombat() || bot->GetTradeData())
        return false;
    Creature* npc = bot->GetNPCIfCanInteractWith(_trainer, UNIT_NPC_FLAG_TRAINER);
    auto* trainer = npc ? sObjectMgr->GetTrainer(npc->GetEntry()) : nullptr;
    if (!trainer || !trainer->IsTrainerValidForPlayer(bot))
    {
        manager.Report(_bot.GetCounter(), {Goal::Train, Outcome::Blocked, "trainer is no longer accessible"});
        return false;
    }
    std::vector<Trainer::Spell const*> spells;
    for (auto const& spell : trainer->GetSpells())
        if ((!_spell || spell.SpellId == _spell) && PlayerbotFactory::IsTrainerSpellAllowedForBot(bot, trainer, &spell))
            spells.push_back(&spell);
    std::sort(spells.begin(), spells.end(),
              [](auto* left, auto* right)
              {
                  return std::tie(left->ReqLevel, left->ReqSkillRank, left->MoneyCost, left->SpellId) <
                         std::tie(right->ReqLevel, right->ReqSkillRank, right->MoneyCost, right->SpellId);
              });
    uint32 learned = 0;
    bool unaffordable = false;
    float discount = bot->GetReputationPriceDiscount(npc);
    for (auto* spell : spells)
    {
        if (learned >= 8 || !trainer->CanTeachSpell(bot, spell))
            continue;
        uint32 cost = static_cast<uint32>(spell->MoneyCost * discount);
        if (!bot->HasEnoughMoney(cost))
        {
            unaffordable = true;
            continue;
        }
        std::vector<uint32> targets;
        auto* info = sSpellMgr->GetSpellInfo(spell->SpellId);
        if (!info)
            continue;
        for (auto const& effect : info->GetEffects())
            if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && effect.TriggerSpell && !bot->HasSpell(effect.TriggerSpell))
                targets.push_back(effect.TriggerSpell);
        if (targets.empty() && !bot->HasSpell(spell->SpellId))
            targets.push_back(spell->SpellId);
        trainer->TeachSpell(npc, bot, spell->SpellId);
        if (std::any_of(targets.begin(), targets.end(), [&](uint32 target) { return bot->HasSpell(target); }))
            ++learned;
        else
        {
            manager.Report(_bot.GetCounter(), {Goal::Train, Outcome::Blocked,
                                               "trainer accepted payment but no expected spell was learned: " +
                                                   std::to_string(spell->SpellId)});
            return false;
        }
    }
    manager.Report(_bot.GetCounter(), {Goal::Train, learned ? Outcome::Started : Outcome::Blocked,
                                       learned ? "paid trainer learning verified; refreshing remaining training needs"
                                       : unaffordable ? "insufficient money for eligible trainer spells"
                                                      : "trainer has no eligible unlearned spell"});
    return learned != 0;
}

void Manager::CommitParty(BotId id, BotId leader, Goal activity)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto now = static_cast<Timestamp>(time(nullptr));
        if (state->PartyLeader != leader || state->PartyActivity != activity)
        {
            state->PartyJoined = now;
            state->PartyWipes = 0;
            state->PartyWasWiped = false;
        }
        state->PartyLeader = leader;
        state->PartyActivity = activity;
        auto target = state->LifeGoal.Kind == activity ? state->LifeGoal.Target : 0;
        state->RetainActivityBefore(activity);
        state->LifeGoal = {activity,
                           now,
                           now + 120 + state->Persona.Patience * 4,
                           0,
                           target,
                           0,
                           Outcome::Started,
                           "accepted shared party activity"};
        state->NextSnapshot = 0;
        state->NextDecision = 0;
        state->Dirty = true;
    }
}

bool Manager::CanInitiatePull(BotId id) const
{
    auto state = Get(id);
    if (!state)
        return true;
    std::lock_guard lock(state->Mutex);
    return !state->PartyHold;
}

std::optional<Goal> Manager::PartyPurpose(BotId id, BotId leader) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->PartyLeader == leader && state->PartyActivity != Goal::None)
            return state->PartyActivity;
    }
    return std::nullopt;
}

bool Manager::PartyShouldLeave(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto now = static_cast<Timestamp>(time(nullptr));
        bool exhausted = state->PartyWipes >= 2 + state->Persona.Patience / 20;
        bool sessionEnded =
            now >= state->Life.SessionDue && state->Life.SessionDue && now >= state->PartyJoined + 30 * 60;
        return state->PartyLeader && (exhausted || sessionEnded);
    }
    return false;
}

void Manager::SetPartyReadiness(BotId id, bool hold, bool wiped, std::string const& reason)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto now = static_cast<Timestamp>(time(nullptr));
        if (state->PartyHold != hold && state->PartyLeader && now >= state->PartyJoined + 5)
            state->QueueReaction(
                hold ? Intent::PartyWait : Intent::PartyReady,
                {SocialEventKind::Conversation, state->PartyLeader == id ? 0 : state->PartyLeader, now, !hold});
        state->PartyHold = hold;
        state->PartyReason = reason;
        if (wiped && !state->PartyWasWiped)
        {
            state->PartyWipes = std::min<std::uint8_t>(20, state->PartyWipes + 1);
            state->ObserveExperience({SocialEventKind::Wipe, 0, now, false});
        }
        if (wiped)
            state->PartyWasWiped = true;
        else if (!hold)
            state->PartyWasWiped = false;
        state->Dirty = true;
    }
}

bool PartyUpdateOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!bot || !bot->IsInWorld() || !group || group->GetLeaderGUID() != _bot || !manager.Applies(bot))
        return false;
    auto* ai = GET_PLAYERBOT_AI(bot);
    if (!ai || ai->HasGameClientMaster() || bot->InBattleground())
        return false;
    std::vector<Player*> members;
    bool combat = false;
    uint32 alive = 0;
    std::string reason;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsInWorld())
        {
            reason = "waiting for an offline party member";
            continue;
        }
        if (!manager.Applies(member))
        {
            for (GroupReference* other = group->GetFirstMember(); other; other = other->next())
                if (Player* player = other->GetSource(); player && manager.Applies(player))
                    manager.SetPartyReadiness(player->GetGUID().GetCounter(), false, false, "human-led party");
            return false;
        }
        members.push_back(member);
        combat |= member->IsInCombat();
        alive += member->IsAlive();
        if (!reason.empty())
            continue;
        if (!member->IsAlive())
            reason = "waiting for dead party members to recover";
        else if (member->GetTradeData() || member->IsInFlight() || member->IsBeingTeleported())
            reason = "waiting for a party member's transaction or travel";
        else if (member->GetMap() != bot->GetMap() || !member->IsWithinDistInMap(bot, 100.0f))
            reason = "waiting for the party to regroup";
        else if (member->GetHealthPct() < 70 ||
                 (member->getPowerType() == POWER_MANA && member->GetPowerPct(POWER_MANA) < 40))
            reason = "waiting for health and mana recovery";
        else
        {
            uint64 durability = 0, maximum = 0;
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                if (auto* item = member->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    durability += item->GetUInt32Value(ITEM_FIELD_DURABILITY);
                    maximum += item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
                }
            if (maximum && durability * 100 / maximum < 10)
                reason = "party equipment needs repairs before another pull";
        }
    }
    if (members.size() != group->GetMembersCount())
        reason = "waiting for an offline party member";
    if (combat && bot->GetMap()->IsDungeon() && group->GetTargetIcon(7).IsEmpty())
        for (auto* member : members)
            if (PlayerbotAI::IsTank(member))
                if (auto* target = member->GetVictim(); target && target->IsAlive() &&
                    target->GetMap() == bot->GetMap() && bot->CanSeeOrDetect(target) &&
                    bot->IsWithinLOSInMap(target) && bot->IsValidAttackTarget(target))
                {
                    // Respect encounter scripts' existing marks; assist the tank's actual engagement.
                    group->SetTargetIcon(7, _bot, target->GetGUID());
                    break;
                }
    if (!combat && bot->GetMap()->IsDungeon() && reason.empty())
    {
        bool tank = false, healer = false;
        for (auto* member : members)
        {
            tank |= PlayerbotAI::IsTank(member);
            healer |= PlayerbotAI::IsHeal(member);
        }
        if (!tank || !healer)
            reason = "waiting for the tank and healer roles before another pull";
    }
    auto purpose = manager.PartyPurpose(_bot.GetCounter(), _bot.GetCounter());
    if (!purpose)
    {
        Goal activity = manager.PlannedGoal(_bot.GetCounter()).value_or(Goal::Quest);
        if (activity != Goal::Dungeon && activity != Goal::Raid && activity != Goal::Arena &&
            activity != Goal::Battleground)
            activity = Goal::Quest;
        for (Player* member : members)
            manager.CommitParty(member->GetGUID().GetCounter(), _bot.GetCounter(), activity);
    }
    bool wiped = !members.empty() && !alive && !combat;
    for (Player* member : members)
        manager.SetPartyReadiness(member->GetGUID().GetCounter(), !combat && !reason.empty(), wiped, reason);
    if (!combat && !reason.empty() && bot->IsAlive() && bot->GetMap()->IsDungeon())
        bot->StopMoving();
    // Leave through the same request as a player, after recovery and ongoing transactions.
    // A success since the last wipe resets patience's attempt budget.
    if (!combat && alive == members.size() &&
        std::none_of(members.begin(), members.end(), [](Player* member)
                     { return member->GetTradeData() || member->IsBeingTeleported() || member->IsInFlight(); }))
        for (Player* member : members)
            if (manager.PartyShouldLeave(member->GetGUID().GetCounter()))
            {
                auto activity = manager.PartyPurpose(member->GetGUID().GetCounter(), _bot.GetCounter());
                if (activity)
                    manager.Report(member->GetGUID().GetCounter(),
                                   {*activity, Outcome::Cancelled,
                                    "party commitment ended after recovery: attempt budget or session exhausted"});
                WorldPacket leave(CMSG_GROUP_DISBAND);
                member->GetSession()->HandleGroupDisbandOpcode(leave);
                if (!member->GetGroup())
                    if (auto* memberAi = GET_PLAYERBOT_AI(member))
                    {
                        memberAi->SetMaster(nullptr);
                        memberAi->ResetStrategies();
                        memberAi->Reset();
                    }
                break;  // Stagger departures; group changes invalidate this roster.
            }
    return true;
}

bool Manager::QueuedActivity(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->Snapshot.InQueue;
    }
    return true;
}

void Manager::CommitObservedActivity(BotId id, Goal kind, std::string const& reason)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto now = static_cast<Timestamp>(time(nullptr));
        Timestamp commitment = 60 + state->Persona.Patience * 3;
        if (kind == Goal::Raid || kind == Goal::Battleground || kind == Goal::Arena)
            commitment += state->Persona.Competitiveness * 2;
        state->RetainActivityBefore(kind);
        state->LifeGoal = {kind, now, now + commitment, 0, 0, 0, Outcome::Started, reason};
        state->NextSnapshot = 0;
        state->Dirty = true;
    }
}

bool DungeonJoinOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !bot->GetSession() || !manager.Applies(bot) ||
        !manager.Active(_bot.GetCounter()) || !bot->IsAlive() || bot->IsInCombat() || bot->InBattleground())
        return false;
    auto goal = manager.PlannedGoal(_bot.GetCounter());
    auto* ai = GET_PLAYERBOT_AI(bot);
    bool humanCommand = ai && ai->HasGameClientMaster();
    if ((!goal || (*goal != Goal::Dungeon && *goal != Goal::Raid)) && !humanCommand)
        return false;
    Goal activity = goal && *goal == Goal::Raid ? Goal::Raid : Goal::Dungeon;
    Group* group = bot->GetGroup();
    if (group && group->GetLeaderGUID() != _bot)
    {
        manager.Report(_bot.GetCounter(),
                       {activity, Outcome::Blocked, "only the group leader may select a dungeon activity"});
        return false;
    }
    if (sLFGMgr->GetState(_bot) != lfg::LFG_STATE_NONE ||
        (group && sLFGMgr->GetState(group->GetGUID()) != lfg::LFG_STATE_NONE))
        return true;
    std::vector<Player*> participants;
    if (group)
    {
        for (GroupReference* member = group->GetFirstMember(); member; member = member->next())
            if (Player* player = member->GetSource(); player && player->IsInWorld() && player->GetSession())
                participants.push_back(player);
        if (participants.size() != group->GetMembersCount())
        {
            manager.Report(_bot.GetCounter(), {activity, Outcome::Blocked, "a committed group member is offline"});
            return false;
        }
    }
    else
        participants.push_back(bot);
    for (Player* member : participants)
    {
        if (!member->IsAlive() || member->IsInCombat() || member->GetTradeData() || member->IsInFlight() ||
            member->GetHealthPct() < 70 ||
            (member->getPowerType() == POWER_MANA && member->GetPowerPct(POWER_MANA) < 40))
        {
            manager.Report(_bot.GetCounter(), {activity, Outcome::Blocked,
                                               "party member is unavailable or still recovering before dungeon queue"});
            return false;
        }
        sLFGMgr->InitializeLockedDungeons(member, group);
    }
    std::vector<uint32> eligible;
    uint32 reason = 0;
    for (auto id : _dungeons)
    {
        auto* dungeon = sLFGDungeonStore.LookupEntry(id);
        if (!dungeon || ((activity == Goal::Raid) != (dungeon->TypeID == lfg::LFG_TYPE_RAID)))
            continue;
        bool locked = false;
        for (Player* member : participants)
            for (auto const& [entry, status] : sLFGMgr->GetLockedDungeons(member->GetGUID()))
                if ((entry & 0x00FFFFFFU) == id)
                {
                    locked = true;
                    reason = status;
                    break;
                }
        if (!locked)
            eligible.push_back(id);
    }
    if (eligible.empty())
    {
        manager.Report(_bot.GetCounter(),
                       {activity, Outcome::Blocked,
                        "no eligible dungeon for the whole group; core lock reason=" + std::to_string(reason)});
        return false;
    }
    auto persona = manager.Persona(_bot.GetCounter());
    if (activity == Goal::Raid && !humanCommand)
    {
        auto target = manager.RaidTarget(_bot.GetCounter());
        if (target && std::find(eligible.begin(), eligible.end(), target) == eligible.end())
        {
            manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked,
                                               "committed raid is no longer eligible for the whole group"});
            return false;
        }
        if (!target)
        {
            std::sort(eligible.begin(), eligible.end());
            target = eligible[persona.Seed % eligible.size()];
            manager.SelectRaidTarget(_bot.GetCounter(), target);
        }
        return RaidAssemblyOperation(_bot, target).Execute();
    }
    std::vector<uint32> random;
    for (auto id : eligible)
        if (sLFGDungeonStore.LookupEntry(id)->TypeID == lfg::LFG_TYPE_RANDOM)
            random.push_back(id);
    // A random category must be the only selected entry; mixing it with specifics is rejected by the core.
    if (!random.empty())
        eligible = {random[(persona.Seed + static_cast<uint64>(time(nullptr)) / 600) % random.size()]};
    else
    {
        std::sort(eligible.begin(), eligible.end());
        auto offset = (persona.Seed + static_cast<uint64>(time(nullptr)) / 600) % eligible.size();
        std::rotate(eligible.begin(), eligible.begin() + offset, eligible.end());
        if (eligible.size() > 3)
            eligible.resize(3);
    }
    WorldPackets::LFG::LFGJoin packet{WorldPacket(CMSG_LFG_JOIN)};
    packet.Roles = _roles;
    packet.Slots.resize(eligible.size());
    for (std::size_t index = 0; index < eligible.size(); ++index)
        packet.Slots[index] = eligible[index];
    bot->GetSession()->HandleLfgJoinOpcode(packet);
    bool joined = sLFGMgr->GetState(_bot) != lfg::LFG_STATE_NONE ||
                  (group && sLFGMgr->GetState(group->GetGUID()) != lfg::LFG_STATE_NONE);
    if (joined)
        manager.CommitObservedActivity(
            _bot.GetCounter(), activity,
            "core accepted eligible dungeon selection; awaiting group and encounter progress");
    else
        manager.Report(_bot.GetCounter(),
                       {activity, Outcome::Blocked,
                        "normal dungeon join checks rejected role, cooldown, membership or eligibility"});
    return joined;
}

bool AcceptInviteOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    Player* inviter = ObjectAccessor::FindPlayer(_inviter);
    if (!bot || !inviter || !manager.Applies(bot) || !bot->GetSession() || !bot->GetGroupInvite() ||
        bot->GetGroupInvite()->GetLeaderGUID() != _inviter)
        return false;
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    if (!ai || !ai->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, inviter))
    {
        WorldPacket decline(CMSG_GROUP_DECLINE);
        bot->GetSession()->HandleGroupDeclineOpcode(decline);
        return false;
    }
    if (bot->isAFK())
        bot->ToggleAFK();
    WorldPacket accept(CMSG_GROUP_ACCEPT);
    accept << uint32(0);
    bot->GetSession()->HandleGroupAcceptOpcode(accept);
    if (!bot->GetGroup() || !bot->GetGroup()->IsMember(_inviter))
        return false;
    ai->SetMaster(inviter);
    ai->ResetStrategies();
    ai->ChangeStrategy("+follow,-lfg,-bg", BOT_STATE_NON_COMBAT);
    ai->Reset();
    if (manager.Applies(inviter))
    {
        Goal activity = manager.PlannedGoal(_inviter.GetCounter()).value_or(Goal::Quest);
        if (activity != Goal::Quest && activity != Goal::Dungeon && activity != Goal::Raid && activity != Goal::Arena &&
            activity != Goal::Battleground)
            activity = Goal::Quest;
        BotId leader = bot->GetGroup()->GetLeaderGUID().GetCounter();
        for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
            if (Player* participant = ref->GetSource(); participant && manager.Applies(participant))
                manager.CommitParty(participant->GetGUID().GetCounter(), leader, activity);
        if (activity == Goal::Raid)
            for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
                if (Player* participant = ref->GetSource(); participant && manager.Applies(participant))
                    manager.SelectRaidTarget(participant->GetGUID().GetCounter(), manager.RaidTarget(leader));
    }
    return true;
}

bool InviteOperation::Execute()
{
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    Player* peer = ObjectAccessor::FindPlayer(_peer);
    if (!bot || !peer || !bot->IsInWorld() || !peer->IsInWorld() || !bot->GetSession() || peer->GetGroup() ||
        peer->GetGroupInvite() || bot->IsInCombat() || !peer->IsAlive() || peer->IsInCombat() || peer->GetTradeData() ||
        peer->IsInFlight() || peer->IsBeingTeleported() || peer->InBattlegroundQueue() || peer->InBattleground() ||
        bot->GetTeamId() != peer->GetTeamId())
        return false;
    auto& manager = Manager::Instance();
    if (manager.Applies(peer))
    {
        auto* ai = GET_PLAYERBOT_AI(peer);
        if (!manager.Active(_peer.GetCounter()) || !ai || ai->HasGameClientMaster())
            return false;
    }
    WorldPacket packet(CMSG_GROUP_INVITE);
    packet << peer->GetName() << uint32(0);
    bot->GetSession()->HandleGroupInviteOpcode(packet);
    Group* invitation = peer->GetGroupInvite();
    return invitation && (invitation == bot->GetGroup() || invitation->GetLeaderGUID() == _bot);
}

bool Manager::AvailableForActivity(BotId id, Goal activity) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto const& snapshot = state->Snapshot;
        if (!state->Life.Initialized || snapshot.HasCommitment() || snapshot.Group || snapshot.Health < 75 ||
            snapshot.Durability < 30 || !state->Requests.empty() || state->Offer)
            return false;
        return state->LifeGoal.Kind == activity ||
               (static_cast<Timestamp>(std::time(nullptr)) >= state->LifeGoal.CommittedUntil &&
                state->Persona.Sociability >= 60);
    }
    return false;
}

std::uint32_t Manager::RaidTarget(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->LifeGoal.Kind == Goal::Raid ? state->LifeGoal.Target : 0;
    }
    return 0;
}

void Manager::SelectRaidTarget(BotId id, std::uint32_t dungeon)
{
    if (!dungeon)
        return;
    auto* entry = sLFGDungeonStore.LookupEntry(dungeon);
    if (!entry || entry->TypeID != lfg::LFG_TYPE_RAID)
        throw std::runtime_error("ADVAI raid target is absent from the native dungeon catalog");
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.Kind == Goal::Raid && state->LifeGoal.Target != dungeon)
        {
            state->LifeGoal.Target = dungeon;
            state->RaidRosterReady = false;
            state->Dirty = true;
        }
    }
}

bool Manager::RaidReady(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->RaidRosterReady;
    }
    return false;
}

void Manager::SetRaidReady(BotId id, bool ready)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        state->RaidRosterReady = ready;
        auto* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        state->RaidRosterSignature = ready && player ? RaidRosterSignature(player->GetGroup()) : 0;
    }
}

std::vector<ServiceLocation> const& Manager::RaidEntrances(std::uint32_t map) const
{
    static std::vector<ServiceLocation> const empty;
    auto found = _raidEntrances.find(map);
    return found == _raidEntrances.end() ? empty : found->second;
}

bool EnterRaidOperation::Execute()
{
    auto& manager = Manager::Instance();
    auto* bot = ObjectAccessor::FindPlayer(_bot);
    auto* raid = sLFGDungeonStore.LookupEntry(_dungeon);
    auto* portal = sObjectMgr->GetAreaTriggerTeleport(_trigger);
    auto* trigger = sObjectMgr->GetAreaTrigger(_trigger);
    if (!bot || !raid || !portal || !trigger || !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) ||
        !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->IsBeingTeleported() ||
        bot->GetTradeData() || !bot->GetSession() || manager.RaidTarget(_bot.GetCounter()) != _dungeon ||
        !bot->GetGroup() || !bot->GetGroup()->isRaidGroup() || !manager.RaidReady(_bot.GetCounter()) ||
        portal->target_mapId != raid->MapID || !bot->IsInAreaTriggerRadius(trigger))
        return false;
    WorldPacket packet(CMSG_AREATRIGGER);
    packet << _trigger;
    bot->GetSession()->HandleAreaTriggerOpcode(packet);
    bool entered = bot->GetMapId() == raid->MapID || bot->IsBeingTeleported();
    manager.Report(_bot.GetCounter(), {Goal::Raid, entered ? Outcome::Started : Outcome::Blocked,
                                       entered ? "native raid portal accepted entry"
                                               : "native raid portal rejected entry; check access, lockout, difficulty and capacity"});
    return entered;
}

bool PortalUseOperation::Execute()
{
    auto& manager = Manager::Instance();
    auto* bot = ObjectAccessor::FindPlayer(_bot);
    auto* portal = bot ? ObjectAccessor::GetGameObject(*bot, _portal) : nullptr;
    if (!bot || !portal || !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) || !bot->GetSession() ||
        !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->IsBeingTeleported() || bot->IsInFlight() ||
        bot->GetTradeData() || !portal->isSpawned() || !bot->InSamePhase(portal) ||
        !bot->IsWithinDistInMap(portal, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(portal))
        return false;
    auto* info = portal->GetGOInfo();
    auto spellId = info->type == GAMEOBJECT_TYPE_SPELLCASTER ? info->spellcaster.spellId
                   : info->type == GAMEOBJECT_TYPE_GOOBER ? info->goober.spellId : 0;
    auto* spell = sSpellMgr->GetSpellInfo(spellId);
    if (!spell || !spell->HasEffect(SPELL_EFFECT_TELEPORT_UNITS))
        return false;
    WorldLocation before(*bot);
    WorldPacket packet(CMSG_GAMEOBJ_USE);
    packet << _portal;
    bot->GetSession()->HandleGameObjectUseOpcode(packet);
    bool changed = bot->GetMapId() != before.GetMapId() || bot->IsBeingTeleported() ||
                   bot->GetExactDist(before) > 1.0f;
    manager.Report(_bot.GetCounter(), {_purpose, changed ? Outcome::Started : Outcome::Blocked,
                                       changed ? "native gameobject portal use accepted"
                                               : "native portal use rejected or awaits a required ordinary interaction"});
    return changed;
}

bool AreaTriggerUseOperation::Execute()
{
    auto& manager = Manager::Instance();
    auto* bot = ObjectAccessor::FindPlayer(_bot);
    auto* trigger = sObjectMgr->GetAreaTrigger(_trigger);
    if (!bot || !trigger || !sObjectMgr->GetAreaTriggerTeleport(_trigger) || !manager.Applies(bot) ||
        !manager.Active(_bot.GetCounter()) || !bot->GetSession() || !bot->IsInWorld() || !bot->IsAlive() ||
        bot->IsInCombat() || bot->GetTradeData() || bot->IsBeingTeleported() || bot->IsInFlight() ||
        !bot->IsInAreaTriggerRadius(trigger))
        return false;
    WorldLocation before(*bot);
    WorldPacket packet(CMSG_AREATRIGGER);
    packet << _trigger;
    bot->GetSession()->HandleAreaTriggerOpcode(packet);
    bool changed = bot->GetMapId() != before.GetMapId() || bot->IsBeingTeleported() ||
                   bot->GetExactDist(before) > 1.0f;
    manager.Report(_bot.GetCounter(), {_purpose, changed ? Outcome::Started : Outcome::Blocked,
                                       changed ? "native reached area trigger accepted travel"
                                               : "native reached area trigger rejected travel"});
    return changed;
}

bool RaidAssemblyOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    auto* entry = sLFGDungeonStore.LookupEntry(_dungeon);
    if (!bot || !entry || entry->TypeID != lfg::LFG_TYPE_RAID || !bot->IsInWorld() || !bot->IsAlive() ||
        !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) || bot->IsInCombat() || bot->GetTradeData() ||
        bot->IsInFlight() || bot->GetGroupInvite() || bot->InBattleground() || bot->GetMap()->IsDungeon())
        return false;
    auto* ai = GET_PLAYERBOT_AI(bot);
    Group* group = bot->GetGroup();
    if (!ai || ai->HasGameClientMaster() || (group && (!group->IsLeader(_bot) || group->isLFGGroup())))
        return false;
    manager.SetRaidReady(_bot.GetCounter(), false);
    auto* map = sMapStore.LookupEntry(entry->MapID);
    auto* difficulty = GetMapDifficultyData(entry->MapID, static_cast<Difficulty>(entry->Difficulty));
    auto capacity = difficulty && difficulty->maxPlayers ? difficulty->maxPlayers : map ? map->maxPlayers : 0;
    if (!map || !map->IsRaid() || capacity < 5 || capacity > 40)
    {
        manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked, "raid has no supported native roster capacity"});
        return false;
    }
    if (group)
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (auto* member = ref->GetSource(); !member || !manager.Applies(member))
            {
                manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked,
                                                   "raid group requires human coordination before changing its setup"});
                return false;
            }
    if (group && group->GetMembersCount() >= 2 && !group->isRaidGroup())
    {
        WorldPacket convert(CMSG_GROUP_RAID_CONVERT);
        bot->GetSession()->HandleGroupRaidConvertOpcode(convert);
        if (!group->isRaidGroup())
        {
            manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked, "native raid conversion rejected the group"});
            return false;
        }
    }
    if (bot->GetRaidDifficulty() != static_cast<Difficulty>(entry->Difficulty))
    {
        WorldPackets::Instance::SetRaidDifficultyClient packet{WorldPacket(MSG_SET_RAID_DIFFICULTY)};
        packet.Mode = entry->Difficulty;
        bot->GetSession()->HandleSetRaidDifficultyOpcode(packet);
        if (bot->GetRaidDifficulty() != static_cast<Difficulty>(entry->Difficulty))
        {
            manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked, "native raid difficulty change was rejected"});
            return false;
        }
    }
    std::array<std::uint32_t, 3> counts{};
    auto role = [](Player* player) -> std::size_t
    { return PlayerbotAI::IsTank(player) ? 0 : PlayerbotAI::IsHeal(player) ? 1 : 2; };
    if (group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            auto* member = ref->GetSource();
            if (!member || !member->IsInWorld() || !manager.Applies(member))
            {
                manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked, "raid member is offline or needs human coordination"});
                return false;
            }
            ++counts[role(member)];
        }
    }
    else
        ++counts[role(bot)];
    std::array<std::uint32_t, 3> required = {capacity >= 25 ? 3U : 2U, std::max(2U, capacity / 5), 0};
    required[2] = capacity - required[0] - required[1];
    if (counts[0] >= required[0] && counts[1] >= required[1] && counts[2] >= required[2])
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (auto* member = ref->GetSource())
            {
                manager.SelectRaidTarget(member->GetGUID().GetCounter(), _dungeon);
                manager.SetRaidReady(member->GetGUID().GetCounter(), true);
            }
        manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Started, "native raid roster has the required roles; prepare and travel to the entrance"});
        return true;
    }
    if (counts[0] + counts[1] + counts[2] >= capacity)
    {
        manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked,
                                           "raid is full but lacks required roles; roster changes require normal group decisions"});
        return false;
    }
    Player* recruit = nullptr;
    int bestScore = -1;
    for (auto it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        auto* peer = it->second;
        if (peer && peer->GetGroupInvite() && peer->GetGroupInvite()->GetLeaderGUID() == _bot)
        {
            manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Started,
                                               "waiting for the outstanding native raid invitation"});
            return true;
        }
        if (!peer || peer == bot || !peer->IsInWorld() || !peer->IsAlive() || !manager.Applies(peer) ||
            !manager.Active(peer->GetGUID().GetCounter()) || !peer->GetSession() || peer->GetGroup() ||
            peer->GetGroupInvite() || peer->GetTradeData() || peer->IsInCombat() || peer->IsInFlight() ||
            peer->IsBeingTeleported() || peer->InBattlegroundQueue() || peer->InBattleground() ||
            peer->GetMap()->IsDungeon() || peer->GetTeamId() != bot->GetTeamId() ||
            !manager.AvailableForActivity(peer->GetGUID().GetCounter(), Goal::Raid) ||
            peer->GetLevel() < entry->MinLevel || (entry->MaxLevel && peer->GetLevel() > entry->MaxLevel))
            continue;
        auto* peerAi = GET_PLAYERBOT_AI(peer);
        bool guildmate = bot->GetGuildId() && bot->GetGuildId() == peer->GetGuildId();
        bool nearby = bot->GetMap() == peer->GetMap() && bot->IsWithinDistInMap(peer, 100.0f) &&
                      bot->CanSeeOrDetect(peer, false, true);
        auto index = role(peer);
        if (!peerAi || peerAi->HasGameClientMaster() || (!guildmate && !nearby) || counts[index] >= required[index])
            continue;
        sLFGMgr->InitializeLockedDungeons(peer, nullptr);
        bool locked = false;
        for (auto const& [dungeon, reason] : sLFGMgr->GetLockedDungeons(peer->GetGUID()))
            locked |= (dungeon & 0x00FFFFFFU) == _dungeon;
        if (locked)
            continue;
        int affinity = manager.SocialPreference(_bot.GetCounter(), peer->GetGUID().GetCounter());
        if (affinity <= -20 || manager.SocialPreference(peer->GetGUID().GetCounter(), _bot.GetCounter()) <= -20)
            continue;
        int score = (guildmate ? 40 : 0) + (index < 2 ? 100 : 0) +
                    manager.Persona(peer->GetGUID().GetCounter()).Skill + affinity;
        if (score > bestScore)
        {
            recruit = peer;
            bestScore = score;
        }
    }
    if (!recruit)
    {
        manager.Report(_bot.GetCounter(), {Goal::Raid, Outcome::Blocked,
                                           "no available eligible guildmate or visible player can fill the missing raid roles"});
        return false;
    }
    bool invited = InviteOperation(_bot, recruit->GetGUID()).Execute();
    manager.Report(_bot.GetCounter(), {Goal::Raid, invited ? Outcome::Started : Outcome::Blocked,
                                       invited ? "normal raid invitation sent; waiting for acceptance"
                                               : "native raid invitation was rejected"});
    return invited;
}

bool ArenaCharterOperation::Execute()
{
    auto& manager = Manager::Instance();
    auto* bot = ObjectAccessor::FindPlayer(_bot);
    auto blocked = [&](char const* reason)
    {
        manager.Report(_bot.GetCounter(), {Goal::Arena, Outcome::Blocked, reason});
        return false;
    };
    if (!bot || !bot->IsInWorld() || !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) ||
        !bot->IsAlive() || bot->IsInCombat() || bot->GetGroup() || bot->GetTradeData() ||
        bot->IsInFlight() || bot->IsBeingTeleported() || bot->InBattleground() || bot->InBattlegroundQueue() ||
        manager.PlannedGoal(_bot.GetCounter()) != Goal::Arena)
        return false;
    auto* registrar = bot->GetNPCIfCanInteractWith(_vendor, UNIT_NPC_FLAG_PETITIONER);
    if (!registrar || registrar->IsTabardDesigner())
        return blocked("arena charter requires an accessible arena registrar");
    constexpr std::array<std::uint32_t, 3> charterItems = {
        ARENA_TEAM_CHARTER_2v2, ARENA_TEAM_CHARTER_3v3, ARENA_TEAM_CHARTER_5v5};
    constexpr std::array<std::uint8_t, 3> teamSizes = {2, 3, 5};
    std::size_t choice = manager.Persona(_bot.GetCounter()).Seed % teamSizes.size();
    Item* charter = nullptr;
    for (std::size_t slot = 0; slot < charterItems.size(); ++slot)
        if (bot->GetArenaTeamId(slot))
            return true;
    for (std::size_t slot = 0; slot < charterItems.size(); ++slot)
    {
        if (auto* item = bot->GetItemByEntry(charterItems[slot]))
        {
            charter = item;
            choice = slot;
            break;
        }
    }
    if (!charter)
    {
        constexpr std::array costs = {CONFIG_CHARTER_COST_ARENA_2v2, CONFIG_CHARTER_COST_ARENA_3v3,
                                      CONFIG_CHARTER_COST_ARENA_5v5};
        if (!bot->HasEnoughMoney(sWorld->getIntConfig(costs[choice])))
            return blocked("insufficient earned money for an arena charter");
        constexpr std::array suffixes = {"Duelists", "Vanguard", "Resolve", "Outriders",
                                         "Sentinels", "Concord", "Tempest", "Wardens"};
        std::string name;
        for (std::size_t offset = 0; offset < suffixes.size(); ++offset)
        {
            auto candidate = bot->GetName() + " " + suffixes[(choice + offset) % suffixes.size()];
            if (!sArenaTeamMgr->GetArenaTeamByName(candidate))
            {
                name = std::move(candidate);
                break;
            }
        }
        if (name.empty())
            return blocked("arena charter name catalog exhausted for this character");
        WorldPacket packet(CMSG_PETITION_BUY);
        packet << _vendor << std::uint32_t(0) << std::uint64_t(0) << name << std::string();
        for (int field = 0; field < 7; ++field)
            packet << std::uint32_t(0);
        packet << std::uint16_t(0);
        for (int field = 0; field < 3; ++field)
            packet << std::uint32_t(0);
        for (int field = 0; field < 10; ++field)
            packet << std::string();
        packet << static_cast<std::uint32_t>(choice + 1) << std::uint32_t(0);
        bot->GetSession()->HandlePetitionBuyOpcode(packet);
        charter = bot->GetItemByEntry(charterItems[choice]);
        if (!charter)
            return blocked("native arena charter purchase rejected funds, level, name or inventory requirements");
        manager.Report(_bot.GetCounter(), {Goal::Arena, Outcome::Started, "paid arena charter observed in inventory"});
        return true;
    }
    auto* petition = sPetitionMgr->GetPetition(charter->GetGUID());
    if (!petition || petition->ownerGuid != _bot || petition->petitionType != teamSizes[choice])
        return blocked("owned arena charter has inconsistent native petition data");
    auto* signatures = sPetitionMgr->GetSignature(charter->GetGUID());
    if (signatures && signatures->signatureMap.size() >= teamSizes[choice] - 1U)
    {
        WorldPacket packet(CMSG_TURN_IN_PETITION);
        packet << charter->GetGUID();
        bot->GetSession()->HandleTurnInPetitionOpcode(packet);
        if (!bot->GetArenaTeamId(choice))
            return blocked("native arena team creation rejected charter signatures or eligibility");
        manager.Report(_bot.GetCounter(), {Goal::Arena, Outcome::Started,
                                           "arena team created through paid charter and verified native signatures"});
        return true;
    }
    struct Offers
    {
        Timestamp Last = 0;
        std::deque<std::pair<BotId, Timestamp>> Recent;
    };
    static std::unordered_map<BotId, Offers> offers;
    auto now = static_cast<Timestamp>(time(nullptr));
    std::erase_if(offers, [now](auto const& entry) { return entry.second.Last + 1800 < now; });
    auto& history = offers[_bot.GetCounter()];
    history.Last = now;
    std::erase_if(history.Recent, [now](auto const& offer) { return offer.second <= now; });
    auto* ai = GET_PLAYERBOT_AI(bot);
    if (!ai)
        return false;
    for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
    {
        auto* peer = ObjectAccessor::FindPlayer(guid);
        if (!peer || peer == bot || !peer->IsInWorld() || !peer->IsAlive() || !peer->GetSession() ||
            peer->GetTeamId() != bot->GetTeamId() || peer->GetLevel() < bot->GetLevel() || peer->GetGroup() ||
            peer->IsInCombat() || peer->GetTradeData() || peer->GetArenaTeamId(choice) ||
            peer->GetArenaTeamIdInvited() || !bot->IsWithinDistInMap(peer, 30.0f) || !bot->IsWithinLOSInMap(peer) ||
            !bot->CanSeeOrDetect(peer) || peer->GetSession()->GetAccountId() == bot->GetSession()->GetAccountId() ||
            (peer->GetSocial() && peer->GetSocial()->HasIgnore(_bot)) ||
            (bot->GetSocial() && bot->GetSocial()->HasIgnore(guid)))
            continue;
        if (signatures && std::any_of(signatures->signatureMap.begin(), signatures->signatureMap.end(),
            [&](auto const& signature) { return signature.first == guid || signature.second == peer->GetSession()->GetAccountId(); }))
            continue;
        if (std::any_of(history.Recent.begin(), history.Recent.end(),
                        [&](auto const& offer) { return offer.first == guid.GetCounter(); }))
            continue;
        if (manager.Applies(peer) && !manager.AvailableForActivity(guid.GetCounter(), Goal::Arena))
            continue;
        if (history.Recent.size() >= 16)
            break;
        history.Recent.emplace_back(guid.GetCounter(), now + 600);
        WorldPacket packet(CMSG_OFFER_PETITION);
        packet << std::uint32_t(0) << charter->GetGUID() << guid;
        bot->GetSession()->HandleOfferPetitionOpcode(packet);
        manager.Report(_bot.GetCounter(), {Goal::Arena, Outcome::Started,
                                           "arena charter offered to an available nearby player; awaiting consent"});
        return true;
    }
    manager.Report(_bot.GetCounter(), {Goal::Arena, Outcome::Blocked,
                                       "arena charter needs more willing eligible nearby signers; prior responses retained"});
    return false;
}

bool ArenaAssemblyOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) ||
        !bot->IsAlive() || bot->IsInCombat() || bot->InBattleground() || bot->GetTradeData())
        return false;
    auto goal = manager.PlannedGoal(_bot.GetCounter());
    if (!goal || *goal != Goal::Arena)
        return false;
    static std::unordered_map<BotId, Timestamp> nextAttempt;
    auto now = static_cast<Timestamp>(time(nullptr));
    if (now < nextAttempt[_bot.GetCounter()])
        return true;
    nextAttempt[_bot.GetCounter()] = now + 30;
    std::erase_if(nextAttempt, [now](auto const& entry) { return entry.second + 300 < now; });
    ArenaTeam* team = sArenaTeamMgr->GetArenaTeamByCaptain(_bot, static_cast<ArenaType>(_type));
    if (!team)
    {
        manager.Report(_bot.GetCounter(), {Goal::Arena, Outcome::Blocked, "no arena team captained by this character"});
        return false;
    }
    Group* group = bot->GetGroup();
    if (group && (!group->IsLeader(_bot) || group->isRaidGroup() || group->GetMembersCount() > _type))
    {
        manager.Report(_bot.GetCounter(),
                       {Goal::Arena, Outcome::Blocked, "existing group is not an available arena premade"});
        return false;
    }
    std::uint32_t invited = 0;
    std::uint32_t present = group ? group->GetMembersCount() : 1;
    for (auto const& member : team->GetMembers())
    {
        if (member.Guid == _bot || present + invited >= _type)
            continue;
        Player* peer = ObjectAccessor::FindPlayer(member.Guid);
        if (!peer || !peer->IsInWorld() || !manager.Applies(peer) || !manager.Active(member.Guid.GetCounter()) ||
            peer->GetGroup() || peer->GetGroupInvite() || !peer->IsAlive() || peer->IsInCombat() ||
            peer->GetTradeData() || peer->InBattleground() || peer->InBattlegroundQueue() ||
            peer->GetMap()->IsDungeon() || peer->GetLevel() != bot->GetLevel())
            continue;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(peer);
        auto purpose = manager.PlannedGoal(member.Guid.GetCounter());
        if (!ai || ai->HasGameClientMaster() || !purpose || (*purpose != Goal::Arena && *purpose != Goal::Battleground))
            continue;
        if (!manager.WantsArena(member.Guid.GetCounter(), _bot.GetCounter()) ||
            manager.SocialPreference(_bot.GetCounter(), member.Guid.GetCounter()) <= -20)
            continue;
        InviteOperation invitation(_bot, member.Guid);
        if (invitation.Execute())
            ++invited;
    }
    if (!invited && present < _type)
    {
        manager.Report(_bot.GetCounter(),
                       {Goal::Arena, Outcome::Blocked, "not enough online, available and willing arena teammates"});
        return false;
    }
    manager.Report(_bot.GetCounter(),
                   {Goal::Arena, Outcome::Started, "waiting for native group invitations to be accepted"});
    return true;
}
}  // namespace Advai
