/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_TRAVEL_H
#define PLAYERBOTS_ADVAI_TRAVEL_H

#include <functional>

#include "AdvaiModel.h"
#include "TravelMgr.h"

class PlayerbotAI;
class Player;
class Transport;

namespace Advai
{
enum class TransferKind : std::uint8_t { AreaTrigger, GameObject, Transport, Flight };
struct TravelTransfer
{
    TransferKind Kind = TransferKind::AreaTrigger;
    std::uint32_t Entry = 0;
    WorldPosition From;
    WorldPosition To;
};
enum class JourneyStatus : std::uint8_t { Walk, Board, Wait, Riding, Submitted, Arrived, Blocked };
struct JourneyStep
{
    JourneyStatus Status = JourneyStatus::Blocked;
    std::optional<WorldPosition> Point;
    std::string Reason;
};

// Populated on the world thread before bots start; route records contain no object pointers.
void InitializeTravelNetwork();
bool HasTravelConnection(std::uint32_t from, std::uint32_t to);
bool OnTransportSurface(Player* bot, Transport* transport);
class Journey
{
public:
    JourneyStep Step(PlayerbotAI* ai, WorldPosition const& destination, Goal purpose);
    bool Travel(PlayerbotAI* ai, WorldPosition const& destination, Goal purpose,
                std::function<bool(WorldPosition const&)> const& walk);
    void Reset();

private:
    std::optional<TravelTransfer> _transfer;
    std::optional<WorldPosition> _destination;
    std::optional<WorldPosition> _dock;
    ObjectGuid _transport;
    Timestamp _waitingSince = 0;
    Timestamp _submittedAt = 0;
    Timestamp _ridingSince = 0;
    Timestamp _nextFlightPlan = 0;
    Timestamp _nextAbilityPlan = 0;
    Timestamp _abilityStarted = 0;
    std::optional<WorldPosition> _abilityDestination;
    std::vector<std::uint32_t> _flightRoute;
    std::unordered_map<std::uint64_t, Timestamp> _excluded;
};
}  // namespace Advai

#endif
