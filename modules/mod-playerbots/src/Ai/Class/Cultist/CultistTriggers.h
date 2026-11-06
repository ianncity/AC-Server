/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_CULTISTTRIGGERS_H
#define PLAYERBOTS_CULTISTTRIGGERS_H

#include "GenericTriggers.h"

class PlayerbotAI;

class VoidShieldTrigger : public BuffTrigger
{
public:
    VoidShieldTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "void shield") {}
};

class VoidBlessingTrigger : public BuffTrigger
{
public:
    VoidBlessingTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "void blessing") {}
};

class WrathOfTheBlackEmpireTrigger : public DebuffTrigger
{
public:
    WrathOfTheBlackEmpireTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "wrath of the black empire") {}
};

class EldritchTentacleTrigger : public BuffTrigger
{
public:
    EldritchTentacleTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "eldritch tentacle") {}
};

#endif
