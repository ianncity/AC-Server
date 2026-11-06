/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#include <cmath>
#include <stdexcept>

#include "AdvaiBehavior.h"
#include "AdvaiMgr.h"
#include "AdvaiMovement.h"
#include "AiObjectContext.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"

namespace Advai
{
namespace
{
void TurnGradually(PlayerbotAI* ai, float turn, unsigned steps, uint32 interval)
{
    auto guid = ai->GetBot()->GetGUID();
    float start = ai->GetBot()->GetOrientation();
    for (unsigned step = 1; step <= steps; ++step)
    {
        float const previous = static_cast<float>(step - 1) / steps;
        float const progress = static_cast<float>(step) / steps;
        float const previousEase = TurnEase(previous);
        float const ease = TurnEase(progress);
        float expected = Position::NormalizeOrientation(start + turn * previousEase);
        float angle = Position::NormalizeOrientation(start + turn * ease);
        ai->AddTimedEvent(
            [guid, expected, angle]
            {
                auto* self = ObjectAccessor::FindPlayer(guid);
                if (!self || !self->IsInWorld() || !self->IsAlive() || self->isMoving() || self->IsInCombat() ||
                    self->IsBeingTeleported() || self->IsNonMeleeSpellCast(false) || self->GetTransport() ||
                    self->GetVehicle() || self->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_NOT_MOVE))
                    return;
                auto* currentAI = GET_PLAYERBOT_AI(self);
                if (!currentAI || currentAI->HasGameClientMaster())
                    return;
                float difference = Position::NormalizeOrientation(self->GetOrientation() - expected);
                if (difference > 0.15f && difference < 2.0f * static_cast<float>(M_PI) - 0.15f)
                    return;
                auto const oldSpline = self->movespline->GetId();
                self->SetFacingTo(angle);
                if (auto movement = Manager::Instance().MovementFor(guid.GetCounter()))
                {
                    std::lock_guard lock(movement->Mutex);
                    if (movement->LookBehindPending && movement->Spline == oldSpline)
                        movement->Spline = self->movespline->GetId();
                }
            },
            step * interval);
    }
}
bool PrepareTravelHop(Player* bot, G3D::Vector3 const& segment, BotPersona const& persona, float& x, float& y, float& z)
{
    float dx = segment.x - bot->GetPositionX(), dy = segment.y - bot->GetPositionY();
    float length = std::hypot(dx, dy);
    if (length < 7.0f || std::abs(segment.z - bot->GetPositionZ()) > 0.75f)
        return false;
    float distance = 2.6f + (persona.Seed % 9) * 0.1f + static_cast<float>(urand(0, 14)) * 0.1f;
    x = bot->GetPositionX() + dx * distance / length;
    y = bot->GetPositionY() + dy * distance / length;
    z = bot->GetPositionZ();
    for (unsigned sample = 1; sample <= 4; ++sample)
    {
        float t = static_cast<float>(sample) / 4.0f;
        float sx = bot->GetPositionX() + (x - bot->GetPositionX()) * t;
        float sy = bot->GetPositionY() + (y - bot->GetPositionY()) * t;
        float ground = bot->GetMap()->GetHeight(bot->GetPhaseMask(), sx, sy, z + 1.0f, true, 3.0f);
        if (ground <= INVALID_HEIGHT || std::abs(ground - bot->GetPositionZ()) > 0.5f ||
            bot->GetMap()->IsInWater(bot->GetPhaseMask(), sx, sy, ground, bot->GetCollisionHeight()))
            return false;
        z = ground;
    }
    if (!bot->IsWithinLOS(x, y, z + 1.5f))
        return false;
    PathGenerator path(bot);
    if (!path.CalculatePath(x, y, z, false) || path.GetPathType() != PATHFIND_NORMAL || path.GetPath().size() < 2 ||
        (path.GetActualEndPosition() - G3D::Vector3(x, y, z)).length() > 0.5f)
        return false;
    for (auto const& point : path.GetPath())
    {
        float lateral = std::abs((point.x - bot->GetPositionX()) * dy - (point.y - bot->GetPositionY()) * dx) / length;
        if (lateral > 0.4f || std::abs(point.z - bot->GetPositionZ()) > 0.5f)
            return false;
    }
    return true;
}
}  // namespace

void Manager::AttendToSpeaker(PlayerbotAI* ai, Player* speaker, std::shared_ptr<BotState> const& state, Timestamp now)
{
    auto* bot = ai->GetBot();
    if (!speaker || speaker == bot || !bot->IsAlive() || bot->IsInCombat() || bot->IsNonMeleeSpellCast(false) ||
        ai->HasGameClientMaster() || bot->GetGroup() || bot->GetTradeData() || bot->GetLootGUID() ||
        bot->IsInFlight() || bot->IsMounted() || bot->GetTransport() || bot->GetVehicle() ||
        !bot->IsWithinDistInMap(speaker, 12.0f) || !bot->CanSeeOrDetect(speaker) || !bot->IsWithinLOSInMap(speaker))
        return;
    {
        std::lock_guard lock(state->Mutex);
        auto contact = state->Relationships.find(speaker->GetGUID().GetCounter());
        if (now < state->NextAttention || (contact != state->Relationships.end() && contact->second.Affinity < -20))
            return;
        state->NextAttention = now + 20 + (100 - state->Persona.Sociability) / 5;
        state->NextGlance = now + 10;
    }
    PausePlayerTravel(ai, 2);
    if (bot->isMoving())
        return;
    float turn = Position::NormalizeOrientation(bot->GetAngle(speaker) - bot->GetOrientation());
    if (turn > static_cast<float>(M_PI))
        turn -= 2.0f * static_cast<float>(M_PI);
    TurnGradually(ai, turn, 4, urand(120, 200));
}

void Manager::TravelMannerisms(PlayerbotAI* ai, std::shared_ptr<BotState> const& state, Timestamp now)
{
    auto* bot = ai->GetBot();
    auto* motion = bot->GetMotionMaster();
    bool managedRoute = PlayerRouteActive(ai);
    bool available = Active(bot->GetGUID().GetCounter()) && bot->IsAlive() && !bot->IsInCombat() &&
        !bot->GetGroup() && !bot->GetGroupInvite() && !ai->HasGameClientMaster() && !bot->GetTradeData() &&
        !bot->IsInFlight() && !bot->IsBeingTeleported() && !bot->GetTransport() && !bot->GetVehicle() &&
        !bot->IsNonMeleeSpellCast(false) && !bot->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_NOT_MOVE) &&
        !bot->IsMounted() && !bot->IsFlying() && !bot->IsInWater() && !bot->GetLootGUID() &&
        !bot->GetMap()->IsDungeon() && !bot->InBattleground() && bot->GetHealthPct() >= 85.0f;
    // Idle glances: a person standing about does not stay rigidly facing one way. Sweep the facing toward a random
    // side in two or three small steps (a mouse drag, not a snap), at irregular intervals that vary per character.
    if (available && !bot->isMoving() && !managedRoute && bot->IsOutdoors() && bot->movespline->Finalized())
    {
        bool glance = false;
        {
            std::lock_guard lock(state->Mutex);
            if (!state->NextGlance)
                state->NextGlance = now + urand(3, 25);
            else if (now >= state->NextGlance)
            {
                state->NextGlance = now + 5 + urand(0, 20) + urand(0, 35) * (state->Persona.Patience > 50 ? 2 : 1);
                glance = urand(0, 99) < 70 && !state->Snapshot.HasCommitment();
            }
        }
        if (glance)
        {
            float const total =
                (urand(0, 1) ? 1.0f : -1.0f) * (0.35f + static_cast<float>(urand(0, 100)) / 100.0f * 1.25f);
            unsigned const steps = urand(2, 3);
            TurnGradually(ai, total, steps, urand(180, 420));
            return;
        }
    }
    std::optional<Goal> travelGoal;
    std::optional<ServiceLocation> resume;
    bool repeatHop = false;
    BotPersona persona;
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.State != Outcome::Succeeded && state->LifeGoal.State != Outcome::Cancelled &&
            (state->LifeGoal.State != Outcome::Blocked || now >= state->LifeGoal.RetryAfter))
            travelGoal = state->LifeGoal.Kind;
        if (state->ResumeAfterHop)
        {
            if (!available || now > state->HopExpires || state->ResumeAfterHop->Map != bot->GetMapId() ||
                (state->HopGoal != Goal::None && (!travelGoal || *travelGoal != state->HopGoal)))
            {
                state->ResumeAfterHop.reset();
                state->HopsRemaining = 0;
            }
            else if (motion->GetCurrentMovementGeneratorType() == EFFECT_MOTION_TYPE)
                return;
            else
            {
                // A new route or reaction wins over the old travel destination.
                if (bot->movespline->Finalized())
                {
                    resume = state->ResumeAfterHop;
                    repeatHop = state->HopsRemaining > 0 && !state->Snapshot.HasCommitment() && !state->Offer &&
                                state->Requests.empty();
                    persona = state->Persona;
                }
                state->ResumeAfterHop.reset();
                state->HopsRemaining = 0;
            }
        }
        if (!resume)
        {
            if (!state->NextTravelGesture)
            {
                state->NextTravelGesture = now + 45 + state->Persona.Seed % 180;
                return;
            }
            if (now < state->NextTravelGesture)
                return;
            persona = state->Persona;
            state->NextTravelGesture = now + 90 + (100 - persona.RiskTolerance) * 2 + urand(45, 180);
            // Some characters simply don't have the jumping habit.
            if (!state->Life.Initialized || state->Snapshot.HasCommitment() || state->Offer ||
                !state->Requests.empty() || persona.Seed % 5 == 0 || urand(0, 99) >= 25 + persona.RiskTolerance / 3)
                return;
        }
    }
    if (resume)
    {
        if (repeatHop)
            for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
                if (auto* enemy = ai->GetUnit(guid); enemy && enemy->IsAlive() && bot->IsWithinDistInMap(enemy, 25.0f))
                {
                    repeatHop = false;
                    break;
                }
        if (repeatHop && bot->IsOutdoors() && !bot->IsWalking() && !bot->Unit::IsFalling())
        {
            float x, y, z;
            G3D::Vector3 const destination(resume->X, resume->Y, resume->Z);
            if (PrepareTravelHop(bot, destination, persona, x, y, z))
            {
                std::lock_guard lock(state->Mutex);
                state->ResumeAfterHop = resume;
                state->HopExpires = now + 4;
                ++state->TravelHops;
                motion->MoveJump(x, y, z, bot->GetSpeed(MOVE_RUN), 5.0f);
                return;
            }
        }
        auto moved = MovePlayerGround(ai, resume->Map, resume->X, resume->Y, resume->Z, false, false, false);
        if (!moved)
            throw std::runtime_error("ADVAI travel hop lost its managed ground movement eligibility");
        return;
    }
    if (!available || !bot->IsOutdoors() || bot->IsWalking() || !bot->isMoving() || bot->Unit::IsFalling() ||
        bot->movespline->Finalized() ||
        (motion->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE && !managedRoute) ||
        motion->GetMotionSlotType(MOTION_SLOT_CONTROLLED) != NULL_MOTION_TYPE)
        return;
    for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
        if (auto* enemy = ai->GetUnit(guid); enemy && enemy->IsAlive() && bot->IsWithinDistInMap(enemy, 25.0f))
            return;
    if (managedRoute && urand(0, 4) == 0)
    {
        float turn;
        if (BeginLookBehind(ai, turn))
        {
            TurnGradually(ai, turn, 12, 150);
            return;
        }
    }
    auto destination = bot->movespline->FinalDestination();
    G3D::Vector3 segment;
    if (!TravelLookahead(ai, 7.0f, segment))
        return;
    float x, y, z;
    if (!PrepareTravelHop(bot, segment, persona, x, y, z))
        return;
    {
        std::lock_guard lock(state->Mutex);
        state->ResumeAfterHop = ServiceLocation{0, bot->GetMapId(), 0, 0, destination.x, destination.y, destination.z};
        state->HopExpires = now + 4;
        state->HopGoal = travelGoal.value_or(Goal::None);
        state->HopsRemaining = urand(0, 4) == 0 ? 1 : 0;
        ++state->TravelHops;
    }
    // Native physical jump, at normal running speed. Resume the original ground
    // route after landing rather than treating the hop as arrival at the goal.
    motion->MoveJump(x, y, z, bot->GetSpeed(MOVE_RUN), 5.0f);
}
}  // namespace Advai
