/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "GenericCultistNonCombatStrategy.h"
#include "Playerbots.h"

GenericCultistNonCombatStrategy::GenericCultistNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
}

void GenericCultistNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("void blessing", {NextAction("void blessing", 15.0f)}));
    triggers.push_back(new TriggerNode("void shield", {NextAction("void shield", 14.0f)}));
    triggers.push_back(new TriggerNode("party member almost full health", {NextAction("eldritch mending on party", 20.0f)}));
}
