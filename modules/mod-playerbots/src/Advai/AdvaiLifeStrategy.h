/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_LIFE_STRATEGY_H
#define PLAYERBOTS_ADVAI_LIFE_STRATEGY_H

#include <unordered_map>

#include "AdvaiModel.h"
#include "AdvaiProfession.h"
#include "AdvaiTravel.h"
#include "NewRpgBaseAction.h"
#include "Strategy.h"

class AdvaiLifeStrategy : public Strategy
{
public:
    explicit AdvaiLifeStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "advai life"; }
    std::vector<NextAction> getDefaultActions() override;
};

class AdvaiLifeAction : public NewRpgBaseAction
{
public:
    explicit AdvaiLifeAction(PlayerbotAI* ai) : NewRpgBaseAction(ai, "advai life") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    struct BlockedService
    {
        Advai::ServiceLocation Location;
        Advai::Timestamp Until = 0;
    };
    std::optional<Advai::ServiceLocation> _destination;
    std::optional<Advai::RecipePlan> _recipe;
    std::vector<Advai::RecipePlan> _craftParents;
    std::uint32_t _ingredient = 0;
    std::uint32_t _gatherIngredient = 0;
    Advai::Timestamp _gatherRetryAfter = 0;
    bool _craftPending = false;
    bool _restockBank = false;
    std::optional<Advai::ServiceLocation> _craftStation;
    std::uint32_t _craftStationSpell = 0;
    std::deque<BlockedService> _blockedServices;
    Advai::Goal _serviceGoal = Advai::Goal::None;
    Advai::Timestamp _nextAttempt = 0;
    Advai::Journey _journey;
};

class AdvaiLeisureAction : public NewRpgBaseAction
{
public:
    explicit AdvaiLeisureAction(PlayerbotAI* ai) : NewRpgBaseAction(ai, "advai leisure") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    std::optional<Advai::ServiceLocation> _inn;
    Advai::Goal _goal = Advai::Goal::None;
    Advai::Timestamp _nextCheck = 0;
    Advai::Timestamp _nextInnVisit = 0;
    Advai::Timestamp _pauseUntil = 0;
    Advai::Timestamp _nextGesture = 0;
    std::unordered_map<ObjectGuid::LowType, Advai::Timestamp> _gestured;
};

class AdvaiArenaPreparationAction : public NewRpgBaseAction
{
public:
    explicit AdvaiArenaPreparationAction(PlayerbotAI* ai) : NewRpgBaseAction(ai, "advai arena preparation") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    Advai::Timestamp _nextCheck = 0;
    std::optional<Advai::ServiceLocation> _registrar;
    Advai::Journey _journey;
};

class AdvaiPvpFootworkAction : public NewRpgBaseAction
{
public:
    explicit AdvaiPvpFootworkAction(PlayerbotAI* ai) : NewRpgBaseAction(ai, "advai pvp footwork") {}
    bool isUseful() override;
    bool Execute(Event event) override;
private:
    Advai::Timestamp _nextFootwork = 0;
};

class AdvaiRaidAction : public NewRpgBaseAction
{
public:
    explicit AdvaiRaidAction(PlayerbotAI* ai) : NewRpgBaseAction(ai, "advai raid") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    Advai::Timestamp _nextCheck = 0;
    Advai::Timestamp _nextRoster = 0;
    std::optional<Advai::ServiceLocation> _entrance;
    std::uint32_t _dungeon = 0;
    Advai::Journey _journey;
};

#endif
