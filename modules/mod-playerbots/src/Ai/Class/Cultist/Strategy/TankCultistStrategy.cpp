/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "TankCultistStrategy.h"
#include "Playerbots.h"

TankCultistStrategy::TankCultistStrategy(PlayerbotAI* botAI) : GenericCultistStrategy(botAI)
{
}

std::vector<NextAction> TankCultistStrategy::getDefaultActions()
{
    return {
        NextAction("entropic slam", 18.0f),
        NextAction("blade of the empire", 16.0f),
        NextAction("gaze of c'thun", 14.0f),
        NextAction("melee", 1.0f)
    };
}

void TankCultistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericCultistStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("void shield", {NextAction("void shield", 25.0f)}));
    triggers.push_back(new TriggerNode("lose aggro", {NextAction("entropic slam", 28.0f)}));
    triggers.push_back(new TriggerNode("critical health", {NextAction("abyssal reconstruction", 30.0f)}));
}
