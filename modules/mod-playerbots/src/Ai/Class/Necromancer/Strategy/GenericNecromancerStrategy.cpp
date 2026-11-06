/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "GenericNecromancerStrategy.h"
#include "Playerbots.h"

GenericNecromancerStrategy::GenericNecromancerStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
}

std::vector<NextAction> GenericNecromancerStrategy::getDefaultActions()
{
    return {
        NextAction("lichfrost", 12.0f),
        NextAction("heartchill", 10.0f),
        NextAction("chill of the tomb", 8.0f),
        NextAction("shoot", 1.0f)
    };
}

void GenericNecromancerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("plague of undeath", {NextAction("plague of undeath", 18.0f)}));
    triggers.push_back(new TriggerNode("harvest plague", {NextAction("harvest plague", 16.0f)}));
    triggers.push_back(new TriggerNode("low mana", {NextAction("siphon mana", 20.0f)}));
    triggers.push_back(new TriggerNode("critical health", {NextAction("transfer life", 25.0f)}));
}

void NecromancerAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("light aoe", {NextAction("corpse explosion", 22.0f)}));
    triggers.push_back(new TriggerNode("medium aoe", {NextAction("blight", 24.0f)}));
}

void NecromancerBuffStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("undead: assault", {NextAction("undead: assault", 15.0f)}));
    triggers.push_back(new TriggerNode("foul mandate", {NextAction("foul mandate", 14.0f)}));
}
