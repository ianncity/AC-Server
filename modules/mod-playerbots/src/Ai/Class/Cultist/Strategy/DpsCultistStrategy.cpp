/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "DpsCultistStrategy.h"
#include "Playerbots.h"

DpsCultistStrategy::DpsCultistStrategy(PlayerbotAI* botAI) : GenericCultistStrategy(botAI)
{
}

std::vector<NextAction> DpsCultistStrategy::getDefaultActions()
{
    return {
        NextAction("blade of the empire", 15.0f),
        NextAction("wrath of the black empire", 14.0f),
        NextAction("horrorbolt", 12.0f),
        NextAction("eldritch blast", 10.0f),
        NextAction("melee", 1.0f)
    };
}

void DpsCultistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericCultistStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("wrath of the black empire", {NextAction("wrath of the black empire", 19.0f)}));
    triggers.push_back(new TriggerNode("void blessing", {NextAction("void blessing", 16.0f)}));
    triggers.push_back(new TriggerNode("void shield", {NextAction("void shield", 17.0f)}));
}
