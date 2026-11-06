/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_GENERICCULTISTSTRATEGY_H
#define PLAYERBOTS_GENERICCULTISTSTRATEGY_H

#include "CombatStrategy.h"

class PlayerbotAI;

class GenericCultistStrategy : public CombatStrategy
{
public:
    GenericCultistStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "cultist"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override { return CombatStrategy::GetType() | STRATEGY_TYPE_DPS; }
};

class CultistAoeStrategy : public CombatStrategy
{
public:
    CultistAoeStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI) {}

    std::string const getName() override { return "aoe"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

class CultistBuffStrategy : public Strategy
{
public:
    CultistBuffStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "buff"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
