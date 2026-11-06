/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_NECROMANCERTRIGGERS_H
#define PLAYERBOTS_NECROMANCERTRIGGERS_H

#include "GenericTriggers.h"

class PlayerbotAI;

class PlagueOfUndeathTrigger : public DebuffTrigger
{
public:
    PlagueOfUndeathTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "plague of undeath") {}
};

class HarvestPlagueTrigger : public DebuffTrigger
{
public:
    HarvestPlagueTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "harvest plague") {}
};

class UndeadProtectTrigger : public BuffTrigger
{
public:
    UndeadProtectTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "undead: protect") {}
};

class UndeadAssaultTrigger : public BuffTrigger
{
public:
    UndeadAssaultTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "undead: assault") {}
};

class FoulMandateTrigger : public BuffTrigger
{
public:
    FoulMandateTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "foul mandate") {}
};

class NecromancerNoPetTrigger : public Trigger
{
public:
    NecromancerNoPetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "no pet", 5) {}

    bool IsActive() override;
};

#endif
