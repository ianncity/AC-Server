/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "HealerCultistStrategy.h"
#include "Playerbots.h"

HealerCultistStrategy::HealerCultistStrategy(PlayerbotAI* botAI) : GenericCultistStrategy(botAI)
{
}

std::vector<NextAction> HealerCultistStrategy::getDefaultActions()
{
    return {
        NextAction("horrorbolt", 10.0f),
        NextAction("eldritch blast", 8.0f)
    };
}

void HealerCultistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericCultistStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("party member critical health", {NextAction("abyssal reconstruction on party", 35.0f)}));
    triggers.push_back(new TriggerNode("party member almost full health", {NextAction("eldritch mending on party", 25.0f)}));
    triggers.push_back(new TriggerNode("critical health", {NextAction("abyssal reconstruction", 40.0f)}));
    triggers.push_back(new TriggerNode("low health", {NextAction("eldritch mending", 30.0f)}));
    triggers.push_back(new TriggerNode("void shield", {NextAction("void shield", 28.0f)}));
}
