/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "GenericCultistStrategy.h"
#include "Playerbots.h"

GenericCultistStrategy::GenericCultistStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
}

std::vector<NextAction> GenericCultistStrategy::getDefaultActions()
{
    return {
        NextAction("blade of the empire", 12.0f),
        NextAction("horrorbolt", 10.0f),
        NextAction("eldritch blast", 8.0f),
        NextAction("melee", 1.0f)
    };
}

void GenericCultistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("void shield", {NextAction("void shield", 18.0f)}));
    triggers.push_back(new TriggerNode("wrath of the black empire", {NextAction("wrath of the black empire", 15.0f)}));
    triggers.push_back(new TriggerNode("critical health", {NextAction("abyssal reconstruction", 25.0f)}));
    triggers.push_back(new TriggerNode("low health", {NextAction("eldritch mending", 20.0f)}));
}

void CultistAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("light aoe", {NextAction("abyssal orb", 22.0f)}));
    triggers.push_back(new TriggerNode("medium aoe", {NextAction("entropic slam", 24.0f)}));
}

void CultistBuffStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("void blessing", {NextAction("void blessing", 14.0f)}));
    triggers.push_back(new TriggerNode("vision of doom", {NextAction("vision of doom", 13.0f)}));
}
