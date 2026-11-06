/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "DpsNecromancerStrategy.h"
#include "Playerbots.h"

DpsNecromancerStrategy::DpsNecromancerStrategy(PlayerbotAI* botAI) : GenericNecromancerStrategy(botAI)
{
}

std::vector<NextAction> DpsNecromancerStrategy::getDefaultActions()
{
    return {
        NextAction("lichfrost", 15.0f),
        NextAction("plague of undeath", 14.0f),
        NextAction("harvest plague", 13.0f),
        NextAction("heartchill", 11.0f),
        NextAction("chill of the tomb", 9.0f),
        NextAction("shoot", 1.0f)
    };
}

void DpsNecromancerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericNecromancerStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("plague of undeath", {NextAction("plague of undeath", 20.0f)}));
    triggers.push_back(new TriggerNode("harvest plague", {NextAction("harvest plague", 18.0f)}));
    triggers.push_back(new TriggerNode("undead: assault", {NextAction("undead: assault", 16.0f)}));
    triggers.push_back(new TriggerNode("foul mandate", {NextAction("foul mandate", 15.0f)}));
}
