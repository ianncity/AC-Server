/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "TankNecromancerStrategy.h"
#include "Playerbots.h"

TankNecromancerStrategy::TankNecromancerStrategy(PlayerbotAI* botAI) : GenericNecromancerStrategy(botAI)
{
}

std::vector<NextAction> TankNecromancerStrategy::getDefaultActions()
{
    return {
        NextAction("heartchill", 16.0f),
        NextAction("lichfrost", 14.0f),
        NextAction("chill of the tomb", 12.0f),
        NextAction("melee", 1.0f)
    };
}

void TankNecromancerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericNecromancerStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("undead: protect", {NextAction("undead: protect", 25.0f)}));
    triggers.push_back(new TriggerNode("lose aggro", {NextAction("heartchill", 28.0f)}));
    triggers.push_back(new TriggerNode("critical health", {NextAction("transfer life", 30.0f)}));
}
