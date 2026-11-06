/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiTravel.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <queue>
#include <stdexcept>

#include "AdvaiSocial.h"
#include "AdvaiMgr.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Creature.h"
#include "Event.h"
#include "GameObject.h"
#include "GameObjectModel.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Transport.h"
#include "TransportMgr.h"
#include "WorldSession.h"

namespace Advai
{
namespace
{
std::unordered_map<std::uint32_t, std::vector<TravelTransfer>> transfers;
std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> reachable;
std::unordered_map<std::uint32_t, std::vector<ServiceLocation>> portApproaches;
bool initialized = false;

std::uint64_t TransferKey(TravelTransfer const& edge)
{
    return (static_cast<std::uint64_t>(edge.Kind) << 60) |
           (static_cast<std::uint64_t>(edge.From.GetMapId()) << 32) | edge.Entry;
}

bool OrdinaryMap(std::uint32_t id)
{
    auto* map = sMapStore.LookupEntry(id);
    return map && map->map_type == MAP_COMMON;
}

unsigned RemainingHops(std::uint32_t from, std::uint32_t to)
{
    if (from == to)
        return 0;
    std::queue<std::pair<std::uint32_t, unsigned>> pending;
    std::unordered_set<std::uint32_t> visited{from};
    pending.emplace(from, 0);
    while (!pending.empty())
    {
        auto [map, depth] = pending.front();
        pending.pop();
        if (depth >= 4)
            continue;
        if (auto edges = transfers.find(map); edges != transfers.end())
            for (auto const& edge : edges->second)
            {
                auto next = edge.To.GetMapId();
                if (next == to)
                    return depth + 1;
                if (visited.insert(next).second)
                    pending.emplace(next, depth + 1);
            }
    }
    return 100;
}

bool StoppedAt(MotionTransport* transport, WorldPosition const& point)
{
    if (!transport || transport->GetMapId() != point.GetMapId() || transport->GetExactDist(point) > 3.0f)
        return false;
    auto period = transport->GetPeriod();
    if (!period)
        return false;
    auto progress = transport->GetPathProgress() % period;
    for (auto const& frame : transport->GetKeyFrames())
        if (frame.IsStopFrame() && frame.Node->mapid == point.GetMapId() &&
            point.GetExactDist(frame.Node->x, frame.Node->y, frame.Node->z) < 3.0f)
        {
            auto arrival = frame.ArriveTime % period;
            auto departure = frame.DepartureTime % period;
            if (arrival <= departure ? progress >= arrival && progress < departure
                                     : progress >= arrival || progress < departure)
                return true;
        }
    return false;
}

float DeckHeight(MotionTransport* boat, Player* bot, float x, float y, float top)
{
    if (!boat || !boat->m_model)
        return INVALID_HEIGHT;
    G3D::Ray ray(G3D::Vector3(x, y, top), G3D::Vector3(0, 0, -1));
    float distance = 12.0f;
    return boat->m_model->intersectRay(ray, distance, false, bot->GetPhaseMask(), VMAP::ModelIgnoreFlags::Nothing)
               ? top - distance : INVALID_HEIGHT;
}

bool WalkableSegment(Player* bot, MotionTransport* boat, WorldPosition const& end)
{
    auto distance = bot->GetExactDist(end);
    if (distance > 8.0f || !bot->IsWithinLOS(end.GetPositionX(), end.GetPositionY(), end.GetPositionZ() + 1.5f))
        return false;
    auto steps = std::max(1, static_cast<int>(std::ceil(distance * 2)));
    float previous = bot->GetPositionZ();
    for (int index = 1; index <= steps; ++index)
    {
        float fraction = static_cast<float>(index) / steps;
        auto x = bot->GetPositionX() + (end.GetPositionX() - bot->GetPositionX()) * fraction;
        auto y = bot->GetPositionY() + (end.GetPositionY() - bot->GetPositionY()) * fraction;
        auto expected = bot->GetPositionZ() + (end.GetPositionZ() - bot->GetPositionZ()) * fraction;
        auto land = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, expected + 2.0f, true, 6.0f);
        auto deck = DeckHeight(boat, bot, x, y, expected + 2.0f);
        auto floor = std::max(land, deck);
        if (floor == INVALID_HEIGHT || floor <= VMAP_INVALID_HEIGHT_VALUE || std::abs(floor - previous) > 1.0f ||
            std::abs(floor - expected) > 0.6f ||
            bot->GetMap()->IsInWater(bot->GetPhaseMask(), x, y, floor, bot->GetCollisionHeight()))
            return false;
        previous = floor;
    }
    return true;
}

std::optional<WorldPosition> BoardingPoint(Player* bot, MotionTransport* boat, bool leave)
{
    std::optional<WorldPosition> selected;
    float shortest = 9.0f;
    for (unsigned direction = 0; direction < 16; ++direction)
        for (float radius : {1.5f, 3.0f, 5.0f, 7.0f})
        {
            auto angle = direction * 6.28318530718f / 16;
            auto x = bot->GetPositionX() + std::cos(angle) * radius;
            auto y = bot->GetPositionY() + std::sin(angle) * radius;
            auto z = leave ? bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, bot->GetPositionZ() + 4, true, 10)
                           : DeckHeight(boat, bot, x, y, bot->GetPositionZ() + 4);
            if (z <= VMAP_INVALID_HEIGHT_VALUE || std::abs(z - bot->GetPositionZ()) > 4.0f)
                continue;
            bool onBoat = bot->GetMap()->GetTransportForPos(bot->GetPhaseMask(), x, y, z, bot) == boat;
            if (onBoat == leave)
                continue;
            WorldPosition point(bot->GetMapId(), x, y, z);
            auto distance = bot->GetExactDist(point);
            if (distance < shortest && WalkableSegment(bot, boat, point))
            {
                selected = point;
                shortest = distance;
            }
        }
    return selected;
}

std::optional<WorldPosition> DockPoint(Player* bot, WorldPosition const& stop)
{
    std::optional<WorldPosition> selected;
    float shortest = std::numeric_limits<float>::max();
    for (unsigned direction = 0; direction < 16; ++direction)
        for (float radius : {8.0f, 16.0f, 24.0f, 36.0f, 48.0f})
        {
            auto angle = direction * 6.28318530718f / 16;
            auto x = stop.GetPositionX() + std::cos(angle) * radius;
            auto y = stop.GetPositionY() + std::sin(angle) * radius;
            auto z = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, stop.GetPositionZ() + 20, true, 40);
            if (z <= VMAP_INVALID_HEIGHT_VALUE || std::abs(z - stop.GetPositionZ()) > 20 ||
                bot->GetMap()->IsInWater(bot->GetPhaseMask(), x, y, z, bot->GetCollisionHeight()) ||
                bot->GetMap()->GetTransportForPos(bot->GetPhaseMask(), x, y, z, bot))
                continue;
            WorldPosition point(bot->GetMapId(), x, y, z);
            auto distance = radius * 4.0f + std::min(100.0f, bot->GetExactDist(point) * 0.1f);
            if (distance < shortest)
            {
                shortest = distance;
                selected = point;
            }
        }
    return selected;
}
}  // namespace

void InitializeTravelNetwork()
{
    transfers.clear();
    portApproaches.clear();
    auto rows = WorldDatabase.Query("SELECT ID FROM areatrigger_teleport");
    if (!rows)
        throw std::runtime_error("ADVAI travel cannot load the native portal catalog");
    do
    {
        auto id = rows->Fetch()[0].Get<std::uint32_t>();
        auto* portal = sObjectMgr->GetAreaTriggerTeleport(id);
        auto* from = sObjectMgr->GetAreaTrigger(id);
        if (portal && from && OrdinaryMap(from->map) && OrdinaryMap(portal->target_mapId))
            transfers[from->map].push_back({TransferKind::AreaTrigger, id,
                WorldPosition(from->map, from->x, from->y, from->z),
                WorldPosition(portal->target_mapId, portal->target_X, portal->target_Y, portal->target_Z)});
    } while (rows->NextRow());
    for (auto const& [guid, data] : sObjectMgr->GetAllGOData())
    {
        auto* info = sObjectMgr->GetGameObjectTemplate(data.id);
        if (!info || !OrdinaryMap(data.mapid))
            continue;
        auto spellId = info->type == GAMEOBJECT_TYPE_SPELLCASTER ? info->spellcaster.spellId
                       : info->type == GAMEOBJECT_TYPE_GOOBER ? info->goober.spellId : 0;
        auto* spell = sSpellMgr->GetSpellInfo(spellId);
        if (!spell)
            continue;
        for (std::uint8_t index = 0; index < MAX_SPELL_EFFECTS; ++index)
            if (spell->Effects[index].Effect == SPELL_EFFECT_TELEPORT_UNITS)
                if (auto* target = sSpellMgr->GetSpellTargetPosition(spellId, static_cast<SpellEffIndex>(index));
                    target && OrdinaryMap(target->target_mapId))
                    transfers[data.mapid].push_back({TransferKind::GameObject, data.id,
                        WorldPosition(data.mapid, data.posX, data.posY, data.posZ),
                        WorldPosition(target->target_mapId, target->target_X, target->target_Y, target->target_Z)});
    }
    for (auto const& [id, info] : *sObjectMgr->GetGameObjectTemplates())
        if (info.type == GAMEOBJECT_TYPE_MO_TRANSPORT)
            if (auto* route = sTransportMgr->GetTransportTemplate(id); route && !route->inInstance)
                for (auto const& from : route->keyFrames)
                    if (from.IsStopFrame() && OrdinaryMap(from.Node->mapid))
                        for (auto const& to : route->keyFrames)
                            if (to.IsStopFrame() && OrdinaryMap(to.Node->mapid))
                                transfers[from.Node->mapid].push_back({TransferKind::Transport, id,
                                    WorldPosition(from.Node->mapid, from.Node->x, from.Node->y, from.Node->z),
                                    WorldPosition(to.Node->mapid, to.Node->x, to.Node->y, to.Node->z)});
    for (auto const& [guid, data] : sObjectMgr->GetAllCreatureData())
        if (auto* info = sObjectMgr->GetCreatureTemplate(data.id);
            info && (info->npcflag & (UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_FLIGHTMASTER |
                                     UNIT_NPC_FLAG_INNKEEPER)))
            if (auto edges = transfers.find(data.mapid); edges != transfers.end())
                if (std::any_of(edges->second.begin(), edges->second.end(), [&](auto const& edge)
                    { return edge.Kind == TransferKind::Transport &&
                             edge.From.GetExactDist(data.posX, data.posY, data.posZ) < 300.0f; }))
                    portApproaches[data.mapid].push_back({data.id, data.mapid, info->npcflag, info->faction,
                                                         data.posX, data.posY, data.posZ});
    reachable.clear();
    for (auto const& [from, edges] : transfers)
        for (auto const& [to, destinations] : transfers)
            if (RemainingHops(from, to) < 100)
                reachable[from].insert(to);
    initialized = true;
}

bool OnTransportSurface(Player* bot, Transport* transport)
{
    auto* boat = dynamic_cast<MotionTransport*>(transport);
    if (!bot || !boat || bot->GetMap() != boat->GetMap())
        return false;
    auto floor = DeckHeight(boat, bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ() + 2.0f);
    return floor > VMAP_INVALID_HEIGHT_VALUE && std::abs(floor - bot->GetPositionZ()) < 0.75f;
}

bool HasTravelConnection(std::uint32_t from, std::uint32_t to)
{
    if (!initialized)
        throw std::runtime_error("ADVAI travel network was not initialized");
    auto found = reachable.find(from);
    return from == to || (found != reachable.end() && found->second.contains(to));
}

void Journey::Reset()
{
    _transfer.reset();
    _destination.reset();
    _dock.reset();
    _transport.Clear();
    _waitingSince = 0;
    _submittedAt = 0;
    _ridingSince = 0;
    _nextFlightPlan = 0;
    _nextAbilityPlan = 0;
    _abilityStarted = 0;
    _abilityDestination.reset();
    _flightRoute.clear();
}

JourneyStep Journey::Step(PlayerbotAI* ai, WorldPosition const& destination, Goal purpose)
{
    if (!initialized)
        throw std::runtime_error("ADVAI travel network was not initialized");
    auto* bot = ai->GetBot();
    if (!ai->AllowActivity() || !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData())
        return {JourneyStatus::Blocked, {}, "survival, an existing transaction or suspension preempts travel"};
    auto now = static_cast<Timestamp>(time(nullptr));
    if (_destination && (_destination->GetMapId() != destination.GetMapId() ||
                         _destination->GetExactDist(destination) > 1.0f))
        Reset();
    _destination = destination;
    std::erase_if(_excluded, [now](auto const& entry) { return now >= entry.second; });
    if (bot->IsInFlight() || bot->IsBeingTeleported())
        return {JourneyStatus::Wait, {}, "waiting for the current native travel operation"};
    if (_abilityDestination)
    {
        if (bot->GetMapId() == _abilityDestination->GetMapId() && bot->GetExactDist(*_abilityDestination) < 80.0f)
        {
            _abilityDestination.reset();
            _abilityStarted = 0;
        }
        else if (now < _abilityStarted + 15)
            return {JourneyStatus::Wait, {}, "waiting for the owned travel item or learned class ability"};
        else
        {
            _abilityDestination.reset();
            _abilityStarted = 0;
            return {JourneyStatus::Blocked, {}, "owned travel item or learned class teleport did not reach its actual destination"};
        }
    }
    if (_transfer && !bot->GetTransport() && bot->GetMapId() == _transfer->To.GetMapId() &&
        (bot->GetMapId() != _transfer->From.GetMapId() || bot->GetExactDist(_transfer->To) < 80.0f))
        Reset();
    if (!_transfer && !bot->GetTransport() && bot->GetMapId() == destination.GetMapId() &&
        bot->GetExactDist(destination) < 1.0f)
        return {JourneyStatus::Arrived, {}, "destination reached through normal travel"};
    if (!_transfer)
    {
        if (!bot->GetTransport() && now >= _nextAbilityPlan &&
            (bot->GetMapId() != destination.GetMapId() || bot->GetExactDist(destination) > 3000.0f))
        {
            _nextAbilityPlan = now + 180;
            WorldPosition home(bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY, bot->m_homebindZ);
            if (home.GetMapId() == destination.GetMapId() && home.GetExactDist(destination) < 1500.0f &&
                bot->GetItemCount(6948, false) && !bot->HasSpellCooldown(8690) &&
                ai->DoSpecificAction("hearthstone", Event("advai"), true))
            {
                _abilityDestination = home;
                _abilityStarted = now;
                return {JourneyStatus::Submitted, {}, "casting the owned hearthstone through normal item use"};
            }
            std::uint32_t selected = 0;
            std::optional<WorldPosition> arrival;
            float nearest = 2000.0f;
            for (auto const& [id, known] : bot->GetSpellMap())
            {
                if (!known || known->State == PLAYERSPELL_REMOVED || !known->Active ||
                    !known->IsInSpec(bot->GetActiveSpec()))
                    continue;
                auto* spell = sSpellMgr->GetSpellInfo(id);
                if (!spell || spell->IsPassive() || !spell->HasEffect(SPELL_EFFECT_TELEPORT_UNITS))
                    continue;
                for (std::uint8_t index = 0; index < MAX_SPELL_EFFECTS; ++index)
                {
                    if (spell->Effects[index].Effect != SPELL_EFFECT_TELEPORT_UNITS)
                        continue;
                    std::optional<WorldPosition> target;
                    if (auto* position = sSpellMgr->GetSpellTargetPosition(id, static_cast<SpellEffIndex>(index)))
                        target = WorldPosition(position->target_mapId, position->target_X,
                                               position->target_Y, position->target_Z);
                    else if (spell->Effects[index].TargetA.GetTarget() == TARGET_DEST_HOME ||
                             spell->Effects[index].TargetB.GetTarget() == TARGET_DEST_HOME)
                        target = home;
                    if (!target || target->GetMapId() != destination.GetMapId())
                        continue;
                    auto distance = target->GetExactDist(destination);
                    if ((distance < nearest || (distance == nearest && id < selected)) && ai->CanCastSpell(id, bot))
                    {
                        selected = id;
                        arrival = target;
                        nearest = distance;
                    }
                }
            }
            if (selected && arrival && ai->CastSpell(selected, bot))
            {
                _abilityDestination = arrival;
                _abilityStarted = now;
                return {JourneyStatus::Submitted, {}, "casting a learned class travel ability with native costs and cooldowns"};
            }
        }
        if (!bot->GetTransport() && now >= _nextFlightPlan &&
            (bot->GetMapId() != destination.GetMapId() || bot->GetExactDist(destination) > 3000.0f))
        {
            _nextFlightPlan = now + 180;
            TravelMgr::FlightMasterInfo origin;
            if (sTravelMgr.GetFlightToward(bot, destination, origin, _flightRoute))
            {
                auto* arrival = sTaxiNodesStore.LookupEntry(_flightRoute.back());
                if (!arrival)
                    throw std::runtime_error("ADVAI known taxi route references a missing destination");
                TravelTransfer flight{TransferKind::Flight, origin.templateEntry, WorldPosition(origin.pos),
                    WorldPosition(arrival->map_id, arrival->x, arrival->y, arrival->z)};
                if (!_excluded.contains(TransferKey(flight)))
                    _transfer = flight;
            }
        }
    }
    if (!_transfer)
    {
        float best = bot->GetMapId() == destination.GetMapId() ? bot->GetExactDist(destination) * 0.65f
                                                            : std::numeric_limits<float>::max();
        if (auto found = transfers.find(bot->GetMapId()); found != transfers.end())
            for (auto const& edge : found->second)
            {
                if (_excluded.contains(TransferKey(edge)))
                    continue;
                if (auto* map = sMapStore.LookupEntry(edge.To.GetMapId());
                    !map || !bot->GetSession() || map->Expansion() > bot->GetSession()->Expansion())
                    continue;
                if (edge.Kind == TransferKind::GameObject)
                    if (auto* object = sObjectMgr->GetGameObjectTemplate(edge.Entry))
                        if (auto* addon = sObjectMgr->GetGameObjectTemplateAddon(edge.Entry))
                            if (auto* faction = sFactionTemplateStore.LookupEntry(addon->faction);
                                faction && bot->GetFactionTemplateEntry()->IsHostileTo(*faction))
                                continue;
                if (auto* boat = bot->GetTransport();
                    boat && (edge.Kind != TransferKind::Transport || edge.Entry != boat->GetEntry()))
                    continue;
                if (!bot->GetTransport() && edge.From.GetMapId() == edge.To.GetMapId() &&
                    edge.From.GetExactDist(edge.To) < 80.0f)
                    continue;
                auto hops = RemainingHops(edge.To.GetMapId(), destination.GetMapId());
                if (hops >= 100)
                    continue;
                float cost = (bot->GetTransport() ? 0.0f : bot->GetExactDist(edge.From)) + hops * 5000.0f +
                             (edge.Kind == TransferKind::Transport ? 1000.0f : 200.0f);
                if (!hops)
                    cost += edge.To.GetExactDist(destination);
                if (cost < best)
                {
                    best = cost;
                    _transfer = edge;
                }
            }
        if (!_transfer)
            return bot->GetMapId() == destination.GetMapId()
                       ? JourneyStep{JourneyStatus::Walk, destination, "walking along a normal ground route"}
                       : JourneyStep{JourneyStatus::Blocked, {}, "no known native portal or transport connects the destination"};
    }
    auto const& edge = *_transfer;
    if (_submittedAt)
    {
        if (now < _submittedAt + 10)
            return {JourneyStatus::Wait, {}, "waiting for the submitted native travel operation"};
        if (_excluded.size() >= 32)
            throw std::runtime_error("ADVAI journey exhausted its bounded route exclusions");
        _excluded[TransferKey(edge)] = now + 300;
        Reset();
        return {JourneyStatus::Blocked, {}, "native travel operation did not complete; route is excluded before replanning"};
    }
    if (edge.Kind == TransferKind::Flight)
    {
        if (bot->GetExactDist(edge.From) > INTERACTION_DISTANCE)
            return {JourneyStatus::Walk, edge.From, "walking to the known flight master"};
        auto* master = bot->FindNearestCreature(edge.Entry, INTERACTION_DISTANCE);
        bool queued = master && PlayerbotWorldThreadProcessor::instance().QueueOperation(
            std::make_unique<FlightOperation>(bot->GetGUID(), master->GetGUID(), _flightRoute, purpose));
        if (queued)
            _submittedAt = now;
        return {queued ? JourneyStatus::Submitted : JourneyStatus::Blocked, {},
                queued ? "paid known taxi route submitted" : "known flight master is unavailable or its operation queue is full"};
    }
    if (edge.Kind != TransferKind::Transport)
    {
        bool reached = bot->GetExactDist(edge.From) <= INTERACTION_DISTANCE;
        if (edge.Kind == TransferKind::AreaTrigger)
        {
            auto* trigger = sObjectMgr->GetAreaTrigger(edge.Entry);
            if (!trigger)
                throw std::runtime_error("ADVAI planned area trigger disappeared from the native catalog");
            reached = bot->IsInAreaTriggerRadius(trigger);
        }
        if (!reached)
            return {JourneyStatus::Walk, edge.From, "walking to a real portal entrance"};
        bool queued = false;
        if (edge.Kind == TransferKind::AreaTrigger)
            queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<AreaTriggerUseOperation>(bot->GetGUID(), edge.Entry, purpose));
        else if (auto* portal = bot->FindNearestGameObject(edge.Entry, INTERACTION_DISTANCE))
            queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<PortalUseOperation>(bot->GetGUID(), portal->GetGUID(), purpose));
        if (queued)
            _submittedAt = now;
        return {queued ? JourneyStatus::Submitted : JourneyStatus::Blocked, {},
                queued ? "normal portal use submitted" : "planned portal is unavailable or its operation queue is full"};
    }
    if (auto* current = bot->GetTransport())
    {
        auto* boat = dynamic_cast<MotionTransport*>(current);
        if (!boat || boat->GetEntry() != edge.Entry)
            return {JourneyStatus::Blocked, {}, "current transport does not match the planned route"};
        _transport = boat->GetGUID();
        if (!_ridingSince)
            _ridingSince = now;
        if (now > _ridingSince + std::max<std::uint32_t>(600, boat->GetPeriod() / 1000 * 3))
            return {JourneyStatus::Blocked, {}, "transport failed to provide a safe destination landing within three circuits"};
        if (StoppedAt(boat, edge.To))
            if (auto point = BoardingPoint(bot, boat, true))
                return {JourneyStatus::Board, point, "walking off the docked transport onto verified ground"};
        return {JourneyStatus::Riding, {}, "riding the actual transport until the destination stop"};
    }
    if (!_dock)
        _dock = DockPoint(bot, edge.From);
    if (!_dock)
    {
        std::optional<WorldPosition> approach;
        float nearest = 300.0f;
        if (bot->GetExactDist(edge.From) > 150.0f)
            if (auto known = portApproaches.find(bot->GetMapId()); known != portApproaches.end())
                for (auto const& place : known->second)
                    if (auto* faction = sFactionTemplateStore.LookupEntry(place.Faction);
                        faction && !bot->GetFactionTemplateEntry()->IsHostileTo(*faction))
                    {
                        auto distance = edge.From.GetExactDist(place.X, place.Y, place.Z);
                        if (distance < nearest)
                        {
                            approach = WorldPosition(place.Map, place.X, place.Y, place.Z);
                            nearest = distance;
                        }
                    }
        if (approach)
            return {JourneyStatus::Walk, approach, "approaching a known harbor or tower before inspecting its dock"};
        return {JourneyStatus::Blocked, {}, "transport stop has no verified dry waiting location or reachable approach"};
    }
    if (bot->GetExactDist(*_dock) > 2.0f)
        return {JourneyStatus::Walk, *_dock, "walking to the transport's actual dock"};
    if (!_waitingSince)
        _waitingSince = now;
    if (now > _waitingSince + 600)
    {
        _excluded[TransferKey(edge)] = now + 300;
        Reset();
        return {JourneyStatus::Blocked, {}, "transport boarding timed out; diagnose the dock before retrying"};
    }
    for (auto* transport : bot->GetMap()->GetAllTransports())
        if (auto* boat = dynamic_cast<MotionTransport*>(transport);
            boat && boat->GetEntry() == edge.Entry && bot->InSamePhase(boat) && StoppedAt(boat, edge.From))
            if (auto point = BoardingPoint(bot, boat, false))
            {
                _transport = boat->GetGUID();
                return {JourneyStatus::Board, point, "walking onto the docked vessel across verified surfaces"};
            }
    return {JourneyStatus::Wait, {}, "waiting at the dock for a boardable scheduled vessel"};
}
bool Journey::Travel(PlayerbotAI* ai, WorldPosition const& destination, Goal purpose,
                     std::function<bool(WorldPosition const&)> const& walk)
{
    auto* bot = ai->GetBot();
    if (!ai->AllowActivity() || !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData())
        return false;
    auto step = Step(ai, destination, purpose);
    bool acted = false;
    switch (step.Status)
    {
        case JourneyStatus::Walk:
            if (!step.Point)
                throw std::runtime_error("ADVAI travel walk has no destination");
            acted = walk(*step.Point);
            if (!acted)
            {
                if (_transfer)
                {
                    if (_excluded.size() >= 32)
                        throw std::runtime_error("ADVAI travel path exclusions exhausted");
                    _excluded[TransferKey(*_transfer)] = static_cast<Timestamp>(time(nullptr)) + 300;
                }
                Reset();
                step.Status = JourneyStatus::Blocked;
                step.Reason = "normal pathfinding failed for the selected travel step";
            }
            break;
        case JourneyStatus::Board:
            if (!step.Point || !bot->GetMotionMaster())
                throw std::runtime_error("ADVAI boarding step has no movement destination or motion controller");
            bot->GetMotionMaster()->MovePoint(0, step.Point->GetPositionX(), step.Point->GetPositionY(),
                step.Point->GetPositionZ(), FORCED_MOVEMENT_NONE, 0.0f, 0.0f, false, false);
            ai->SetNextCheckDelay(1000);
            acted = true;
            break;
        case JourneyStatus::Wait:
        case JourneyStatus::Riding:
            if (!bot->IsInFlight() && !bot->IsBeingTeleported())
                bot->StopMovingOnCurrentPos();
            ai->SetNextCheckDelay(1000);
            acted = true;
            break;
        case JourneyStatus::Submitted:
            acted = true;
            break;
        case JourneyStatus::Arrived:
            break;
        case JourneyStatus::Blocked:
            if (_transfer && !bot->GetTransport())
            {
                if (_excluded.size() >= 32)
                    throw std::runtime_error("ADVAI journey exhausted its bounded failed-route records");
                _excluded[TransferKey(*_transfer)] = static_cast<Timestamp>(time(nullptr)) + 300;
                Reset();
            }
            break;
    }
    Manager::Instance().Report(bot->GetGUID().GetCounter(),
        {purpose, step.Status == JourneyStatus::Blocked ? Outcome::Blocked : Outcome::Started, step.Reason});
    return acted;
}
}  // namespace Advai
