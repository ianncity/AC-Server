#pragma once

#include "Player.h"
#include "SpellInfo.h"

// Talent spell IDs below are rank markers from the with-Bulwark class talent
// candidate. Query the active spec's talent map so these effects do not leak
// across dual specs. The legacy role-node coordinates below do not identify
// cells in this talent candidate and are intentionally not used for mechanics.
namespace AscensionClassTalent
{
constexpr uint32 BONE_PLATING = 998207;
constexpr uint32 CROWNS_CALL = 998213;
constexpr uint32 PROTECTION_FROM_LIGHT = 998130;
constexpr uint32 VOIDWARDING = 998306;
}

inline bool AscensionTalentSelected(Player const* player, uint32 markerSpell)
{
    return player && player->HasTalent(markerSpell, player->GetActiveSpec());
}

inline uint32 AscensionTalentRankForNode(Player const*, uint32, uint32, uint32)
{
    return 0;
}

inline uint32 AscensionTalentNodeRank(Player const*, uint32, uint32, uint32)
{
    return 0;
}

inline void AscensionTalentRestoreMana(Player*, uint32, uint32, float)
{
}

inline uint32 AscensionTalentRoleRank(Player const*, uint32, uint16)
{
    return 0;
}

inline uint32 AscensionTalentAbilityRank(Player const*, uint32, uint32, uint32, uint16)
{
    return 0;
}

inline float AscensionTalentHealingPercent(Player const*, Unit const*, SpellInfo const*, uint32)
{
    // These healing passives are implemented by their granted source spells
    // and source-spell aura scripts; adding another multiplier here double counts them.
    return 0.0f;
}

inline float AscensionTalentDamageReductionPercent(Player const*, uint32)
{
    // Cultist mitigation passives use their source aura or the dedicated
    // source-spell callback; this hook has no source-backed modifier.
    return 0.0f;
}

inline uint32 AscensionTalentSummonRoleRank(Player const*, uint32, uint32, uint16)
{
    return 0;
}

inline float AscensionTalentSummonDefensePercent(Player const* player, uint32, uint32)
{
    return player && player->getClass() == 13 &&
           AscensionTalentSelected(player, AscensionClassTalent::BONE_PLATING) ? 20.0f : 0.0f;
}

inline float AscensionTalentSummonMeleePercent(Player const*, uint32, uint32)
{
    return 0.0f;
}

inline float AscensionTalentDamageFactor(Player const* player, Unit const*, Unit const*, SpellInfo const* info, bool,
                                         uint32, bool isMinion, bool = false, uint32 = 0)
{
    if (!player || !info || player->getClass() != 13)
        return 1.0f;

    float factor = 1.0f;
    if (info->GetSchoolMask() & SPELL_SCHOOL_MASK_FROST)
    {
        if (isMinion && AscensionTalentSelected(player, AscensionClassTalent::CROWNS_CALL))
            factor *= 1.10f;
    }
    return factor;
}
