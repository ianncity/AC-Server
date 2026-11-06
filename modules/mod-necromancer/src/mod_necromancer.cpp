/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 */

void AddNecromancerProgressionScripts();
void AddAscensionNecromancerScripts();
void AddAscensionNecromancerAbilityScripts();
void AddAscensionNecromancerAuraScripts();
void AddAscensionNecromancerContractScripts();
void AddAscensionNecromancerEventScripts();
void AddAscensionNecromancerSummonScripts();

void Addmod_necromancerScripts()
{
    AddAscensionNecromancerScripts();
    AddAscensionNecromancerAbilityScripts();
    AddAscensionNecromancerAuraScripts();
    AddAscensionNecromancerContractScripts();
    AddAscensionNecromancerEventScripts();
    AddAscensionNecromancerSummonScripts();
    AddNecromancerProgressionScripts();
}
