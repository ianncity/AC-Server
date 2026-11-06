/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "SummonerNecromancerStrategy.h"
#include "Playerbots.h"

SummonerNecromancerStrategy::SummonerNecromancerStrategy(PlayerbotAI* botAI) : GenericNecromancerStrategy(botAI)
{
}

std::vector<NextAction> SummonerNecromancerStrategy::getDefaultActions()
{
    return {
        NextAction("lichfrost", 14.0f),
        NextAction("plague of undeath", 12.0f),
        NextAction("heartchill", 10.0f)
    };
}

void SummonerNecromancerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericNecromancerStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("no pet", {NextAction("raise: greater skeletal warrior", 30.0f)}));
    triggers.push_back(new TriggerNode("foul mandate", {NextAction("foul mandate", 20.0f)}));
    triggers.push_back(new TriggerNode("undead: assault", {NextAction("undead: assault", 18.0f)}));
}
