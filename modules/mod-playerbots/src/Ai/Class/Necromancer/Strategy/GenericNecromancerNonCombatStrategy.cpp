/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "GenericNecromancerNonCombatStrategy.h"
#include "Playerbots.h"

GenericNecromancerNonCombatStrategy::GenericNecromancerNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
}

void GenericNecromancerNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("no pet", {NextAction("raise: lesser skeletal warrior", 20.0f)}));
    triggers.push_back(new TriggerNode("undead: assault", {NextAction("undead: assault", 15.0f)}));
}
