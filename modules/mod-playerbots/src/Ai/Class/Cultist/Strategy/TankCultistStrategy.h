/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_TANKCULTISTSTRATEGY_H
#define PLAYERBOTS_TANKCULTISTSTRATEGY_H

#include "GenericCultistStrategy.h"

class PlayerbotAI;

class TankCultistStrategy : public GenericCultistStrategy
{
public:
    TankCultistStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "tank"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override { return CombatStrategy::GetType() | STRATEGY_TYPE_TANK | STRATEGY_TYPE_MELEE; }
};

#endif
