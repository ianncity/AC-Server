/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <algorithm>
#include <stdexcept>

#include "AdvaiAscensionData.h"
#include "AdvaiAscensionIdentity.h"
#include "AllSpellScript.h"
#include "AscensionCultist.h"
#include "AscensionNecromancer.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "World.h"

void AddSC_AscensionCultist();
void AddSC_AscensionCultistAbilities();
void AddSC_AscensionCultistAuras();
void AddSC_AscensionCultistContracts();
void AddSC_AscensionCultistEvents();
void AddSC_AscensionCultistSummons();
void AddAscensionNecromancerScripts();
void AddAscensionNecromancerAbilityScripts();
void AddAscensionNecromancerAuraScripts();
void AddAscensionNecromancerContractScripts();
void AddAscensionNecromancerEventScripts();
void AddAscensionNecromancerSummonScripts();

namespace Advai::Ascension
{
bool Enabled() { return sPlayerbotAIConfig.AdvaiEnabled && sPlayerbotAIConfig.AdvaiAscensionClasses; }

bool CanThink(Player* player)
{
    if (!player)
        return false;
    if (!Manager::Instance().Applies(player))
        return true;
    auto* ai = GET_PLAYERBOT_AI(player);
    return ai && ai->AllowActivity();
}

void ValidateContent()
{
    if (!Enabled())
        return;
    for (uint32 spell : Data::Spells)
        if (!sSpellMgr->GetSpellInfo(spell))
            throw std::runtime_error("ADVAI Ascension class content is missing required spell " +
                                     std::to_string(spell));
    if (!sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED))
        throw std::runtime_error("ADVAI Ascension classes require durable native player settings for charge recovery");
}

class Metadata : public AllSpellScript
{
public:
    Metadata() : AllSpellScript("advai_ascension_metadata", {ALLSPELLHOOK_ON_METADATA_CORRECTION}) {}

    void OnSpellInfoCorrection(SpellInfo* info) override
    {
        if (!std::binary_search(Data::Spells.begin(), Data::Spells.end(), info->Id))
            return;
        // The dedicated Cultist and Necromancer modules own their spell
        // contract hooks. Dispatching them here would apply each correction
        // twice, and some corrections intentionally remove the source effect.
        auto charge = std::lower_bound(Data::Charges.begin(), Data::Charges.end(), info->Id,
                                       [](auto const& value, uint32 id) { return value.Spell < id; });
        if (charge != Data::Charges.end() && charge->Spell == info->Id)
        {
            info->MaxCharges = charge->Maximum;
            info->ChargeRecoveryTime = charge->Recovery;
            info->ChargeRecoveryKey = charge->Root;
            info->ChargeCategoryId = charge->Category;
        }
    }
};
}  // namespace Advai::Ascension

void AddAdvaiAscensionScripts()
{
    new Advai::Ascension::Metadata();
    // The Cultist/Necromancer runtime scripts are registered once by their
    // dedicated modules (Addmod_cultistScripts / Addmod_necromancerScripts).
    // Registering them here as well double-registered every PlayerScript,
    // UnitScript and AuraScript hook (double procs, double resource ticks).
}
