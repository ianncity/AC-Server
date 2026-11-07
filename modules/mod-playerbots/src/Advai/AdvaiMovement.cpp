/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#include "AdvaiMovement.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <ctime>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "AdvaiMgr.h"
#include "AdvaiLifeStrategy.h"
#include "AdvaiPvp.h"
#include "AiObjectContext.h"
#include "Event.h"
#include "IVMapMgr.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PvpTriggers.h"
#include "Random.h"

namespace Advai
{
namespace
{
Timestamp Now() { return static_cast<Timestamp>(time(nullptr)); }
G3D::Vector3 PositionOf(Unit* unit) { return {unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ()}; }
float Distance(G3D::Vector3 const& a, G3D::Vector3 const& b) { return (a - b).length(); }
float Length(Movement::PointsArray const& path)
{
    float result = 0;
    for (std::size_t i = 1; i < path.size(); ++i)
        result += Distance(path[i - 1], path[i]);
    return result;
}

// Population histograms: lets us check that behaviour is spread out instead of clustered on a few values.
struct Histogram
{
    static constexpr unsigned Bins = 12;
    std::atomic<std::uint64_t> Count[Bins]{};
    void Add(float value, float width)
    {
        unsigned bin = std::min<unsigned>(Bins - 1, static_cast<unsigned>(std::max(0.0f, value / width)));
        Count[bin].fetch_add(1, std::memory_order_relaxed);
    }
    std::string Describe(float width, char const* unit)
    {
        std::ostringstream out;
        for (unsigned i = 0; i < Bins; ++i)
            if (auto n = Count[i].load())
                out << " [" << i * width << unit << "+]=" << n;
        return out.str();
    }
    void Reset() { for (auto& c : Count) c = 0; }
};
Histogram OvershootYards, UndershootYards, CornerRadius, SwingTurnRadians, HurryDistribution;
std::atomic<std::uint64_t> RoutesLaunched{0}, RoutesExact{0}, HabitTrips[6]{};
std::atomic<std::uint64_t> PathsStarted{0}, ProgressSamples{0};

// Mood drifts a little every trip (mean-reverting random walk) so one bot is not the same person every walk.
float DriftHurry(MovementState& state)
{
    if (state.Hurry < 0.0f)
        state.Hurry = 0.2f + static_cast<float>(state.Persona.Seed % 61) / 100.0f;
    float const baseline = 0.25f + static_cast<float>((state.Persona.Seed >> 5) % 51) / 100.0f;
    float const noise = (static_cast<float>(urand(0, 1000)) / 1000.0f - 0.5f) * 0.45f;
    state.Hurry = std::clamp(state.Hurry + (baseline - state.Hurry) * 0.12f + noise, 0.0f, 1.0f);
    HurryDistribution.Add(state.Hurry, 1.0f / Histogram::Bins);
    return state.Hurry;
}
float Jitter(float low, float high) { return low + (high - low) * static_cast<float>(urand(0, 1000)) / 1000.0f; }

bool GroundEligible(PlayerbotAI* ai)
{
    auto* bot = ai->GetBot();
    return Manager::Instance().Applies(bot) && !ai->HasGameClientMaster() && bot->IsAlive() &&
        !bot->GetVehicle() && !bot->GetTransport() && !bot->IsInFlight() && !bot->IsFlying() &&
        !bot->isSwimming() && !bot->IsInWater() && !bot->IsBeingTeleported() &&
        !bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_NOT_MOVE);
}

bool NativePath(Player* bot, G3D::Vector3 const& to, Movement::PointsArray& result)
{
    PathGenerator path(bot);
    if (!path.CalculatePath(to.x, to.y, to.z, false) || path.GetPathType() != PATHFIND_NORMAL ||
        path.GetPath().size() < 2 || path.GetPath().size() > 512 ||
        Distance(path.GetActualEndPosition(), to) > 1.0f)
        return false;
    result = path.GetPath();
    return true;
}

bool ClearSegment(Player* bot, G3D::Vector3 const& a, G3D::Vector3 const& b)
{
    if (std::abs(a.z - b.z) > 0.65f || Distance(a, b) > 8.0f ||
        !bot->GetMap()->isInLineOfSight(a.x, a.y, a.z + 1.0f, b.x, b.y, b.z + 1.0f, bot->GetPhaseMask(),
                                       LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
        return false;
    for (unsigned sample = 1; sample <= 3; ++sample)
    {
        auto p = a + (b - a) * (static_cast<float>(sample) / 3.0f);
        auto ground = bot->GetMap()->GetHeight(bot->GetPhaseMask(), p.x, p.y, p.z + 1.0f, true, 2.0f);
        if (ground <= INVALID_HEIGHT || std::abs(ground - p.z) > 0.45f ||
            bot->GetMap()->IsInWater(bot->GetPhaseMask(), p.x, p.y, ground, bot->GetCollisionHeight()))
            return false;
    }
    // A short normal path must stay close to the proposed segment; LOS alone
    // cannot establish navigability over a hole or across disconnected polygons.
    PathGenerator check(bot);
    check.SetUseRaycast(true);
    if (!check.CalculatePath(a.x, a.y, a.z, b.x, b.y, b.z, false) ||
        check.GetPathType() != PATHFIND_NORMAL || check.getPathLength() > Distance(a, b) + 0.3f)
        return false;
    if (Distance(check.GetActualEndPosition(), b) > 0.5f)
        return false;
    float cx = b.x, cy = b.y, cz = b.z;
    if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, a.x, a.y, a.z, cx, cy, cz))
        return false;
    return Distance({cx, cy, cz}, b) < 0.3f;
}

void RoundCorners(Player* bot, MovementState& state, Movement::PointsArray& path)
{
    if (state.Precise || state.Backwards || path.size() < 3 || path.size() > 160 || bot->GetMap()->IsDungeon() ||
        !bot->IsOutdoors())
        return;
    Movement::PointsArray rounded;
    rounded.push_back(path.front());
    unsigned examined = 0;
    // Stable input styles: tighter mouse-like turns, broader keyboard-like turns,
    // and restrained autorun corrections. No random displacement of path points.
    float radius = (state.Persona.Seed % 3 == 0   ? 0.55f
                    : state.Persona.Seed % 3 == 1 ? 1.6f
                                                  : 0.9f) *
                   (1.35f - 0.7f * DriftHurry(state)) * Jitter(0.75f, 1.3f);
    for (std::size_t i = 1; i + 1 < path.size(); ++i)
    {
        auto const& b = path[i];
        auto incoming = b - rounded.back(), outgoing = path[i + 1] - b;
        float before = incoming.length(), after = outgoing.length();
        bool accepted = false;
        if (before > 0.8f && after > 0.8f && examined < 12 && std::abs(incoming.z) < 0.5f && std::abs(outgoing.z) < 0.5f)
        {
            auto u = incoming / before, v = outgoing / after;
            float cosine = u.dot(v);
            if (cosine > -0.9f && cosine < 0.98f)
            {
                ++examined;
                // Sharper turns swing wider, like a hand easing the camera round instead of snapping to the new
                // heading.
                float const sharpness = 1.0f + std::max(0.0f, -cosine) * 1.6f;
                float cut = std::min({radius * sharpness, before * 0.3f, after * 0.3f});
                auto a = b - u * cut, c = b + v * cut;
                Movement::PointsArray curve{a};
                for (unsigned j = 1; j <= 12; ++j)
                {
                    float t = static_cast<float>(j) / 12.0f;
                    curve.push_back(a * ((1 - t) * (1 - t)) + b * (2 * t * (1 - t)) + c * (t * t));
                }
                // The approach to A is a portion of the existing validated edge.
                // Only the newly introduced curve needs additional raycasts.
                accepted = true;
                for (std::size_t j = 1; accepted && j < curve.size(); ++j)
                    accepted = ClearSegment(bot, curve[j - 1], curve[j]);
                if (accepted)
                {
                    rounded.insert(rounded.end(), curve.begin(), curve.end());
                    ++state.RoundedCorners;
                    CornerRadius.Add(cut, 0.25f);
                }
            }
        }
        if (!accepted)
            rounded.push_back(b);
    }
    rounded.push_back(path.back());
    path = std::move(rounded);
}

// Gently drift across a straight, open corridor while retaining its original endpoints.
void DriftAlongRoute(Player* bot, MovementState& state, Movement::PointsArray& path)
{
    if (state.Precise || state.Backwards || state.Combat || bot->IsInCombat() || bot->GetMap()->IsDungeon() ||
        !bot->IsOutdoors() || path.size() < 3 || path.size() > 160 || urand(0, 99) >= 35)
        return;

    for (std::size_t start = 0; start + 2 < path.size() && start < 32; ++start)
    {
        std::size_t end = start + 1;
        while (end + 1 < path.size() && Distance(path[start], path[end]) < 8.0f)
            ++end;
        auto const leg = path[end] - path[start];
        float const length = leg.length();
        if (length < 6.0f || length > 12.0f || std::abs(leg.z) > 0.4f)
            continue;
        bool straight = true;
        for (std::size_t i = start + 1; i < end; ++i)
        {
            float const lateral =
                std::abs((path[i].x - path[start].x) * leg.y - (path[i].y - path[start].y) * leg.x) / length;
            if (lateral > 0.35f || std::abs(path[i].z - path[start].z) > 0.4f)
            {
                straight = false;
                break;
            }
        }
        if (!straight)
            continue;
        float const offset = (urand(0, 1) ? 1.0f : -1.0f) * Jitter(0.3f, 0.9f);
        G3D::Vector3 const side(-leg.y / length, leg.x / length, 0.0f);
        Movement::PointsArray curve{path[start]};
        bool valid = true;
        for (unsigned sample = 1; sample <= 4; ++sample)
        {
            float const t = static_cast<float>(sample) / 4.0f;
            auto point = path[start] + leg * t + side * (offset * 4.0f * t * (1.0f - t));
            if (sample < 4)
            {
                float const ground =
                    bot->GetMap()->GetHeight(bot->GetPhaseMask(), point.x, point.y, point.z + 1.0f, true, 2.0f);
                if (ground <= INVALID_HEIGHT || std::abs(ground - point.z) > 0.4f)
                {
                    valid = false;
                    break;
                }
                point.z = ground;
            }
            if (!ClearSegment(bot, curve.back(), point))
            {
                valid = false;
                break;
            }
            curve.push_back(point);
        }
        if (valid)
        {
            path.erase(path.begin() + start + 1, path.begin() + end);
            path.insert(path.begin() + start + 1, curve.begin() + 1, curve.end() - 1);
            ++state.RouteDrifts;
            return;
        }
    }
}

// Index of the first point at least `distance` yards along the path (last point if the path is shorter).
std::size_t PointAtDistance(Movement::PointsArray const& path, float distance)
{
    float travelled = 0.0f;
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        travelled += Distance(path[i - 1], path[i]);
        if (travelled >= distance)
            return i;
    }
    return path.size() - 1;
}

// Humans do not pivot on the spot and then walk: they start moving while the camera/character swings round, so
// a large heading change becomes a short arc that leaves along the bot's current facing and blends into the first
// leg. About one persona in four keeps the hard keyboard pivot. Every added segment is validated like a rounded corner.
void SwingIntoRoute(Player* bot, MovementState& state, Movement::PointsArray& path)
{
    if (state.Precise || state.Backwards || state.Combat || path.size() < 2 || path.size() > 200 ||
        state.Persona.Seed % 4 == 0 || bot->IsInCombat() || bot->GetMap()->IsDungeon() || !bot->IsOutdoors())
        return;
    G3D::Vector3 const start = path.front();
    std::size_t const joinIndex = PointAtDistance(path, 6.0f);
    G3D::Vector3 const join = path[joinIndex];
    G3D::Vector3 const leg = join - start;
    float const legLength = leg.length();
    if (legLength < 4.0f || std::abs(leg.z) > 0.8f)
        return;
    float const wanted = std::atan2(leg.y, leg.x);
    float diff = Position::NormalizeOrientation(wanted - bot->GetOrientation());
    if (diff > static_cast<float>(M_PI))
        diff -= 2.0f * static_cast<float>(M_PI);
    float const turn = std::abs(diff);
    // Under ~35 degrees is ordinary steering; a hurried mover also skips more of the swing.
    if (turn < 0.6f)
        return;
    float const lead =
        std::min((1.6f + static_cast<float>(state.Persona.Seed % 3) * 0.6f) * Jitter(0.7f, 1.4f), legLength * 0.45f);
    G3D::Vector3 const heading(std::cos(bot->GetOrientation()), std::sin(bot->GetOrientation()), 0.0f);
    G3D::Vector3 const control = start + heading * lead;
    Movement::PointsArray arc{start};
    for (unsigned j = 1; j <= 8; ++j)
    {
        float const t = static_cast<float>(j) / 8.0f;
        G3D::Vector3 point = start * ((1 - t) * (1 - t)) + control * (2 * t * (1 - t)) + join * (t * t);
        if (j < 8)
        {
            float const ground =
                bot->GetMap()->GetHeight(bot->GetPhaseMask(), point.x, point.y, start.z + 1.0f, true, 2.0f);
            if (ground <= INVALID_HEIGHT || std::abs(ground - start.z) > 0.6f)
                return;
            point.z = ground;
        }
        if (!ClearSegment(bot, arc.back(), point))
            return;
        arc.push_back(point);
    }
    arc.insert(arc.end(), path.begin() + joinIndex + 1, path.end());
    path = std::move(arc);
    ++state.SwungTurns;
    SwingTurnRadians.Add(turn, 0.3f);
}

// People differ in how they finish a walk. Each persona has a habit: some stop on the spot, some run past and curve
// back, some stop a step short and nudge forward, and some vary trip to trip. Only long, unhurried routes are touched
// and every added point is validated like a rounded corner. An undershoot is repaired by the next ordinary move
// request (the bot simply takes the last step), an overshoot returns to the exact destination inside the same spline.
void MisjudgeDestination(Player* bot, MovementState& state, Movement::PointsArray& path)
{
    if (state.Precise || state.Backwards || state.Combat || path.size() < 2 || path.size() > 200 || bot->IsInCombat() ||
        bot->GetMap()->IsDungeon() || !bot->IsOutdoors() || Length(path) < 22.0f)
        return;
    float const hurry = DriftHurry(state);
    // Persona sets the usual habit; any given trip has a one-in-four chance of going another way.
    unsigned habit = (state.Persona.Seed >> 3) % 6;  // 0 exact, 1-2 overshoot, 3-4 undershoot, 5 varies
    if (urand(0, 3) == 0)
        habit = urand(0, 5);
    // Hurried people misjudge more often and by more; relaxed people are neater.
    float const sloppiness = (8.0f + (100.0f - std::min<unsigned>(state.Persona.Skill, 100)) / 4.0f) * (0.5f + hurry);
    ++RoutesLaunched;
    ++HabitTrips[habit];
    if (habit == 0 || static_cast<float>(urand(0, 99)) >= sloppiness)
    {
        ++RoutesExact;
        return;
    }
    unsigned const roll = urand(0, 99);
    bool over = habit <= 2 || (habit == 5 && (roll & 1));
    G3D::Vector3 const goal = path.back();
    // Direction of travel over the last ~5 yards (single path segments are often under a yard long).
    std::size_t backIndex = path.size() - 1;
    float walked = 0.0f;
    while (backIndex > 0 && walked < 5.0f)
    {
        walked += Distance(path[backIndex - 1], path[backIndex]);
        --backIndex;
    }
    G3D::Vector3 incoming = goal - path[backIndex];
    incoming.z = 0.0f;
    float const length = incoming.length();
    if (length < 3.0f)
        return;
    incoming /= length;
    float const amount = Jitter(0.5f, 2.3f) * (0.6f + hurry * 0.8f);  // a fresh amount every trip
    if (!over)
    {
        // Stop short: walk back along the route itself so the new end stays on the validated corridor.
        float remaining = std::min(amount, walked * 0.6f);
        float const shortBy = remaining;
        while (path.size() > 2 && Distance(path[path.size() - 2], path.back()) <= remaining)
        {
            remaining -= Distance(path[path.size() - 2], path.back());
            path.pop_back();
        }
        G3D::Vector3 const tail = path.back() - path[path.size() - 2];
        float const tailLength = tail.length();
        if (tailLength < 0.05f || remaining >= tailLength)
            return;
        path.back() = path.back() - tail * (remaining / tailLength);
        ++state.Undershoots;
        UndershootYards.Add(shortBy, 0.25f);
        return;
    }
    G3D::Vector3 const side(-incoming.y, incoming.x, 0.0f);
    // Try the chosen overshoot, then the other side, then a shorter one: a wall or a drop on one side shouldn't mean
    // that bot never overshoots.
    float const preferred = (urand(0, 1) ? 1.0f : -1.0f) * Jitter(0.2f, 0.9f);
    bool placed = false;
    float usedAmount = amount;
    for (unsigned attempt = 0; attempt < 3; ++attempt)
    {
        float const sway = attempt == 1 ? -preferred : preferred;
        float const reach = attempt == 2 ? amount * 0.5f : amount;
        G3D::Vector3 points[3] = {goal + incoming * reach, goal + incoming * (reach * 0.35f) + side * sway, goal};
        Movement::PointsArray extra;
        G3D::Vector3 last = goal;
        bool valid = true;
        for (auto& point : points)
        {
            if (point != goal)
            {
                float const ground = bot->GetMap()->GetHeight(bot->GetPhaseMask(), point.x, point.y, goal.z + 1.0f, true, 2.0f);
                if (ground <= INVALID_HEIGHT || std::abs(ground - goal.z) > 0.5f)
                {
                    valid = false;
                    break;
                }
                point.z = ground;
            }
            if (!ClearSegment(bot, last, point))
            {
                valid = false;
                break;
            }
            extra.push_back(point);
            last = point;
        }
        if (valid)
        {
            path.insert(path.end(), extra.begin(), extra.end());
            usedAmount = reach;
            placed = true;
            break;
        }
    }
    if (!placed)
        return;
    ++state.Overshoots;
    OvershootYards.Add(usedAmount, 0.25f);
}

void Launch(Player* bot, MovementState& state, Movement::PointsArray path, Unit* facing)
{
    // One uninterrupted native spline traverses the whole validated corridor.
    // Linear segments are deliberately retained: unconstrained spline smoothing
    // can overshoot walls and ledges even when its control points are on the mesh.
    bot->GetMotionMaster()->Clear(false);
    bot->GetMotionMaster()->MoveIdle();
    if (bot->IsSitState())
        bot->SetStandState(UNIT_STAND_STATE_STAND);
    if (facing)
        bot->SetFacingToObject(facing);
    Movement::MoveSplineInit init(bot);
    init.MovebyPath(path);
    init.SetWalk(bot->IsWalking());
    if (state.Backwards)
        init.SetOrientationInversed();
    if (facing)
    {
        init.SetFacing(facing);
        init.SetOrientationFixed(true);
    }
    init.Launch();
    ++PathsStarted;
    state.Spline = bot->movespline->GetId();
    state.Speed = bot->GetSpeed(state.Backwards ? MOVE_RUN_BACK : bot->IsWalking() ? MOVE_WALK : MOVE_RUN);
    state.Facing = facing ? facing->GetGUID() : ObjectGuid::Empty;
    state.LastPosition = PositionOf(bot);
    state.ProgressAt = Now();
    state.Started = Now();
    state.Active = true;
    ++state.Routes;
}

// A destination is blacklisted after this many consecutive ground routes with no complete corridor.
constexpr std::uint32_t BLOCK_AFTER_FAILURES = 3;
constexpr Timestamp BLOCK_SECONDS = 600;
constexpr std::size_t MAX_BLOCKED_DESTINATIONS = 16;

MovementState::BlockedDestination* FindBlocked(MovementState& state, std::uint32_t map,
                                               G3D::Vector3 const& destination)
{
    for (auto& entry : state.Blocked)
        if (entry.Map == map && Distance(entry.Position, destination) < 2.0f)
            return &entry;
    return nullptr;
}

float RouteScore(Movement::PointsArray const& path, MovementState const& state,
                 std::vector<G3D::Vector3> const& threats, Timestamp now)
{
    float score = Length(path);
    for (std::size_t i = 0; i < path.size(); i += std::max<std::size_t>(1, path.size() / 12))
    {
        for (auto const& threat : threats)
            score += std::max(0.0f, 18.0f - Distance(path[i], threat)) * (100 - state.Persona.RiskTolerance) / 100.0f;
        for (auto const& excluded : state.Exclusions)
            if (now < excluded.Until)
                score += std::max(0.0f, 8.0f - Distance(path[i], excluded.Position)) * 12.0f;
    }
    return score;
}
}

std::shared_ptr<MovementState> Manager::MovementFor(BotId id)
{
    auto state = Get(id);
    if (!state)
        return {};
    std::lock_guard lock(state->Mutex);
    if (!state->Movement)
    {
        state->Movement = std::make_shared<MovementState>();
        state->Movement->Persona = state->Persona;
    }
    return state->Movement;
}

std::optional<bool> MovePlayerGround(PlayerbotAI* ai, std::uint32_t map, float x, float y, float z,
                                     bool precise, bool backwards, bool combat, Unit* facing)
{
    if (!GroundEligible(ai))
        return std::nullopt;
    auto* bot = ai->GetBot();
    auto& manager = Manager::Instance();
    if (!manager.Active(bot->GetGUID().GetCounter()) || map != bot->GetMapId() ||
        bot->IsMovementPreventedByCasting() ||
        bot->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_CONTROLLED) != NULL_MOTION_TYPE)
        return false;
    auto state = manager.MovementFor(bot->GetGUID().GetCounter());
    if (!state)
        throw std::runtime_error("ADVAI movement requested without a loaded personality");
    G3D::Vector3 destination(x, y, z);
    // Snap a target slightly off the walkable floor onto it: the route end check requires 1 yd in 3D.
    if (float ground = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, z + 2.0f);
        ground > INVALID_HEIGHT && std::abs(ground - z) <= 1.5f)
        destination.z = ground;
    auto now = Now();
    std::unique_lock lock(state->Mutex);
    bool same = state->Map == map && Distance(state->Destination, destination) < (precise ? 0.25f : 0.8f);
    if (Distance(PositionOf(bot), destination) < 0.25f)
    {
        if (state->Active && state->Spline == bot->movespline->GetId())
            bot->StopMoving();
        state->Active = false;
        state->PauseUntil = 0;
        return true;
    }
    if (same && now < state->PauseUntil && !combat && !bot->IsInCombat() && !precise && !backwards)
        return true;
    if (same && state->Active && state->Spline == bot->movespline->GetId() && !bot->movespline->Finalized() &&
        (state->Backwards == backwards || (state->LookBehindActive && !backwards)) && state->Precise == precise &&
        state->Combat == combat && state->Facing == (facing ? facing->GetGUID() : ObjectGuid::Empty))
    {
        ++state->Reused;
        return true;
    }
    if (auto* blocked = FindBlocked(*state, map, destination); blocked && now < blocked->Until)
        return false;
    if (same && now < state->RetryAt)
        return false;
    std::vector<G3D::Vector3> threats;
    if (!combat && !precise && !bot->IsInCombat())
        for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
            if (auto* unit = ai->GetUnit(guid); unit && unit->IsAlive() && bot->CanSeeOrDetect(unit) &&
                                                bot->InSamePhase(unit) && bot->IsWithinDistInMap(unit, 60.0f) &&
                                                bot->IsWithinLOSInMap(unit))
            {
                threats.push_back(PositionOf(unit));
                if (threats.size() == 12)
                    break;
            }
    if (!same)
    {
        state->Recoveries = 0;
        state->RetryAt = 0;
        state->Replanning = false;
    }
    state->Destination = destination;
    state->Map = map;
    state->Precise = precise;
    state->Backwards = backwards;
    state->LookBehindPending = false;
    state->LookBehindActive = false;
    state->Replanning = false;
    state->Combat = combat;
    state->PauseUntil = 0;
    std::erase_if(state->Exclusions, [now](auto const& point) { return now >= point.Until; });
    Movement::PointsArray chosen;
    float best = std::numeric_limits<float>::max();
    Movement::PointsArray direct;
    if (NativePath(bot, destination, direct))
    {
        best = RouteScore(direct, *state, threats, now);
        chosen = std::move(direct);
    }
    if (!precise && !combat)
    {
        for (auto const& familiar : state->Familiar)
            if (familiar.Map == map && now < familiar.Used + 900 && familiar.Points.size() >= 2 &&
                Distance(familiar.Points.back(), destination) < 0.5f)
            {
                std::size_t join = familiar.Points.size();
                float nearest = 12.0f;
                auto forward = destination - PositionOf(bot);
                for (std::size_t i = 0; i < familiar.Points.size(); ++i)
                {
                    auto delta = familiar.Points[i] - PositionOf(bot);
                    if (delta.dot(forward) >= -0.3f && delta.length() < nearest)
                    {
                        nearest = delta.length();
                        join = i;
                    }
                }
                if (join == familiar.Points.size())
                    continue;
                Movement::PointsArray prefix;
                if (nearest < 0.25f)
                    prefix.push_back(PositionOf(bot));
                else if (!NativePath(bot, familiar.Points[join], prefix))
                    continue;
                {
                    bool clear = true;
                    for (std::size_t i = join + 1; i < familiar.Points.size(); ++i)
                    {
                        auto const& a = familiar.Points[i - 1];
                        auto const& b = familiar.Points[i];
                        if (!bot->GetMap()->isInLineOfSight(a.x, a.y, a.z + 1.0f, b.x, b.y, b.z + 1.0f,
                            bot->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
                        {
                            clear = false;
                            break;
                        }
                    }
                    if (!clear)
                        continue;
                    prefix.insert(prefix.end(), familiar.Points.begin() + join + 1, familiar.Points.end());
                    auto cost = RouteScore(prefix, *state, threats, now) * 0.93f;
                    if (cost < best)
                    {
                        best = cost;
                        chosen = std::move(prefix);
                    }
                }
            }
        // Nearby alternatives are considered for an observed threat or a failed
        // route, rather than making every trip wander for its own sake.
        if ((!threats.empty() || !state->Exclusions.empty()) && Distance(PositionOf(bot), destination) > 12.0f)
        {
            auto delta = destination - PositionOf(bot);
            float horizontal = std::hypot(delta.x, delta.y);
            if (horizontal > 1.0f)
                for (int side : {-1, 1})
                {
                    float forward = std::min(10.0f, horizontal * 0.3f);
                    auto anchor = PositionOf(bot) + G3D::Vector3(delta.x, delta.y, 0) * (forward / horizontal) +
                        G3D::Vector3(-delta.y, delta.x, 0) * (side * 4.0f / horizontal);
                    anchor.z = bot->GetMap()->GetHeight(bot->GetPhaseMask(), anchor.x, anchor.y, anchor.z + 2.0f);
                    Movement::PointsArray first;
                    if (anchor.z <= INVALID_HEIGHT || !NativePath(bot, anchor, first))
                        continue;
                    PathGenerator onward(bot);
                    // Calculate from the explicit anchor using the native API.
                    if (!onward.CalculatePath(anchor.x, anchor.y, anchor.z, x, y, z, false) ||
                        onward.GetPathType() != PATHFIND_NORMAL || onward.GetPath().size() < 2 ||
                        Distance(onward.GetActualEndPosition(), destination) > 1.0f)
                        continue;
                    first.insert(first.end(), onward.GetPath().begin() + 1, onward.GetPath().end());
                    if (first.size() > 512 || (best < std::numeric_limits<float>::max() && Length(first) >
                        Distance(PositionOf(bot), destination) * 2.0f + 20.0f))
                        continue;
                    float cost = RouteScore(first, *state, threats, now);
                    cost *= (side > 0) == (state->Persona.Seed % 2 == 0) ? 0.98f : 1.02f;
                    if (cost < best)
                    {
                        best = cost;
                        chosen = std::move(first);
                    }
                }
        }
    }
    if (chosen.empty())
    {
        state->Active = false;
        state->RetryAt = now + 15;
        ++state->FailedRoutes;
        auto* blocked = FindBlocked(*state, map, destination);
        if (!blocked)
        {
            if (state->Blocked.size() == MAX_BLOCKED_DESTINATIONS)
                state->Blocked.pop_front();
            state->Blocked.push_back({map, destination, 0, 0});
            blocked = &state->Blocked.back();
        }
        if (++blocked->Failures >= BLOCK_AFTER_FAILURES)
        {
            // Log once per blacklist event, not on every skipped retry.
            bool fresh = blocked->Until <= now;
            blocked->Until = now + BLOCK_SECONDS;
            blocked->Failures = 0;
            if (fresh)
                LOG_INFO("playerbots", "ADVAI bot {} blacklisted destination map {} ({:.1f}, {:.1f}, {:.1f}) "
                         "for {} s after {} failed routes", bot->GetGUID().GetCounter(), map, destination.x,
                         destination.y, destination.z, BLOCK_SECONDS, BLOCK_AFTER_FAILURES);
        }
        lock.unlock();
        auto goal = manager.PlannedGoal(bot->GetGUID().GetCounter());
        manager.Report(bot->GetGUID().GetCounter(), {goal.value_or(Goal::Travel), Outcome::Blocked,
            "ground route has no complete native traversable corridor; retry deferred"});
        return false;
    }
    if (!combat && !precise && chosen.size() <= 128)
    {
        std::erase_if(state->Familiar,
                      [&](auto const& route)
                      {
                          return route.Map != map || now >= route.Used + 900 ||
                                 (!route.Points.empty() && Distance(route.Points.back(), destination) < 0.5f);
                      });
        if (state->Familiar.size() == 4)
            state->Familiar.pop_front();
        state->Familiar.push_back({map, chosen, now});
    }
    if (auto* blocked = FindBlocked(*state, map, destination))
        blocked->Failures = 0;  // only consecutive failures count toward the blacklist
    DriftAlongRoute(bot, *state, chosen);
    RoundCorners(bot, *state, chosen);
    SwingIntoRoute(bot, *state, chosen);
    MisjudgeDestination(bot, *state, chosen);
    state->LastSafe = PositionOf(bot);
    Launch(bot, *state, std::move(chosen), facing);
    return true;
}

bool PlayerRouteActive(PlayerbotAI* ai)
{
    auto state = Manager::Instance().MovementFor(ai->GetBot()->GetGUID().GetCounter());
    if (!state)
        return false;
    std::lock_guard lock(state->Mutex);
    return state->Active && state->Map == ai->GetBot()->GetMapId() &&
        state->Spline == ai->GetBot()->movespline->GetId() && !ai->GetBot()->movespline->Finalized();
}

bool PlayerRouteProgressing(PlayerbotAI* ai)
{
    auto state = Manager::Instance().MovementFor(ai->GetBot()->GetGUID().GetCounter());
    if (!state)
        return false;
    std::lock_guard lock(state->Mutex);
    return state->Active && Now() < state->ProgressAt + 5 && state->Map == ai->GetBot()->GetMapId();
}

void StopForTyping(PlayerbotAI* ai, std::uint32_t milliseconds)
{
    auto* bot = ai->GetBot();
    if (!milliseconds || !bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() || bot->IsBeingTeleported() ||
        bot->GetTransport() || bot->GetVehicle())
        return;
    // Do not touch the per-bot movement state here: callers hold the bot's state mutex. A walk that is interrupted for
    // at most two seconds is simply re-planned by the movement manager afterwards.
    if (bot->isMoving())
        bot->StopMoving();
    ai->SetNextCheckDelay(milliseconds);  // no new actions (and so no new movement) until the reply is sent
}

bool StopForCast(PlayerbotAI* ai, Timestamp seconds)
{
    auto* bot = ai->GetBot();
    if (!bot->isMoving() || bot->IsInCombat() || bot->GetGroup() || !GroundEligible(ai))
        return false;
    if (auto state = Manager::Instance().MovementFor(bot->GetGUID().GetCounter()))
    {
        std::lock_guard lock(state->Mutex);
        if (state->Active)
            state->PauseUntil = Now() + std::min<Timestamp>(seconds, 4);
    }
    bot->StopMoving();
    return true;
}

void PausePlayerTravel(PlayerbotAI* ai, Timestamp seconds)
{
    if (!GroundEligible(ai) || ai->GetBot()->IsInCombat() || ai->GetBot()->GetGroup())
        return;
    auto& manager = Manager::Instance();
    auto goal = manager.PlannedGoal(ai->GetBot()->GetGUID().GetCounter());
    auto state = manager.MovementFor(ai->GetBot()->GetGUID().GetCounter());
    if (!state || !goal)
        return;
    std::lock_guard lock(state->Mutex);
    auto now = Now();
    if (!state->Active || state->Precise || state->Combat || now < state->LastPause + 120 ||
        state->Spline != ai->GetBot()->movespline->GetId())
        return;
    state->PauseUntil = now + std::min<Timestamp>(seconds, 3);
    state->LastPause = now;
    state->PausedGoal = *goal;
    ai->GetBot()->StopMoving();
    state->Spline = ai->GetBot()->movespline->GetId();
}

bool TravelLookahead(PlayerbotAI* ai, float distance, G3D::Vector3& point)
{
    auto* bot = ai->GetBot();
    auto const* move = bot->movespline;
    if (!move->Initialized() || move->Finalized() || move->isCyclic())
        return false;
    auto const& spline = move->_Spline();
    int32 const first = spline.first();
    int32 const last = spline.last();
    int32 const index = std::max(first + 1, first + move->currentPathIdx() + 1);
    auto const start = PositionOf(bot);
    for (int32 i = index; i <= last && i < index + 64; ++i)
    {
        auto const candidate = spline.getPoint(i);
        if (Distance(start, candidate) >= distance)
        {
            point = candidate;
            return true;
        }
    }
    return false;
}

bool BeginLookBehind(PlayerbotAI* ai, float& turn)
{
    auto* bot = ai->GetBot();
    auto& manager = Manager::Instance();
    auto goal = manager.PlannedGoal(bot->GetGUID().GetCounter());
    auto state = manager.MovementFor(bot->GetGUID().GetCounter());
    if (!goal || !state || !GroundEligible(ai) || bot->IsInCombat() || bot->GetGroup() || !bot->IsOutdoors() ||
        bot->IsMounted() || bot->IsWalking() || bot->GetMap()->IsDungeon())
        return false;
    std::lock_guard lock(state->Mutex);
    auto const now = Now();
    if (!state->Active || state->Precise || state->Combat || state->Backwards || state->Facing || state->PauseUntil ||
        state->LookBehindActive || now < state->LastPause + 120 || state->Spline != bot->movespline->GetId() ||
        bot->movespline->Finalized())
        return false;
    G3D::Vector3 segment;
    if (!TravelLookahead(ai, 7.0f, segment))
        return false;
    auto const start = PositionOf(bot);
    auto delta = segment - start;
    float const length = delta.length();
    if (length < 7.0f || std::abs(delta.z) > 0.5f)
        return false;
    auto const target = start + delta * (3.0f / length);
    if (!ClearSegment(bot, start, target))
        return false;
    float const wanted = std::atan2(delta.y, delta.x) + static_cast<float>(M_PI);
    turn = Position::NormalizeOrientation(wanted - bot->GetOrientation());
    if (turn > static_cast<float>(M_PI))
        turn -= 2.0f * static_cast<float>(M_PI);
    state->LookBehindPosition = target;
    state->LookBehindPending = true;
    state->PauseUntil = now + 3;
    state->LastPause = now;
    state->PausedGoal = *goal;
    bot->StopMoving();
    state->Spline = bot->movespline->GetId();
    return true;
}

void UpdatePlayerMovement(PlayerbotAI* ai)
{
    auto* bot = ai->GetBot();
    auto state = Manager::Instance().MovementFor(bot->GetGUID().GetCounter());
    auto goal = Manager::Instance().PlannedGoal(bot->GetGUID().GetCounter());
    if (!state)
        return;
    std::unique_lock lock(state->Mutex);
    if (!state->Active)
        return;
    if (!GroundEligible(ai) || bot->IsMovementPreventedByCasting() ||
        !Manager::Instance().Active(bot->GetGUID().GetCounter()) || state->Map != bot->GetMapId() ||
        state->Spline != bot->movespline->GetId())
    {
        if (state->Spline == bot->movespline->GetId())
            bot->StopMoving();
        state->Active = false;
        state->PauseUntil = 0;
        state->LookBehindPending = false;
        state->LookBehindActive = false;
        return;
    }
    if (state->PauseUntil)
    {
        if (!goal || *goal != state->PausedGoal || bot->IsInCombat())
        {
            state->Active = false;
            state->PauseUntil = 0;
            state->LookBehindPending = false;
            return;
        }
        if (Now() < state->PauseUntil)
            return;
        auto destination = state->Destination;
        state->PauseUntil = 0;
        if (state->LookBehindPending)
        {
            state->LookBehindPending = false;
            Movement::PointsArray route;
            auto const delta = state->LookBehindPosition - PositionOf(bot);
            float const wanted =
                Position::NormalizeOrientation(std::atan2(delta.y, delta.x) + static_cast<float>(M_PI));
            float const error = Position::NormalizeOrientation(bot->GetOrientation() - wanted);
            bool const facingBehind = error < 0.2f || error > 2.0f * static_cast<float>(M_PI) - 0.2f;
            if (facingBehind && NativePath(bot, state->LookBehindPosition, route) &&
                ClearSegment(bot, PositionOf(bot), state->LookBehindPosition))
            {
                state->Backwards = true;
                state->LookBehindActive = true;
                ++state->LookBehinds;
                state->Replanning = true;
                Launch(bot, *state, std::move(route), nullptr);
                return;
            }
        }
        state->Active = false;
        lock.unlock();
        MovePlayerGround(ai, bot->GetMapId(), destination.x, destination.y, destination.z, false, false, false);
        return;
    }
    if (bot->movespline->Finalized())
    {
        bool resume = state->Replanning;
        auto destination = state->Destination;
        if (state->LookBehindActive)
        {
            state->LookBehindActive = false;
            state->Backwards = false;
        }
        bool precise = state->Precise, backwards = state->Backwards, combat = state->Combat;
        auto facing = state->Facing;
        state->Replanning = false;
        state->Active = false;
        lock.unlock();
        if (resume)
            MovePlayerGround(ai, bot->GetMapId(), destination.x, destination.y, destination.z, precise, backwards,
                             combat, facing ? ai->GetUnit(facing) : nullptr);
        return;
    }
    if (state->Facing)
        if (auto* target = ai->GetUnit(state->Facing); target && target->IsAlive() && bot->CanSeeOrDetect(target) &&
                                                       bot->IsWithinLOSInMap(target) &&
                                                       bot->IsWithinDistInMap(target, 45.0f))
            bot->SetFacingToObject(target);
    auto now = Now();
    if (Distance(PositionOf(bot), state->LastPosition) >= 0.6f)
    {
        ++ProgressSamples;
        state->LastSafe = state->LastPosition;
        state->LastPosition = PositionOf(bot);
        state->ProgressAt = now;
    }
    float speed = bot->GetSpeed(state->Backwards ? MOVE_RUN_BACK : bot->IsWalking() ? MOVE_WALK : MOVE_RUN);
    bool stalled = now >= state->ProgressAt + 8;
    if (!stalled && std::abs(speed - state->Speed) < 0.05f)
        return;
    auto destination = state->Destination;
    bool precise = state->Precise, backwards = state->Backwards, combat = state->Combat;
    if (state->LookBehindActive)
    {
        backwards = false;
        state->Backwards = false;
        state->LookBehindActive = false;
    }
    auto facing = state->Facing;
    state->Active = false;
    if (stalled)
    {
        if (state->Exclusions.size() == 8)
            state->Exclusions.pop_front();
        state->Exclusions.push_back({PositionOf(bot), now + 90});
        ++state->Recoveries;
        if (state->Recoveries > 2)
        {
            state->RetryAt = now + 60;
            ++state->FailedRoutes;
            bot->StopMoving();
            lock.unlock();
            auto& manager = Manager::Instance();
            auto goal = manager.PlannedGoal(bot->GetGUID().GetCounter());
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal.value_or(Goal::Travel), Outcome::Blocked,
                            "movement exhausted two bounded local recovery attempts; route blocked"});
            return;
        }
        ++state->RecoveryMoves;
        // A previous verified point supplies a short retreat before replanning.
        if (Distance(PositionOf(bot), state->LastSafe) > 1.0f)
        {
            Movement::PointsArray retreat;
            if (NativePath(bot, state->LastSafe, retreat))
            {
                state->Replanning = true;
                Launch(bot, *state, std::move(retreat), nullptr);
                return;
            }
        }
    }
    state->RetryAt = 0;
    lock.unlock();
    MovePlayerGround(ai, bot->GetMapId(), destination.x, destination.y, destination.z,
                     precise, backwards, combat, facing ? ai->GetUnit(facing) : nullptr);
}

std::string DescribeMovement(std::shared_ptr<MovementState> const& state)
{
    if (!state)
        return " movement=unstarted";
    std::lock_guard lock(state->Mutex);
    std::ostringstream out;
    out << " movement=" << (state->Active ? "route" : "idle") << " routes=" << state->Routes
        << " reusedRoutes=" << state->Reused << " roundedCorners=" << state->RoundedCorners
        << " routeDrifts=" << state->RouteDrifts << " lookBehinds=" << state->LookBehinds
        << " routeFailures=" << state->FailedRoutes << " recoveryMoves=" << state->RecoveryMoves
        << " turnStyle=" << state->Persona.Seed % 3 << " swungTurns=" << state->SwungTurns
        << " overshoots=" << state->Overshoots << " undershoots=" << state->Undershoots;
    return out.str();
}

std::string MovementTelemetry()
{
    return " movement_paths=" + std::to_string(PathsStarted.load()) +
           " movement_progress=" + std::to_string(ProgressSamples.load());
}

std::string DescribeMovementStats(bool reset)
{
    std::ostringstream out;
    out << "routes judged=" << RoutesLaunched.load() << " exact=" << RoutesExact.load()
        << " habits(exact,over,over,under,under,mixed)=";
    for (auto& h : HabitTrips)
        out << h.load() << ",";
    out << "\novershoot yards:" << OvershootYards.Describe(0.25f, "y")
        << "\nundershoot yards:" << UndershootYards.Describe(0.25f, "y")
        << "\ncorner cut radius:" << CornerRadius.Describe(0.25f, "y")
        << "\nstart-swing turn angle:" << SwingTurnRadians.Describe(0.3f, "rad")
        << "\nhurry mood:" << HurryDistribution.Describe(1.0f / Histogram::Bins, "");
    if (reset)
    {
        OvershootYards.Reset();
        UndershootYards.Reset();
        CornerRadius.Reset();
        SwingTurnRadians.Reset();
        HurryDistribution.Reset();
        RoutesLaunched = 0;
        RoutesExact = 0;
        for (auto& h : HabitTrips)
            h = 0;
    }
    return out.str();
}

bool StartPvpFootwork(PlayerbotAI* ai, G3D::Vector3 destination, Unit* target)
{
    auto* bot = ai->GetBot();
    Movement::PointsArray route;
    if (!NativePath(bot, destination, route) || Length(route) > 7.0f ||
        !bot->GetMap()->isInLineOfSight(destination.x, destination.y, destination.z + 1.0f,
            target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + 1.0f,
            bot->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
        return false;
    auto state = Manager::Instance().MovementFor(bot->GetGUID().GetCounter());
    if (!state)
        throw std::runtime_error("ADVAI PvP footwork requires a loaded movement profile");
    std::lock_guard lock(state->Mutex);
    state->Map = bot->GetMapId();
    state->Destination = destination;
    state->Combat = true;
    state->Precise = true;
    state->Backwards = false;
    state->Replanning = false;
    state->PauseUntil = 0;
    state->RetryAt = 0;
    state->Recoveries = 0;
    state->LastSafe = PositionOf(bot);
    Launch(bot, *state, std::move(route), target);
    return true;
}
}

bool AdvaiPvpFootworkAction::isUseful()
{
    auto& manager = Advai::Manager::Instance();
    if (!manager.Applies(bot) || !manager.Active(bot->GetGUID().GetCounter()) || !botAI->AllowActivity() ||
        !bot->InBattleground() || !bot->IsAlive() || !bot->IsInCombat() || botAI->HasGameClientMaster() ||
        bot->GetVehicle() || bot->GetTransport() || bot->IsMounted() || bot->IsInWater() || bot->IsFlying() ||
        bot->IsNonMeleeSpellCast(false) || bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_NOT_MOVE) ||
        bot->GetHealthPct() < 40.0f || PlayerHasFlag::IsCapturingFlag(bot) ||
        bot->HasAura(23333) || bot->HasAura(23335) || bot->HasAura(34976) ||
        static_cast<Advai::Timestamp>(time(nullptr)) < _nextFootwork)
        return false;
    return Advai::VisiblePvpEnemy(bot, bot->GetVictim()) &&
           !IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT);
}

bool AdvaiPvpFootworkAction::Execute(Event /*event*/)
{
    auto* target = bot->GetVictim();
    if (!Advai::VisiblePvpEnemy(bot, target))
        return false;
    auto persona = Advai::Manager::Instance().Persona(bot->GetGUID().GetCounter());
    _nextFootwork = static_cast<Advai::Timestamp>(time(nullptr)) + 3 + persona.Seed % 5;
    float dx = bot->GetPositionX() - target->GetPositionX();
    float dy = bot->GetPositionY() - target->GetPositionY();
    float distance = std::hypot(dx, dy);
    if (distance < 0.5f || distance > 32.0f)
        return false;
    bool pressured = target->GetVictim() == bot && distance < 9.0f;
    if ((PlayerbotAI::IsHeal(bot) || persona.Seed % 4 == 0) && !pressured)
        return false;
    float x, y;
    if (PlayerbotAI::IsRanged(bot))
    {
        float side = persona.Seed % 2 ? 1.0f : -1.0f;
        float retreat = pressured ? 3.0f : 0.0f;
        float strafe = pressured ? 0.8f : 1.6f;
        if (!pressured && (distance < 12.0f || distance > 28.0f))
            return false;
        x = bot->GetPositionX() + dx * retreat / distance - dy * side * strafe / distance;
        y = bot->GetPositionY() + dy * retreat / distance + dx * side * strafe / distance;
    }
    else
    {
        if (distance > 5.5f)
            return false;
        float angle = std::atan2(dy, dx);
        float wanted = target->GetOrientation() + static_cast<float>(M_PI);
        float difference = std::remainder(wanted - angle, 2.0f * static_cast<float>(M_PI));
        if (std::abs(difference) < 0.45f)
            return false;
        float turn = std::min(std::abs(difference), 0.4f + persona.Skill * 0.002f);
        angle += difference < 0 ? -turn : turn;
        float radius = std::clamp(distance, 2.0f, 4.5f);
        x = target->GetPositionX() + std::cos(angle) * radius;
        y = target->GetPositionY() + std::sin(angle) * radius;
    }
    float z = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, bot->GetPositionZ() + 1.0f, true, 3.0f);
    if (z <= INVALID_HEIGHT || std::abs(z - bot->GetPositionZ()) > 0.75f ||
        bot->GetMap()->IsInWater(bot->GetPhaseMask(), x, y, z, bot->GetCollisionHeight()))
        return false;
    bool moved = Advai::StartPvpFootwork(botAI, {x, y, z}, target);
    if (moved)
    {
        auto delay = static_cast<std::uint32_t>(1000 * bot->GetExactDist(x, y, z) / std::max(0.1f, bot->GetSpeed(MOVE_RUN)));
        context->GetValue<LastMovement&>("last movement")->Get().Set(bot->GetMapId(), x, y, z,
            bot->GetOrientation(), delay, MovementPriority::MOVEMENT_COMBAT);
    }
    return moved;
}
