/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 */

void AddCultistProgressionScripts();
void AddSC_AscensionCultist();
void AddSC_AscensionCultistAbilities();
void AddSC_AscensionCultistAuras();
void AddSC_AscensionCultistContracts();
void AddSC_AscensionCultistEvents();
void AddSC_AscensionCultistSummons();

void Addmod_cultistScripts()
{
    AddSC_AscensionCultist();
    AddSC_AscensionCultistAbilities();
    AddSC_AscensionCultistAuras();
    AddSC_AscensionCultistContracts();
    AddSC_AscensionCultistEvents();
    AddSC_AscensionCultistSummons();
    AddCultistProgressionScripts();
}
