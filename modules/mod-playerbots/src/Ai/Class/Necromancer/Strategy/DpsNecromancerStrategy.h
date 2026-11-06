/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_DPSNECROMANCERSTRATEGY_H
#define PLAYERBOTS_DPSNECROMANCERSTRATEGY_H

#include "GenericNecromancerStrategy.h"

class PlayerbotAI;

class DpsNecromancerStrategy : public GenericNecromancerStrategy
{
public:
    DpsNecromancerStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "dps"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
};

#endif
