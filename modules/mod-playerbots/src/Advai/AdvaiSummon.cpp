/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
// Summoning duties for grouped bots: joining a Ritual of Summoning (or any other ritual) that a groupmate channels,
// starting their own ritual when asked, and using a meeting stone to pull a groupmate to them. Every action goes
// through the same server paths a client uses (CMSG_GAMEOBJ_USE / a normal spell cast), so the core's own rules
// (same raid, participant count, channel running, reagents) decide whether it works.
#include <cmath>

#include "AdvaiMgr.h"
#include "AdvaiMovement.h"
#include "CellImpl.h"
#include "GameObject.h"
#include "GameObjectData.h"
#include "Group.h"
#include "NearestGameObjects.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"
#include "SpellMgr.h"

namespace Advai
{
namespace
{
constexpr uint32 SPELL_RITUAL_OF_SUMMONING = 698;
constexpr uint32 ITEM_SOUL_SHARD = 6265;
constexpr float RITUAL_SCAN_RANGE = 40.0f;
constexpr float STONE_SCAN_RANGE = 25.0f;

std::list<GameObject*> NearbyObjects(Player* bot, float range)
{
    std::list<GameObject*> found;
    AnyGameObjectInObjectRangeCheck check(bot, range);
    Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(bot, found, check);
    Cell::VisitObjects(bot, searcher, range);
    return found;
}

void UseObject(Player* bot, GameObject* object)
{
    bot->StopMoving();
    auto* use = new WorldPacket(CMSG_GAMEOBJ_USE);
    *use << object->GetGUID();
    bot->GetSession()->QueuePacket(use);
}

void WalkTo(Player* bot, GameObject* object)
{
    float x, y, z;
    object->GetNearPoint(bot, x, y, z, 1.5f, 2.0f, object->GetAngle(bot));
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
}
}  // namespace

void Manager::SummonDuties(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    Player* bot = ai->GetBot();
    Group* group = bot->GetGroup();
    if (!group || ai->HasGameClientMaster() || !bot->IsAlive() || bot->IsInCombat() || bot->IsBeingTeleported() ||
        bot->IsInFlight() || bot->GetTransport() || bot->GetVehicle() || bot->InBattleground() ||
        bot->GetTradeData() || bot->IsNonMeleeSpellCast(false) || !bot->IsInWorld())
        return;

    std::uint32_t requester = 0;
    bool requested = false;
    std::uint8_t sociability = 50;
    {
        std::lock_guard lock(state->Mutex);
        if (now < state->NextSummonScan)
            return;
        state->NextSummonScan = now + 2;
        requested = state->SummonUntil > now;
        requester = requested ? state->SummonFor : 0;
        sociability = state->Persona.Sociability;
    }

    // 1) Join a ritual a groupmate is channeling.
    for (GameObject* object : NearbyObjects(bot, RITUAL_SCAN_RANGE))
    {
        if (!object->isSpawned() || object->GetGoType() != GAMEOBJECT_TYPE_SUMMONING_RITUAL)
            continue;
        Player* owner = object->GetOwner() ? object->GetOwner()->ToPlayer() : nullptr;
        if (!owner || owner == bot || !bot->IsInSameRaidWith(owner) ||
            !owner->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            continue;
        GameObjectTemplate const* info = object->GetGOInfo();
        if (!info || object->GetUniqueUseCount() >= info->summoningRitual.reqParticipants)
            continue;
        std::uint64_t const key = object->GetGUID().GetRawValue();
        bool join = false;
        {
            std::lock_guard lock(state->Mutex);
            if (state->HandledRitual != key)
            {
                // Decided once per ritual: always for a bot's own friend or a direct request, otherwise by temperament.
                bool const ownerIsBot = GET_PLAYERBOT_AI(owner) != nullptr;
                state->HandledRitual = key;
                state->JoinHandledRitual =
                    (requested && requester == owner->GetGUID().GetCounter()) || ownerIsBot ||
                    urand(0, 99) < 55u + sociability / 4u;
            }
            join = state->JoinHandledRitual;
        }
        if (!join)
            continue;
        if (bot->IsWithinDistInMap(object, INTERACTION_DISTANCE - 0.5f))
        {
            UseObject(bot, object);
            LOG_INFO("playerbots", "ADVAI summon: {} clicks ritual {} owned by {}", bot->GetName(), object->GetEntry(),
                     owner->GetName());
            std::lock_guard lock(state->Mutex);
            state->JoinHandledRitual = false;  // one click per ritual
            state->NextSummonScan = now + 4;
        }
        else
        {
            if (bot->IsMounted())
                bot->Dismount();
            WalkTo(bot, object);
            std::lock_guard lock(state->Mutex);
            state->NextSummonScan = now + 1;
        }
        return;
    }

    if (!requested || !requester)
        return;
    Player* target = ObjectAccessor::FindPlayerByLowGUID(requester);
    if (!target || target == bot || !target->IsInWorld() || !bot->IsInSameRaidWith(target))
        return;
    bool const farAway = target->GetMapId() != bot->GetMapId() || bot->GetDistance(target) > 60.0f;

    // 2) Meeting stone: stand at it and pull the requester over.
    if (farAway && now >= state->NextStoneUse)
        for (GameObject* object : NearbyObjects(bot, STONE_SCAN_RANGE))
        {
            if (!object->isSpawned() || object->GetGoType() != GAMEOBJECT_TYPE_MEETINGSTONE)
                continue;
            GameObjectTemplate const* info = object->GetGOInfo();
            if (!info || bot->GetLevel() < info->meetingstone.minLevel || target->GetLevel() < info->meetingstone.minLevel)
                continue;
            if (!bot->IsWithinDistInMap(object, INTERACTION_DISTANCE - 0.5f))
            {
                WalkTo(bot, object);
                return;
            }
            bot->SetSelection(target->GetGUID());
            UseObject(bot, object);
            LOG_INFO("playerbots", "ADVAI summon: {} uses the meeting stone to pull {}", bot->GetName(), target->GetName());
            std::lock_guard lock(state->Mutex);
            state->NextStoneUse = now + 60;
            state->SummonUntil = 0;
            return;
        }

    // 3) Own Ritual of Summoning: a warlock with a soul shard, at least two helpers close by.
    if (farAway && bot->getClass() == CLASS_WARLOCK && bot->HasSpell(SPELL_RITUAL_OF_SUMMONING) &&
        now >= state->NextOwnRitual && bot->HasItemCount(ITEM_SOUL_SHARD, 1, false))
    {
        uint32 helpers = 0;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member && member != bot && member != target && member->IsAlive() && member->IsInWorld() &&
                member->GetMapId() == bot->GetMapId() && bot->IsWithinDistInMap(member, 35.0f))
                ++helpers;
        }
        if (helpers >= 2 && !bot->HasSpellCooldown(SPELL_RITUAL_OF_SUMMONING))
        {
            if (bot->IsMounted())
                bot->Dismount();
            bot->StopMoving();
            bool const started = ai->CastSpell(SPELL_RITUAL_OF_SUMMONING, target);
            std::lock_guard lock(state->Mutex);
            state->NextOwnRitual = now + (started ? 150 : 20);
            if (started)
                state->SummonUntil = 0;
        }
    }
}
}  // namespace Advai
