/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "NecromancerTriggers.h"
#include "Playerbots.h"

bool NecromancerNoPetTrigger::IsActive()
{
    return !bot->GetPet() && !AI_VALUE(Unit*, "pet target");
}
