/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_SUMMONERNECROMANCERSTRATEGY_H
#define PLAYERBOTS_SUMMONERNECROMANCERSTRATEGY_H

#include "GenericNecromancerStrategy.h"

class PlayerbotAI;

class SummonerNecromancerStrategy : public GenericNecromancerStrategy
{
public:
    SummonerNecromancerStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "summon"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
};

#endif
