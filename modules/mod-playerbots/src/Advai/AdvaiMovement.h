/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_ADVAI_MOVEMENT_H
#define PLAYERBOTS_ADVAI_MOVEMENT_H

#include <memory>
#include <mutex>
#include "AdvaiModel.h"
#include "MoveSplineInitArgs.h"
#include "ObjectGuid.h"

class PlayerbotAI;
class Unit;

namespace Advai
{
struct MovementState
{
    struct FamiliarRoute
    {
        std::uint32_t Map = 0;
        Movement::PointsArray Points;
        Timestamp Used = 0;
    };
    struct Exclusion
    {
        G3D::Vector3 Position;
        Timestamp Until = 0;
    };
    // Destinations whose ground routes failed repeatedly; skipped until Until instead of replanned every 15 s.
    struct BlockedDestination
    {
        std::uint32_t Map = 0;
        G3D::Vector3 Position{0, 0, 0};
        std::uint32_t Failures = 0;
        Timestamp Until = 0;
    };
    std::mutex Mutex;
    BotPersona Persona;
    std::deque<FamiliarRoute> Familiar;
    std::deque<Exclusion> Exclusions;
    std::deque<BlockedDestination> Blocked;
    std::uint32_t Map = 0;
    std::uint32_t Spline = 0;
    G3D::Vector3 Destination{0, 0, 0};
    G3D::Vector3 LastPosition{0, 0, 0};
    G3D::Vector3 LastSafe{0, 0, 0};
    G3D::Vector3 LookBehindPosition{0, 0, 0};
    Timestamp ProgressAt = 0;
    Timestamp Started = 0;
    Timestamp RetryAt = 0;
    Timestamp PauseUntil = 0;
    Timestamp LastPause = 0;
    Goal PausedGoal = Goal::None;
    ObjectGuid Facing;
    float Speed = 0;
    std::uint8_t Recoveries = 0;
    bool Active = false;
    bool Precise = false;
    bool Backwards = false;
    bool Combat = false;
    bool Replanning = false;
    bool LookBehindPending = false;
    bool LookBehindActive = false;
    std::uint32_t Routes = 0;
    std::uint32_t Reused = 0;
    std::uint32_t RoundedCorners = 0;
    std::uint32_t RouteDrifts = 0;
    std::uint32_t LookBehinds = 0;
    std::uint32_t SwungTurns = 0;
    std::uint32_t Overshoots = 0;
    std::uint32_t Undershoots = 0;
    float Hurry = -1.0f;  // slow-drifting 0..1 mood: low = leisurely and loose, high = hurried and tight
    std::uint32_t FailedRoutes = 0;
    std::uint32_t RecoveryMoves = 0;
};

// Empty means an activity belongs to the native vehicle, transport, flight or
// human-control path. Managed ground routes return an explicit success/failure.
std::optional<bool> MovePlayerGround(PlayerbotAI* ai, std::uint32_t map, float x, float y, float z, bool precise,
                                     bool backwards, bool combat, Unit* facing = nullptr);
void UpdatePlayerMovement(PlayerbotAI* ai);
bool BeginLookBehind(PlayerbotAI* ai, float& turn);
bool TravelLookahead(PlayerbotAI* ai, float distance, G3D::Vector3& point);
void PausePlayerTravel(PlayerbotAI* ai, Timestamp seconds);
// Halt a managed ground walk for a few seconds so a cast-time spell can be cast (casting while moving is refused).
bool StopForCast(PlayerbotAI* ai, Timestamp seconds);
// Halt movement and idle the bot's AI while it "types" a chat reply, then let the route resume.
void StopForTyping(PlayerbotAI* ai, std::uint32_t milliseconds);
bool PlayerRouteProgressing(PlayerbotAI* ai);
bool PlayerRouteActive(PlayerbotAI* ai);
std::string DescribeMovement(std::shared_ptr<MovementState> const& state);
// Population-wide histograms of the humanising behaviours (for measuring variety, not for gameplay).
std::string DescribeMovementStats(bool reset);
std::string MovementTelemetry();
}
#endif
