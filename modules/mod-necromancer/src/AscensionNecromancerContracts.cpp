/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
// Adapted from jealous-sound/azerothcore-wotlk-coa at 0487265f863b9c50e7757e1f113169b80505ba8b.
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "AdvaiAscensionIdentity.h"
#include "AscensionNecromancer.h"
#include "AscensionNecromancerData.h"
#include "DBCStores.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "../../AscensionTalentDamage.h"

namespace AscensionNecromancer
{
namespace
{
enum RangeIndex : uint32
{
    SPELL_RANGE_THIRTY_YARDS = 4
};

uint32 SummonSpellFor(Player* player, Unit const* minion)
{
    if (!player || !minion)
        return 0;
    for (auto const& row : State(player).minions)
        if (row.guid == minion->GetGUID())
            return row.spell;
    return 0;
}
}

namespace
{
class necromancer_spell_contracts : public AllSpellScript
{
public:
    necromancer_spell_contracts() : AllSpellScript("necromancer_spell_contracts", {ALLSPELLHOOK_ON_METADATA_CORRECTION}) { }

    void OnSpellInfoCorrection(SpellInfo* info) override { AscensionNecromancer::ApplyContracts(info); }
};
}

void ApplyContracts(SpellInfo* info)
{
    if (!info)
        return;
    uint32 id = info->Id;
    // Crash guard shared by every Ascension class: a SHAPESHIFT aura naming a form that is missing from
    // SpellShapeshiftForm.dbc makes the core dereference a null row when the aura applies (found by the spell smoke
    // test: Lich Form 504691 is cast on Necromancer login and took the realm down). Keep the aura as a script marker.
    for (SpellEffectInfo& effect : info->Effects)
        if (effect.Effect == SPELL_EFFECT_APPLY_AURA && effect.ApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT &&
            effect.MiscValue && !sSpellShapeshiftFormStore.LookupEntry(effect.MiscValue))
        {
            LOG_WARN("spells", "Spell {} applies shapeshift form {} which is missing from SpellShapeshiftForm.dbc; "
                     "converted to a dummy aura to avoid a null dereference.", info->Id, effect.MiscValue);
            effect.ApplyAuraName = SPELL_AURA_DUMMY;
        }
    // The client resource UI reads this tracker through UnitAura. The source DBC
    // marks it DO_NOT_DISPLAY, which hides it from the client's aura enumeration.
    if (id == 805011 || id == 525004)
        info->Attributes &= ~SPELL_ATTR0_DO_NOT_DISPLAY;
    if (info->Id == 552011)
    {
        info->Effects[0].ApplyAuraName = info->Effects[1].ApplyAuraName = SPELL_AURA_DUMMY;
        return;
    }
    for (auto const& coefficient : NecromancerCoefficients)
        if (coefficient.id == info->Id)
            info->Effects[coefficient.effect].BonusMultiplier = 0.0f;
    if (info->SpellFamilyName != 29)
        return;
    if (id == 500119)
        info->ProcFlags = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG |
                          PROC_FLAG_DONE_PERIODIC;
    if (id == 500117)
        info->ProcFlags = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG |
                          PROC_FLAG_DONE_PERIODIC;
    if (id == 704698)
        info->ProcFlags = PROC_FLAG_DONE_PERIODIC;
    if (id == 704705)
        info->ProcFlags = PROC_FLAG_TAKEN_MELEE_AUTO_ATTACK | PROC_FLAG_TAKEN_RANGED_AUTO_ATTACK |
                          PROC_FLAG_TAKEN_SPELL_MELEE_DMG_CLASS | PROC_FLAG_TAKEN_SPELL_RANGED_DMG_CLASS;
    if (id == 806082 || id == 704689)
        info->ProcFlags = DONE_HIT_PROC_FLAG_MASK;
    if (id == 801735)
        info->ProcFlags = TAKEN_HIT_PROC_FLAG_MASK;
    if (id == 302917)
    {
        // The source DBC row is a random-spell placeholder, while its own aura
        // text defines it as the one-shot free Tomb King summon proc.
        for (SpellEffectInfo& effect : info->Effects)
        {
            effect.Effect = 0;
            effect.ApplyAuraName = SPELL_AURA_NONE;
            effect.TriggerSpell = 0;
            effect.TargetA = SpellImplicitTargetInfo();
            effect.TargetB = SpellImplicitTargetInfo();
        }
        info->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
        info->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        info->StackAmount = 1;
    }
    for (SpellEffectInfo& effect : info->Effects)
    {
        if (effect.ApplyAuraName == 340)
            effect.ApplyAuraName = SPELL_AURA_MOD_CRIT_DAMAGE_BONUS;
        // Ascension's trigger-with-value effect has the same contract as the 3.3.5 native effect.
        if (effect.Effect == 178)
            effect.Effect = SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE;
        else if (effect.Effect == 192)
        {
            if (id != 532521)
                throw std::runtime_error("Unported Ascension cooldown effect in spell " + std::to_string(id));
            // The DBC stores this reduction in seconds; the vanilla handler uses signed milliseconds.
            effect.Effect = SPELL_EFFECT_ASCENSION_MODIFY_COOLDOWN;
            effect.BasePoints = -1000 * std::abs(effect.BasePoints);
            effect.DieSides = 0;
            effect.RealPointsPerLevel = 0.0f;
        }
    }
    auto dummy = [info](uint8 index)
    {
        info->Effects[index].ApplyAuraName = SPELL_AURA_DUMMY;
        info->Effects[index].TriggerSpell = 0;
    };
    auto periodic = [info](uint8 index, uint32 interval)
    {
        info->Effects[index].Effect = SPELL_EFFECT_APPLY_AURA;
        info->Effects[index].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[index].Amplitude = interval;
        info->Effects[index].TriggerSpell = 0;
    };
    for (SpellEffectInfo& effect : info->Effects)
        if ((effect.ApplyAuraName == 42 || effect.ApplyAuraName == 354) &&
            !(id == 503740 && effect.Effect == SPELL_EFFECT_ASCENSION_APPLY_AURA_TO_SUMMONS &&
              effect.TriggerSpell == 707014))
        {
            effect.ApplyAuraName = SPELL_AURA_DUMMY;
            effect.TriggerSpell = 0;
        }
    if (id == 500117)
        info->Effects[0].ApplyAuraName = SPELL_AURA_ABILITY_PERIODIC_CRIT;
    if (id == 805044)
        info->SpellVisual[0] = 832847;
    for (auto const& row : NecromancerSummons)
        if (row.spell == id)
            info->Effects[row.effect].MiscValueB = 64;
    if (OccupancyCreature(id))
        for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
        {
            SpellEffectInfo& effect = info->Effects[index];
            if (effect.Effect == SPELL_EFFECT_APPLY_AREA_AURA_OWNER)
            {
                effect.ApplyAuraName = SPELL_AURA_DUMMY;
                effect.SpellClassMask = flag96();
                continue;
            }
            // Some animate spells double as their minion's occupancy marker. Keep their summon
            // effect so spell_ascension_necromancer_summon can bind; the script ignores the
            // minion's own self-cast of the marker.
            bool summonEffect = false;
            for (auto const& row : NecromancerSummons)
                summonEffect |= row.spell == id && row.effect == index && effect.Effect == SPELL_EFFECT_SUMMON;
            if (!summonEffect)
                effect.Effect = 0;
        }
    if (id == 805011 || id == 525004 || id == 805015)
    {
        info->DurationEntry = sSpellDurationStore.LookupEntry(21);
        for (auto& effect : info->Effects)
            effect.Effect = 0;
        info->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
        info->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
        info->Effects[0].BasePoints = 0;
        info->Effects[0].Amplitude = 0;
        info->StackAmount = id == 805015 ? 0 : 2;
        info->ProcFlags =
            id == 525004 ? 0
                         : PROC_FLAG_DONE_MELEE_AUTO_ATTACK | PROC_FLAG_TAKEN_MELEE_AUTO_ATTACK |
                               PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS | PROC_FLAG_TAKEN_SPELL_MELEE_DMG_CLASS |
                               PROC_FLAG_DONE_RANGED_AUTO_ATTACK | PROC_FLAG_TAKEN_RANGED_AUTO_ATTACK |
                               PROC_FLAG_DONE_SPELL_RANGED_DMG_CLASS | PROC_FLAG_TAKEN_SPELL_RANGED_DMG_CLASS |
                               PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG | PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_NEG |
                               PROC_FLAG_DONE_PERIODIC | PROC_FLAG_TAKEN_PERIODIC;
        if (id == 805015)
        {
            info->Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
            info->Effects[1].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_DONE;
            info->Effects[1].MiscValue = SPELL_SCHOOL_MASK_MAGIC;
            info->Effects[1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
            info->Effects[1].TargetB = SpellImplicitTargetInfo();
            info->Effects[1].BasePoints = 0;
        }
    }
    for (uint32 legacy : {805013, 805014, 805018, 805268, 805269, 805270, 805271, 805602, 805608, 500494, 500524})
        if (id == legacy)
            for (auto& effect : info->Effects)
                effect.Effect = 0;
    if (id == 804360)
        dummy(0);
    if (id == 573223)
    {
        dummy(0);
        info->Effects[0].BasePoints = 19;
    }
    if (id == 706949)
        dummy(0);
    if (id == 704706)
        info->Effects[0].ApplyAuraName = SPELL_AURA_MOD_ATTACK_POWER;
    if (id == 561318)
        info->Effects[0].ValueMultiplier = 1.0f;
    if (id == 807098)
    {
        info->Effects[0].Effect = SPELL_EFFECT_DUMMY;
        info->Effects[1].Effect = info->Effects[2].Effect = 0;
    }
    if (id == 570132)
        for (auto& effect : info->Effects)
            effect.Effect = 0;
    if (id == 570132)
    {
        info->Effects[0].Effect = SPELL_EFFECT_DUMMY;
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ENEMY);
    }
    if (id == 560595)
        for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
            dummy(index);
    if (id == 302888 || id == 302920)
        dummy(1);
    if (id == 300945)
        info->Effects[0].Effect = SPELL_EFFECT_ASCENSION_APPLY_AURA_TO_SUMMONS;
    if (id == 300748 || id == 301326)
        info->Effects[1].BasePoints = info->Effects[0].BasePoints;
    if (id == 504439)
        info->Effects[1].Effect = info->Effects[2].Effect = 0;
    if (id == 300960)
        info->Effects[1].Effect = 0;
    if (id == 503738 || id == 570136)
        dummy(1);
    if (id == 300236)
        info->Effects[0].SpellClassMask[1] = 268435456;
    if (id == 302923 || id == 300234 || id == 301325 || id == 704163)
        dummy(0);
    if (id == 707627)
    {
        info->Effects[0].MiscValue = ASCENSION_STATE_MASKED_CRIT_DAMAGE;
        info->Effects[0].SpellClassMask = flag96(0, 0, 134217728);
        dummy(1);
    }
    if (id == 300958 || id == 300960 || id == 560852 || Named(info, 704669))
    {
        uint8 index = id == 300958 ? 1 : 0;
        info->Effects[index].MiscValue = id == 560852   ? ASCENSION_STATE_MASKED_GUARANTEED_CRIT
                                         : Named(info, 704669) ? ASCENSION_STATE_GLOBAL_CRIT_DAMAGE
                                                        : ASCENSION_STATE_MASKED_CRIT;
        if (id == 300958)
            info->Effects[index].SpellClassMask = flag96(0, 64, 64);
    }
    if (id == 806149 || id == 807925)
        dummy(0);
    if (id == 500982 || id == 500983 || id == 500985)
        info->DurationEntry = sSpellDurationStore.LookupEntry(21);
    if (id == 500983)
    {
        dummy(0);
        info->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
    }
    if (id == 500982)
        info->Effects[2].MiscValue = ASCENSION_CREATURE_GLOBAL_CRIT;
    if (id == 500985)
    {
        info->Effects[1].ApplyAuraName = SPELL_AURA_MOD_THREAT;
        info->Effects[1].MiscValue = SPELL_SCHOOL_MASK_ALL;
    }
    if (id == 500991)
        info->RangeEntry = sSpellRangeStore.LookupEntry(SPELL_RANGE_THIRTY_YARDS);
    if (id == 300580)
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (info->Effects[i].IsEffect())
                dummy(i);
    if (id == 572638 || id == 706472)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_DONE;
        info->Effects[0].MiscValue = SPELL_SCHOOL_MASK_MAGIC;
    }
    if (id == 681529)
        info->Effects[2].Effect = SPELL_EFFECT_APPLY_AURA;
    if (id == 500981)
        info->Effects[2].Effect = 0;
    if (id == 804371)
        info->Effects[0].MiscValue = 444914;
    if (id == 505224)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_SCHOOL_ABSORB;
        info->Effects[0].MiscValue = SPELL_SCHOOL_MASK_MAGIC;
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ALLY);
        info->Effects[0].TargetB = SpellImplicitTargetInfo(0);
        info->DurationEntry = sSpellDurationStore.LookupEntry(9);
    }
    if (id == 705746)
        info->Effects[0].ApplyAuraName = SPELL_AURA_SCHOOL_HEAL_ABSORB;
    if (id == 805049 || id == 807811 || id == 807813)
        info->RecoveryTime = info->CategoryRecoveryTime = 180000;
    if (id == 808017)
    {
        info->Effects[2].ApplyAuraName = SPELL_AURA_SPELL_MAGNET;
        info->ProcFlags = info->ProcCharges = 0;
    }
    if (id == 803773)
        info->CategoryRecoveryTime = 60000;
    if (id == 807796)
        periodic(1, 5000);
    if (id == 808016)
        info->SchoolMask = SPELL_SCHOOL_MASK_FROST | SPELL_SCHOOL_MASK_SHADOW;
    if (id == 680928)
    {
        info->Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
        info->Effects[1].ApplyAuraName = SPELL_AURA_MOD_STUN;
    }
    if (id == 801728)
        dummy(1);
    if (id == 800979)
        info->Effects[1].SpellClassMask = flag96(0, 64, 0);
    if (id == 572777)
    {
        info->Effects[0].Effect = 0;
        info->Effects[1].SpellClassMask = flag96(0, 0, 16777216);
        info->Effects[2].Effect = 0;
    }
    if (id == 801747)
        dummy(0);
    if (id == 704355 || id == 707911 || (id >= 707399 && id <= 707402))
        info->TargetAuraState = 0;
    for (uint32 charge : {800979, 572777, 707176, 807856, 801747})
        if (id == charge)
            info->ProcCharges = info->ProcFlags = 0;
    if (id == 706504)
    {
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
    }
    if (id == 561138)
        periodic(0, 3000);
    if (id == 704676)
        periodic(0, 5000);
    if (id == 301207)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_MOD_RESISTANCE_PCT;
        info->Effects[0].MiscValue = SPELL_SCHOOL_MASK_ALL;
        periodic(1, 5000);
    }
    if (id == 525600 || id == 525389)
        for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
            if (info->Effects[index].ApplyAuraName)
                dummy(index);
    if (id == 803767 || id == 803773)
    {
        info->Effects[0].Effect = SPELL_EFFECT_DUMMY;
        for (uint8 i = 1; i < MAX_SPELL_EFFECTS; ++i)
            info->Effects[i].Effect = 0;
    }
    if (id == 504845 || id == 704860)
    {
        info->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
        info->Effects[0].ApplyAuraName = SPELL_AURA_MOD_ROOT;
        info->AuraInterruptFlags |= AURA_INTERRUPT_FLAG_TAKE_DAMAGE;
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ENEMY);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
    }
    if (id == 504845)
        info->Effects[1].Effect = 0;
    if (id == 800043)
        info->Effects[1].Effect = info->Effects[2].Effect = 0;
    if (id == 500933)
        info->Effects[1].Effect = 0;
    if (id == 500729)
        info->Effects[0].Effect = 0;
    if (id == 803782)
        info->DurationEntry = sSpellDurationStore.LookupEntry(1);
    if (id == 801938)
        info->Effects[1].Effect = 0;
    if (id == 707133)
    {
        dummy(0);
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ANY);
        info->Effects[1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ANY);
    }
    if (Family(info, 0, 16) && id >= 562210 && id <= 562213)
    {
        info->Effects[0].Effect = 0;
        periodic(1, 1000);
        info->Effects[1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        info->Effects[1].TargetB = SpellImplicitTargetInfo();
    }
    if (id == 533240 || id == 500267)
    {
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ENEMY);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
    }
    if (id == 500267)
        info->Effects[1].Effect = 0;
    if (id == 500307)
        info->Effects[0].Effect = 0;
    if (id == 500365)
    {
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_DEST_DEST);
        info->AttributesEx5 |= SPELL_ATTR5_ALWAYS_AOE_LINE_OF_SIGHT;
    }
    if (id == 500443)
    {
        info->TargetAuraSpell = 0;
        info->Effects[2].Effect = 0;
    }
    if ((id >= 533236 && id <= 533239) || Family(info, 2, 67108864) || id == 802121)
    {
        info->Effects[0].Effect = SPELL_EFFECT_DUMMY;
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
        info->Effects[1].Effect = info->Effects[2].Effect = 0;
    }
    if (id == 805031)
    {
        info->Effects[0].Effect = 0;
        info->Effects[1].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ALLY);
        info->Effects[1].TargetB = SpellImplicitTargetInfo();
        info->TargetAuraSpell = 0;
    }
    if (id == 801530)
    {
        info->AttributesEx &= ~SPELL_ATTR1_EXCLUDE_CASTER;
        info->AttributesCu &= ~(SPELL_ATTR0_CU_NEGATIVE_EFF0 | SPELL_ATTR0_CU_NEGATIVE_EFF1);
        info->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
        info->Effects[1].Effect = SPELL_EFFECT_ASCENSION_APPLY_AURA_TO_SUMMONS;
        for (auto& effect : info->Effects)
        {
            effect.TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
            effect.TargetB = SpellImplicitTargetInfo();
        }
    }
    if (id == 801545)
        info->TargetAuraSpell = 0;
    if (id == 531132)
        info->AttributesEx4 |= SPELL_ATTR4_NO_CAST_LOG;
    if (id == 801514)
    {
        info->Effects[2].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_MASTER);
        info->Effects[2].TargetB = SpellImplicitTargetInfo();
    }
    for (uint32 child : {573242, 801241, 707575, 561318, 561095, 570050, 505225, 681463})
        if (id == child)
        {
            info->AttributesEx2 |= SPELL_ATTR2_CANT_CRIT;
            info->AttributesEx3 |= SPELL_ATTR3_IGNORE_CASTER_MODIFIERS;
            info->AttributesEx4 |= SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS;
            for (auto& effect : info->Effects)
                effect.BonusMultiplier = 0.0f;
            info->Effects[0].TargetA = SpellImplicitTargetInfo(
                info->Effects[0].Effect == SPELL_EFFECT_HEAL ? TARGET_UNIT_TARGET_ALLY : TARGET_UNIT_TARGET_ENEMY);
            info->Effects[0].TargetB = SpellImplicitTargetInfo();
        }
}
}  // namespace AscensionNecromancer

namespace
{
using namespace AscensionNecromancer;
float Factor(Player* player, Unit* target, SpellInfo const* info, bool periodic)
{
    if (!info || !target)
        return 1.0f;
    float factor = 1.0f;
    if (player->HasAura(996003) && Named(info, 500237))
        factor *= 1.10f;
    if (player->HasAura(996004) && Named(info, 500968))
        factor *= 1.10f;
    if (player->HasAura(996005) && Lichfrost(info))
        factor *= 1.10f;
    if (!periodic && player->HasAura(996009))
        factor *= 1.02f;
    if (periodic && player->HasAura(996010))
        factor *= 1.02f;
    if ((info->SchoolMask & SPELL_SCHOOL_MASK_SHADOW) && target->HealthBelowPct(35))
        factor *= player->HasAura(807925) ? 1.1f : player->HasAura(806149) ? 1.05f : 1.0f;
    if (player->HasAura(704163) && target->HealthBelowPct(20) && (Disease(info) || info->Id == 800343))
        factor *= 1.0f + Amount(704163) / 100.0f;
    if (Lichfrost(info) && Diseases(player, target) && player->HasAura(707627))
        factor *= 1.3f;
    if (info->Id == 707598)
        factor *= 1.0f + Diseases(player, target) * 0.5f;
    if (info->Id == 802132)
        factor *= 1.0f + Diseases(player, target) * Amount(704291) / 100.0f;
    if (info->Id == 800343 && player->HasAura(570136))
        factor *= 1.0f + Diseases(player, target) * 0.04f;
    if (info->Id == 800343 && player->HasAura(704723))
        factor *= 1.0f + player->GetAuraCount(706504) * 0.1f;
    if (periodic && Named(info, 500968))
        factor *= 2.0f - target->GetHealthPct() / 100.0f;
    if (info->Id == 533240 && player->HasAura(503738))
        factor *= 1.25f;
    if (info->Id == 803779 && player->HasAura(561164))
        factor *= 1.0f + Count(player, {50065, 51065, 50078}) * 0.08f;
    if ((info->SchoolMask & (SPELL_SCHOOL_MASK_FROST | SPELL_SCHOOL_MASK_SHADOW)) &&
        (target->GetCreatureType() == CREATURE_TYPE_HUMANOID || target->GetCreatureType() == CREATURE_TYPE_UNDEAD))
        factor *= player->HasAura(301325) ? 1.08f : player->HasAura(300234) ? 1.04f : 1.0f;
    if (info->SchoolMask & SPELL_SCHOOL_MASK_FROST)
        if (Aura const* freeze = target->GetAura(801728, player->GetGUID()); freeze && player->HasAura(560848))
            factor *= 1.0f + 0.05f * std::min<uint32>(3, freeze->GetStackAmount());
    return factor;
}
class necromancer_scaling : public UnitScript
{
public:
    necromancer_scaling()
        : UnitScript("necromancer_scaling", true,
                     {UNITHOOK_MODIFY_SPELL_EFFECT_BASE_VALUE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_ON_BEFORE_ROLL_MELEE_OUTCOME_AGAINST,
                      UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_HEAL_RECEIVED, UNITHOOK_ON_HEAL,
                      UNITHOOK_ON_DAMAGE})
    {
    }
    float Mark(Unit* target, Unit* attacker)
    {
        Player* player = Owner(attacker);
        if (!player || !IsMinion(player, attacker) || (attacker->GetEntry() != 50065 && attacker->GetEntry() != 51065))
            return 1.0f;
        if (Aura const* mark = target->GetAura(706949, player->GetGUID()))
            return 1.0f + mark->GetStackAmount() * Amount(706949) / 100.0f;
        return 1.0f;
    }
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        damage = uint32(damage * Mark(target, attacker));
        if (Player* player = attacker ? attacker->ToPlayer() : nullptr;
            player && player->getClass() == 13 && player->HasAura(805425) && target &&
            target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
            AddPct(damage, Amount(805425));
        if (Player* player = Owner(attacker); player && player->getClass() == 13 && IsMinion(player, attacker))
        {
            AddPct(damage, AscensionTalentSummonMeleePercent(player, 3, SummonSpellFor(player, attacker)));
            uint32 const rank = AscensionTalentNodeRank(player, 3, 1, 19);
            uint32 const missing = player->GetMaxHealth() - player->GetHealth();
            uint32 const healing = std::min(missing, uint32(float(damage) * 0.005f * float(rank)));
            if (healing)
                player->ModifyHealth(int32(healing));
        }
    }
    void ModifyHealReceived(Unit* target, Unit* caster, uint32& amount, SpellInfo const* info) override
    {
        if (Player* player = Owner(caster); player && info && info->SpellFamilyName == 29)
        {
            AddPct(amount, AscensionTalentHealingPercent(player, target, info, 3));
            if (player->getClass() == 13 && target == player && info->Id == 561043 && player->HealthBelowPct(35))
            {
                uint32 const rank = AscensionTalentNodeRank(player, 3, 2, 1);
                amount += uint32(float(player->GetMaxHealth()) * 0.005f * float(rank));
            }
        }
    }
    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        Player* player = Owner(healer);
        if (player && player->getClass() == 13 && receiver == player)
            AscensionTalentRestoreMana(player, gain, AscensionTalentNodeRank(player, 3, 2, 10), 0.5f);
    }
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (Player* player = Owner(victim); player && player->getClass() == 13)
        {
            if (player == victim)
            {
                uint32 const incoming = uint32(std::max<int32>(0, damage));
                damage = uint32(damage * (1.0f - AscensionTalentDamageReductionPercent(player, 3) / 100.0f));
                uint32 const prevented = incoming - uint32(std::max<int32>(0, damage));
                if (prevented)
                {
                    uint32 const missing = player->GetMaxHealth() - player->GetHealth();
                    uint32 const heal = std::min(missing, std::max<uint32>(1u, uint32(float(prevented) * 0.10f)));
                    if (heal)
                        player->ModifyHealth(int32(heal));
                }
                if (player->HealthBelowPct(35) && player->HasAura(996011))
                    damage = uint32(damage * 0.98f);
            }
            else if (IsMinion(player, victim))
            {
                uint32 const incoming = uint32(std::max<int32>(0, damage));
                damage = uint32(damage * (1.0f - AscensionTalentSummonDefensePercent(player, 3, SummonSpellFor(player, victim)) / 100.0f));
                uint32 const prevented = incoming - uint32(std::max<int32>(0, damage));
                if (prevented)
                {
                    uint32 const missing = victim->GetMaxHealth() - victim->GetHealth();
                    uint32 const heal = std::min(missing, std::max<uint32>(1u, uint32(float(prevented) * 0.10f)));
                    if (heal)
                        victim->ModifyHealth(int32(heal));
                }
            }
        }
    }
    void ModifySpellEffectBaseValue(Unit const* caster, SpellInfo const* info, uint8 index, float& value) override
    {
        Player* player = Owner(caster);
        if (!player)
            return;
        for (auto const& row : NecromancerCoefficients)
            if (info->Id == row.id && index == row.effect)
            {
                float sp = float(row.healing ? player->SpellBaseHealingBonusDone(SpellSchoolMask(row.school))
                                             : player->SpellBaseDamageBonusDone(SpellSchoolMask(row.school)));
                float coefficient = row.sp * 100.0f;
                player->ApplySpellMod(info->Id, SPELLMOD_BONUS_MULTIPLIER, coefficient);
                value += std::max(0.0f, sp) * coefficient / 100.0f + player->GetStat(STAT_INTELLECT) * row.intellect +
                         player->GetTotalAttackPowerValue(BASE_ATTACK) * row.ap;
            }
    }
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* info) override
    {
        if (Player* player = Owner(attacker))
        {
            if (info && (info->Id == 500327 || info->Id == 500585) && attacker->GetEntry() == 50303 &&
                IsMinion(player, attacker) &&
                player->HasAura(804397))
                AddPct(damage, Amount(804397));
            if (attacker == player && player->getClass() == 13 && player->HasAura(805425) && target &&
                target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                AddPct(damage, Amount(805425));
            if (info && info->SpellFamilyName == 29 && !info->HasAttribute(SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS))
                damage = int32(damage * Factor(player, target, info, false) * Mark(target, attacker) *
                    AscensionTalentDamageFactor(player, attacker, target, info, false, 3, IsMinion(player, attacker), false,
                                                IsMinion(player, attacker) ? SummonSpellFor(player, attacker) : 0));
        }
    }
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* info) override
    {
        if (Player* player = Owner(attacker))
        {
            if (attacker == player && player->getClass() == 13 && player->HasAura(805425) && target &&
                target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
                AddPct(damage, Amount(805425));
            if (info && info->SpellFamilyName == 29 && !info->HasAttribute(SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS))
            {
                damage = uint32(damage * Factor(player, target, info, true) *
                    AscensionTalentDamageFactor(player, attacker, target, info, true, 3, IsMinion(player, attacker), Disease(info),
                                                IsMinion(player, attacker) ? SummonSpellFor(player, attacker) : 0));
                if (Named(info, 500217) && Disease(info))
                {
                    uint32 const rank = AscensionTalentNodeRank(player, 3, 0, 3);
                    uint32 const otherDiseases = Diseases(player, target);
                    if (rank && otherDiseases > 1)
                        AddPct(damage, rank * std::min<uint32>(3, otherDiseases - 1));
                }
                if (player->getClass() == 13 && Disease(info))
                {
                    uint32 const rank = AscensionTalentNodeRank(player, 3, 0, 19);
                    Aura const* disease = target->GetAura(info->Id, player->GetGUID());
                    if (rank && disease && disease->GetMaxDuration() > 0)
                    {
                        float const progress = std::clamp(
                            float(disease->GetMaxDuration() - disease->GetDuration()) / float(disease->GetMaxDuration()),
                            0.0f, 1.0f);
                        float const ramp = 1.0f + 0.02f * float(rank) * progress;
                        damage = uint32(float(damage) * ramp);
                    }
                }
            }
        }
    }
    void OnBeforeRollMeleeOutcomeAgainst(Unit const* attacker, Unit const*, WeaponAttackType, int32&, int32&, int32&,
                                         int32&, int32& crit, int32&, int32& dodge, int32& parry, int32&) override
    {
        Player* player = Owner(attacker);
        if (!player || !IsMinion(player, attacker))
            return;
        if (SummonSpellFor(player, attacker) == 500330)
            crit += int32(100 * AscensionTalentNodeRank(player, 3, 1, 1));
        crit += int32(std::max(0.0f, player->GetFloatValue(PLAYER_CRIT_PERCENTAGE) - 5.0f) * 100);
        if (player->HasAura(561138))
            crit = 10000;
        int32 expertise = int32(std::max(0.0f, player->m_modMeleeHitChance) * 100);
        dodge = std::max(0, dodge - expertise);
        parry = std::max(0, parry - expertise);
    }
};
}  // namespace
void AddAscensionNecromancerContractScripts()
{
    new AscensionNecromancer::necromancer_spell_contracts();
    new necromancer_scaling();
}
