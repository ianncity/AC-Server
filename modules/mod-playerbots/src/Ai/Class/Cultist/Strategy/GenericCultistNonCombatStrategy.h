/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_GENERICCULTISTNONCOMBATSTRATEGY_H
#define PLAYERBOTS_GENERICCULTISTNONCOMBATSTRATEGY_H

#include "NonCombatStrategy.h"

class PlayerbotAI;

class GenericCultistNonCombatStrategy : public NonCombatStrategy
{
public:
    GenericCultistNonCombatStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "nc"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
