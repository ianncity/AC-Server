/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_HEALERCULTISTSTRATEGY_H
#define PLAYERBOTS_HEALERCULTISTSTRATEGY_H

#include "GenericCultistStrategy.h"

class PlayerbotAI;

class HealerCultistStrategy : public GenericCultistStrategy
{
public:
    HealerCultistStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "heal"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override { return CombatStrategy::GetType() | STRATEGY_TYPE_HEAL | STRATEGY_TYPE_RANGED; }
};

#endif
