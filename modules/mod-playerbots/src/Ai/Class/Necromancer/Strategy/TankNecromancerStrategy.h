/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_TANKNECROMANCERSTRATEGY_H
#define PLAYERBOTS_TANKNECROMANCERSTRATEGY_H

#include "GenericNecromancerStrategy.h"

class PlayerbotAI;

class TankNecromancerStrategy : public GenericNecromancerStrategy
{
public:
    TankNecromancerStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "tank"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override { return CombatStrategy::GetType() | STRATEGY_TYPE_TANK; }
};

#endif
