/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_GENERICNECROMANCERSTRATEGY_H
#define PLAYERBOTS_GENERICNECROMANCERSTRATEGY_H

#include "CombatStrategy.h"

class PlayerbotAI;

class GenericNecromancerStrategy : public CombatStrategy
{
public:
    GenericNecromancerStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "necromancer"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override { return CombatStrategy::GetType() | STRATEGY_TYPE_DPS | STRATEGY_TYPE_RANGED; }
};

class NecromancerAoeStrategy : public CombatStrategy
{
public:
    NecromancerAoeStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI) {}

    std::string const getName() override { return "aoe"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

class NecromancerBuffStrategy : public Strategy
{
public:
    NecromancerBuffStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "buff"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
