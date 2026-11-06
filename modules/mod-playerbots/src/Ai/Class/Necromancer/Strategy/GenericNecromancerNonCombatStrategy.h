/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_GENERICNECROMANCERNONCOMBATSTRATEGY_H
#define PLAYERBOTS_GENERICNECROMANCERNONCOMBATSTRATEGY_H

#include "NonCombatStrategy.h"

class PlayerbotAI;

class GenericNecromancerNonCombatStrategy : public NonCombatStrategy
{
public:
    GenericNecromancerNonCombatStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "nc"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
