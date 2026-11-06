/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
// Adapted from jealous-sound/azerothcore-wotlk-coa at 0487265f863b9c50e7757e1f113169b80505ba8b.
#include <algorithm>
#include <stdexcept>
#include <string>

#include "AdvaiAscensionIdentity.h"
#include "AscensionCultist.h"
#include "AscensionCultistData.h"
#include "DBCStores.h"
#include "Group.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "../../AscensionTalentDamage.h"
namespace AscensionCultist
{
void ApplyContracts(SpellInfo* info)
{
    if (!info || info->SpellFamilyName != 31)
        return;
    uint32 id = info->Id;
    constexpr uint32 HeraldSpellModSelector = 0x00040000;
    // This unused Cultist family bit scopes Herald's native SpellMods to the
    // three source spells named by its tooltip. The bit is not used by any
    // active Family 31 SpellClassMask, and core handlers that inspect it are
    // gated to other spell families.
    if (Any(info, {805133, 803062, 560334}))
        info->SpellFamilyFlags[1] |= HeraldSpellModSelector;
    if (id == 704477)
    {
        SpellEffectInfo& castTime = info->Effects[0];
        SpellEffectInfo& cooldown = info->Effects[2];
        if (castTime.Effect != SPELL_EFFECT_APPLY_AURA ||
            castTime.ApplyAuraName != SPELL_AURA_ADD_PCT_MODIFIER || castTime.MiscValue != SPELLMOD_CASTING_TIME ||
            cooldown.Effect != SPELL_EFFECT_APPLY_AURA ||
            cooldown.ApplyAuraName != SPELL_AURA_ADD_FLAT_MODIFIER || cooldown.MiscValue != SPELLMOD_COOLDOWN)
            throw std::runtime_error("Unexpected Herald of The Black Empire spell-mod contract");
        flag96 const selector(0, HeraldSpellModSelector, 0);
        castTime.SpellClassMask = selector;
        cooldown.SpellClassMask = selector;
    }
    if (id == 300304)
    {
        SpellEffectInfo& unsupportedGlobalDamage = info->Effects[2];
        if (unsupportedGlobalDamage.Effect != SPELL_EFFECT_APPLY_AURA ||
            unsupportedGlobalDamage.ApplyAuraName != SPELL_AURA_ADD_PCT_MODIFIER ||
            unsupportedGlobalDamage.MiscValue != SPELLMOD_DAMAGE)
            throw std::runtime_error("Unexpected Terrorbolts extra damage contract");
        // The source Spell.dbc has no effect in slot 2. The deployed row had
        // added an undocumented +10% damage modifier to every Cultist spell.
        unsupportedGlobalDamage.Effect = 0;
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
            if (id != 301184)
                throw std::runtime_error("Unported Ascension cooldown effect in spell " + std::to_string(id));
            // Blade of the Depths applies its conditional reduction in its spell script.
            effect.Effect = SPELL_EFFECT_DUMMY;
        }
    }
    if (id == 804633)
    {
        SpellEffectInfo& dispelResistance = info->Effects[0];
        if (dispelResistance.Effect != SPELL_EFFECT_APPLY_AURA ||
            dispelResistance.ApplyAuraName != SPELL_AURA_MOD_DISPEL_RESIST)
            throw std::runtime_error("Unexpected Void-Enchanted Armor effect contract");
        // The source row stores -21, which makes the tooltip say -20% and clamps
        // dispel resistance to zero. Set the actual modifier to +20%.
        dispelResistance.BasePoints = 20;
        dispelResistance.DieSides = 0;
    }
    auto dummy = [info](uint8 slot)
    {
        info->Effects[slot].ApplyAuraName = SPELL_AURA_DUMMY;
        info->Effects[slot].TriggerSpell = 0;
    };
    auto aura = [info](uint8 slot, AuraType type, int32 amount, int32 misc, uint32 target)
    {
        auto& effect = info->Effects[slot];
        effect.Effect = SPELL_EFFECT_APPLY_AURA;
        effect.ApplyAuraName = type;
        effect.BasePoints = amount;
        effect.DieSides = 0;
        effect.MiscValue = misc;
        effect.TriggerSpell = 0;
        effect.TargetA = SpellImplicitTargetInfo(target);
        effect.TargetB = SpellImplicitTargetInfo();
    };
    if (id == 805133)
    {
        if (info->Effects[0].Effect || info->Effects[1].Effect || info->Effects[2].Effect)
            throw std::runtime_error("Unexpected Entropic Host source effects");
        aura(0, SPELL_AURA_PERIODIC_HEAL, 0, 0, TARGET_UNIT_TARGET_ALLY);
        info->RangeEntry = sSpellRangeStore.LookupEntry(4);  // Standard 30yd friendly-spell range.
        if (!info->RangeEntry)
            throw std::runtime_error("Missing Entropic Host range contract");
        info->Effects[0].Amplitude = 1000;  // Source tooltip identifies a one-second heal interval.
        info->Effects[0].BonusMultiplier = 0.32f;
        info->DurationEntry = sSpellDurationStore.LookupEntry(8);  // 15s, same as the source helper auras.
        if (!info->DurationEntry)
            throw std::runtime_error("Missing Entropic Host duration contract");
    }
    if (id == 805135)
    {
        if (info->Effects[0].ApplyAuraName != SPELL_AURA_DUMMY)
            throw std::runtime_error("Unexpected Drained Essence marker contract");
        info->StackAmount = 2;
    }
    if (id == VoidRune)
        aura(0, SPELL_AURA_DUMMY, 0, 0, TARGET_UNIT_CASTER);
    for (auto list :
         {std::pair(CultistEvents, std::size(CultistEvents)), std::pair(CultistDrivers, std::size(CultistDrivers))})
        for (size_t n = 0; n < list.second; ++n)
            if (list.first[n] == id)
            {
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (info->Effects[i].ApplyAuraName == SPELL_AURA_PROC_TRIGGER_SPELL ||
                        info->Effects[i].ApplyAuraName == 354 || info->Effects[i].ApplyAuraName == 353)
                        dummy(i);
                info->ProcFlags = info->ProcCharges = 0;
            }
    if (id == 300276)
        info->ProcFlags = PROC_FLAG_DONE_MELEE_AUTO_ATTACK | PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS;
    if (id == 681087)
        info->ProcFlags = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG;
    if (id == 704883)
        info->ProcFlags = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG | PROC_FLAG_DONE_PERIODIC;
    for (uint32 copy : CultistCopies)
        if (id == copy)
        {
            info->AttributesEx2 |= SPELL_ATTR2_CANT_CRIT;
            info->AttributesEx3 |= SPELL_ATTR3_IGNORE_CASTER_MODIFIERS;
            info->AttributesEx4 |= SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS;
            info->AttributesCu |= SPELL_ATTR0_CU_IGNORE_ARMOR;
            info->AscensionInheritsResolvedAmount = true;
            bool damage = id == 707750 || id == 807083 || id == 525062 || id == 706910 || id == 520450;
            info->Effects[0].TargetA =
                SpellImplicitTargetInfo(damage ? TARGET_UNIT_TARGET_ENEMY : TARGET_UNIT_TARGET_ALLY);
            info->Effects[0].TargetB = SpellImplicitTargetInfo();
        }
    if (id == 570263 || id == 500748 || id == 301983 || id == 520450 || id == 520497)
    {
        info->AttributesEx2 &= ~SPELL_ATTR2_CANT_CRIT;
        info->AscensionInheritsResolvedAmount = id == 520450 || id == 520497;
    }
    if (id == 807083)
        info->Effects[1].Effect = 0;
    if (id == 301983 || id == 806175)
        info->Effects[1].Effect = 0;
    if (id == 502133)
    {
        dummy(0);
        info->Effects[1].ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
    }
    if (id == 705106 || id == 705107)
        dummy(0);
    if (id == 802046)
        info->SchoolMask = SPELL_SCHOOL_MASK_NORMAL;
    if (id == 680556)
        info->Effects[0].MiscValue = 250043;
    if (id == 806769)
        info->Effects[2].MiscValue = 250044;
    if (Named(info, 804152))
        info->CasterAuraSpell = 680602;
    if (Any(info, {805116, 804152}))
        info->AscensionIgnoreAbsorb = Named(info, 805116);
    if (Named(info, 567524))
        aura(0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, 0, SPELL_SCHOOL_MASK_ALL, TARGET_UNIT_CASTER);
    if (id == 500727)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[0].TriggerSpell = 0;
        dummy(1);
    }
    if (id == Madness)
        info->Effects[1].Effect = 0;
    if (id == 801157)
        info->Effects[1].Effect = SPELL_EFFECT_DUMMY;
    if (id == 807877)
        dummy(1);
    if (id == 807878)
        info->Effects[1].MiscValue = BlackBlood;
    if (id == 806384 || id == 300284 || id == 805117 || id == 681326 || id == 301258)
        dummy(0);
    if (id == 300287 || id == 300290 || id == 560977)
    {
        dummy(0);
        dummy(1);
        if (id == 560977)
            dummy(2);
    }
    if (id == 300287)
        aura(0, SPELL_AURA_MOD_DISPEL_RESIST, 25, 0, TARGET_UNIT_CASTER);
    if (id == 300290)
        aura(1, SPELL_AURA_MOD_MINIMUM_SPEED, 100, 0, TARGET_UNIT_CASTER);
    if (id == 681106 || id == 707753)
        dummy(0);
    if (id == 300277)
        aura(2, SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, 0, SPELL_SCHOOL_MASK_ALL, TARGET_UNIT_CASTER);
    if (id == 520326)
    {
        dummy(2);
        info->Effects[0].MiscValue = 250042;
    }
    if (id == 681088)
    {
        dummy(0);
        dummy(2);
    }
    for (uint32 infusion : CultistInfusions)
        if (id == infusion)
            info->AuraInterruptFlags = 0;
    if (Named(info, 500714))
        info->Effects[1].Effect = SPELL_EFFECT_DUMMY;
    if (id == BlackBlood)
    {
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ALLY);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
    }
    if (id == 802048)
        dummy(0);
    if (id == 574147)
    {
        aura(0, SPELL_AURA_MOD_HEALING_DONE, 0, SPELL_SCHOOL_MASK_ALL, TARGET_UNIT_CASTER);
        dummy(1);
    }
    if (id == 680609)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[0].TriggerSpell = 0;
    }
    if (id == 680579)
        dummy(1);
    if (id == 680574)
        for (auto& effect : info->Effects)
            if (effect.ApplyAuraName == SPELL_AURA_MOD_BLOCK_CRIT_CHANCE)
                effect.TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
    if (id == 807883 || id == 681476 || id == 567529)
        info->DurationEntry = sSpellDurationStore.LookupEntry(21);
    if (id == 520345)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[0].TriggerSpell = 0;
        dummy(1);
        dummy(2);
        info->Effects[2].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ALLY);
    }
    if (id == 520346)
    {
        info->Effects[1].BasePoints = info->Effects[0].BasePoints;
        info->Effects[1].DieSides = info->Effects[0].DieSides;
        info->Effects[1].RealPointsPerLevel = info->Effects[0].RealPointsPerLevel;
    }
    if (id == 520348)
    {
        info->Effects[0].Effect = SPELL_EFFECT_DUMMY;
        info->Effects[1].Effect = 0;
    }
    if (id == 520333)
    {
        info->Effects[EFFECT_1].Effect = 0;
        info->Effects[EFFECT_2].Effect = 0;
    }
    if (id == 680576)
    {
        info->Effects[1].ApplyAuraName = SPELL_AURA_MOD_DECREASE_SPEED;
        info->Effects[1].MiscValue = 0;
    }
    if (id == 561392)
    {
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER_AREA_RAID);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
        info->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(10);
    }
    if (id == 255281)
    {
        info->Effects[1].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[1].TriggerSpell = 0;
    }
    if (id == 704476)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[0].TriggerSpell = 0;
    }
    if (id == 804277)
        info->Effects[2].Effect = 0;
    if (id == 804275)
        info->Effects[2].BasePoints = 19;
    if (id == 807124)
        info->Effects[1].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
    if (id == 806769)
        info->Effects[0].Effect = 0;
    if (id == 502133 || id == 301186 || id == 561288 || id == 806250 || id == 301259)
        info->ProcFlags = info->ProcCharges = 0;
    if (id == 301259)
        dummy(0);
    if (id == 520388)
        info->ProcCharges = 5;
    if (id == 600327)
        info->ProcCharges = 10;
    if (id == 520450 || id == 520497)
    {
        info->AuraInterruptFlags = 0;
    }
    if (id == 520498)
    {
        info->Effects[0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_TARGET_ENEMY);
        info->Effects[0].TargetB = SpellImplicitTargetInfo();
    }
    if (id == 807216)
    {
        info->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
        info->Effects[0].TriggerSpell = 0;
    }
    if (id == 706910)
    {
        for (uint8 slot = 1; slot < MAX_SPELL_EFFECTS; ++slot)
        {
            aura(slot, SPELL_AURA_DUMMY, 0, 0, info->Effects[0].TargetA.GetTarget());
            info->Effects[slot].TargetB = SpellImplicitTargetInfo();
        }
    }
    if (id == 582591)
    {
        aura(1, SPELL_AURA_DUMMY, 0, 0, TARGET_UNIT_CASTER);
        aura(2, SPELL_AURA_DUMMY, 0, 0, TARGET_UNIT_CASTER);
    }
    if (id == 800430)
        info->Effects[1].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
    info->_InitializeExplicitTargetMask();
}

namespace
{
class cultist_spell_contracts : public AllSpellScript
{
public:
    cultist_spell_contracts() : AllSpellScript("cultist_spell_contracts", {ALLSPELLHOOK_ON_METADATA_CORRECTION}) { }

    void OnSpellInfoCorrection(SpellInfo* info) override { AscensionCultist::ApplyContracts(info); }
};
}
}  // namespace AscensionCultist
namespace
{
using namespace AscensionCultist;
class cultist_scaling : public UnitScript
{
public:
    cultist_scaling()
        : UnitScript("cultist_scaling", true,
                     {UNITHOOK_MODIFY_SPELL_EFFECT_BASE_VALUE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED,
                      UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_ON_HEAL, UNITHOOK_ON_DAMAGE})
    {
    }
    void ModifySpellEffectBaseValue(Unit const* caster, SpellInfo const* info, uint8 index, float& value) override
    {
        Player* player = Owner(caster);
        if (!player || !info || info->SpellFamilyName != 31)
            return;
        if (info->Id == CthunDamage && index == EFFECT_0 && caster->GetEntry() == CthunTentacle)
        {
            double const level = player->GetLevel();
            float const baseVal = std::max(value, 220.0f);
            value = baseVal * float(0.0267291844060354 + 0.0048541098014737 * level + 0.0001859597762293 * level * level);
        }
        for (auto const& row : CultistCoefficients)
            if (row.spell == info->Id && row.effect == index)
            {
                float healing = row.healing;
                if (player->HasAura(300284) && (Named(info, 500711) || Any(info, {808050, 808051, 808052})))
                    healing *= 1 + Amount(300284) / 100.0f;
                float sp = row.sp;
                if (Named(info, 500715) && index == 0 && player->HasAura(681088))
                    value *= 1 + Amount(681088) / 100.0f;
                if (Named(info, 500720) && player->HasAura(803339))
                    sp = .3f;
                value += sp * std::max(0, player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)) +
                         row.ap * player->GetTotalAttackPowerValue(BASE_ATTACK) +
                         healing * std::max(0, player->SpellBaseHealingBonusDone(info->GetSchoolMask())) +
                         row.stamina * player->GetStat(STAT_STAMINA) + row.intellect * player->GetStat(STAT_INTELLECT);
            }
        bool tentacle = caster->GetEntry() == CthunTentacle || caster->GetEntry() == 501464 ||
                        caster->GetEntry() == 500465 || caster->GetEntry() == 50096 || caster->GetEntry() == 500464;
        if (caster != player && tentacle && State(player).summons.count(caster->GetGUID()) && player->HasAura(802048))
            value *= 1 + Amount(802048) / 100.0f;
        value = std::clamp(value, float(INT32_MIN / 2), float(INT32_MAX / 2));
    }
    float Factor(Unit* target, Unit* caster, SpellInfo const* info, bool healing)
    {
        Player* player = Owner(caster);
        if (!player || !info || info->SpellFamilyName != 31 || Derived(info))
            return 1;
        float factor = player->HasAura(Herald) ? 1 + Amount(Herald, 2) / 100.0f : 1;
        if (healing && Any(info, {808050, 808051, 808052}) && player->HasAura(705106))
            factor *= 1 + Amount(705106) / 100.0f;
        if (!healing && Any(info, {808043, 808044, 808045}) && player->HasAura(705107))
            factor *= 1 + Amount(705107) / 100.0f;
        // Class glyph auras use distinct spell IDs and do not alter the base
        // class abilities, so they can be removed with the ordinary glyph UI.
        if (!healing && player->HasAura(996000) && Named(info, 500720))
            factor *= 1.10f;
        if (!healing && player->HasAura(996001) && Named(info, 500714))
            factor *= 1.10f;
        if (healing && player->HasAura(996002) && Named(info, 500711))
            factor *= 1.10f;
        if (healing && target == player && player->HasAura(996006))
            factor *= 1.03f;
        if (!healing && caster == player && (info->GetSchoolMask() & SPELL_SCHOOL_MASK_SHADOW) && player->HasAura(996007))
            factor *= 1.02f;
        if (!healing && caster != player && State(player).summons.count(caster->GetGUID()) && player->HasAura(802048))
            (void)target;
        return factor;
    }
    void ModifySpellDamageTaken(Unit* target, Unit* caster, int32& damage, SpellInfo const* info) override
    {
        damage = int32(damage * Factor(target, caster, info, false));
        if (Player* player = Owner(caster); player && info && info->SpellFamilyName == 31 && !Derived(info))
        {
            bool const classSummon = caster != player && State(player).summons.count(caster->GetGUID());
            damage = int32(damage * AscensionTalentDamageFactor(player, caster, target, info, false, 0, classSummon, false,
                                                                classSummon ? caster->GetEntry() : 0));
            if (info->Id == 680735 && target->HealthBelowPct(20))
            {
                uint32 const talent = 995140; // Soul Rend, first marker rank.
                uint32 const rank = AscensionTalentNodeRank(player, 0, 1, 8);
                uint32 const remaining = player->GetSpellCooldownDelay(680735);
                if (rank && remaining && !State(player).timers.HasTimeUntilEvent(talent))
                {
                    uint32 const reduction = std::min<uint32>(remaining, 1000 * rank);
                    player->ModifySpellCooldown(680735, -int32(reduction));
                    State(player).timers.ScheduleEvent(talent, Milliseconds(1000));
                }
            }
            if (!classSummon && info->Id == 500735)
            {
                uint32 const rank = AscensionTalentNodeRank(player, 0, 1, 10);
                uint32 const missing = player->GetMaxHealth() - player->GetHealth();
                uint32 const healing = std::min(missing, uint32(float(std::max(0, damage)) * 0.005f * float(rank)));
                if (healing)
                    player->ModifyHealth(int32(healing));
            }
            if (target && target->GetHealthPct() < 20.0f)
                AscensionTalentRestoreMana(player, uint32(std::max(0, damage)), AscensionTalentNodeRank(player, 0, 1, 19), 0.5f);
            if (caster != player && State(player).summons.count(caster->GetGUID()))
            {
                uint32 const rank = AscensionTalentNodeRank(player, 0, 2, 19);
                uint32 const activeSummons = std::min<uint32>(3, uint32(State(player).summons.size()));
                damage = int32(float(damage) * (1.0f + 0.01f * float(rank * activeSummons)));
            }
        }
    }
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* caster, uint32& damage, SpellInfo const* info) override
    {
        if (info && info->HasAura(SPELL_AURA_PERIODIC_HEAL))
            return;
        damage = uint32(damage * Factor(target, caster, info, false));
        if (Player* player = Owner(caster); player && info && info->SpellFamilyName == 31 && !Derived(info))
        {
            bool const classSummon = caster != player && State(player).summons.count(caster->GetGUID());
            damage = uint32(damage * AscensionTalentDamageFactor(player, caster, target, info, true, 0, classSummon, false,
                                                                 classSummon ? caster->GetEntry() : 0));
            if (!classSummon && info->Id == 500735)
            {
                uint32 const rank = AscensionTalentNodeRank(player, 0, 1, 10);
                uint32 const missing = player->GetMaxHealth() - player->GetHealth();
                uint32 const healing = std::min(missing, uint32(float(damage) * 0.005f * float(rank)));
                if (healing)
                    player->ModifyHealth(int32(healing));
            }
            if (target && target->GetHealthPct() < 20.0f)
                AscensionTalentRestoreMana(player, damage, AscensionTalentNodeRank(player, 0, 1, 19), 0.5f);
            if (caster != player && State(player).summons.count(caster->GetGUID()))
            {
                uint32 const rank = AscensionTalentNodeRank(player, 0, 2, 19);
                uint32 const activeSummons = std::min<uint32>(3, uint32(State(player).summons.size()));
                damage = uint32(float(damage) * (1.0f + 0.01f * float(rank * activeSummons)));
            }
        }
    }
    void ModifyHealReceived(Unit* target, Unit* caster, uint32& amount, SpellInfo const* info) override
    {
        if (target && target->HasAura(520498))
        {
            amount = 0;
            return;
        }
        amount = uint32(amount * Factor(target, caster, info, true));
        if (Player* player = Owner(caster); player && info && info->SpellFamilyName == 31 && !Derived(info))
        {
            AddPct(amount, AscensionTalentHealingPercent(player, target, info, 0));
            if (target && target->GetHealthPct() < 50.0f && info->Id == 520152)
                AscensionTalentRestoreMana(player, amount, AscensionTalentNodeRank(player, 0, 0, 8), 0.5f);
        }
    }
    void ModifyMeleeDamage(Unit*, Unit* attacker, uint32& damage) override
    {
        Player* player = Owner(attacker);
        if (player == attacker && player && player->HasAura(Herald))
            AddPct(damage, Amount(Herald, 2));
        if (player && player->getClass() == 12 && attacker != player && State(player).summons.count(attacker->GetGUID()))
        {
            AddPct(damage, AscensionTalentSummonMeleePercent(player, 0, attacker->GetEntry()));
            if (attacker->GetEntry() == 512721)
                AscensionTalentRestoreMana(player, damage, AscensionTalentNodeRank(player, 0, 2, 1), 0.5f);
            uint32 const rank = AscensionTalentNodeRank(player, 0, 2, 19);
            uint32 const activeSummons = std::min<uint32>(3, uint32(State(player).summons.size()));
            AddPct(damage, rank * activeSummons);
        }
    }
    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        Player* player = Owner(healer);
        if (!player || player->getClass() != 12 || !receiver)
            return;
        uint32 const rank = AscensionTalentNodeRank(player, 0, 0, 19);
        if (!rank)
            return;
        uint32 const missing = receiver->GetMaxHealth() - receiver->GetHealth();
        uint32 const echo = std::min(missing, uint32(float(gain) * 0.005f * float(rank)));
        if (echo)
            receiver->ModifyHealth(int32(echo));
    }
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (Player* affectedPlayer = victim ? victim->ToPlayer() : nullptr)
        {
            bool protectedByCultist = false;
            if (Group* group = affectedPlayer->GetGroup())
                for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                    if (Player* member = itr->GetSource(); member && member != affectedPlayer &&
                        (AscensionTalentSelected(member, AscensionClassTalent::PROTECTION_FROM_LIGHT) ||
                         AscensionTalentSelected(member, AscensionClassTalent::VOIDWARDING)))
                    {
                        protectedByCultist = true;
                        break;
                    }

            if (protectedByCultist)
                damage = uint32(float(damage) * 0.97f);
        }

        if (Player* player = Owner(attacker); player && player->getClass() == 12)
        {
        }
        Player* player = Owner(victim);
        if (!player)
            return;
        if (player->getClass() == 12 && player == victim)
        {
            uint32 const incoming = damage;
            damage = uint32(damage * (1.0f - AscensionTalentDamageReductionPercent(player, 0) / 100.0f));
            uint32 const prevented = incoming - damage;
            if (prevented)
                if (std::list<Unit*> allies = Allies(player, victim, 40.0f, 1); !allies.empty())
                {
                    Unit* ally = allies.front();
                    uint32 const missing = ally->GetMaxHealth() - ally->GetHealth();
                    uint32 const heal = std::min(missing, std::max<uint32>(1u, uint32(float(prevented) * 0.10f)));
                    if (heal)
                        ally->ModifyHealth(int32(heal));
                }
        }
        else if (player->getClass() == 12 && victim != player && State(player).summons.count(victim->GetGUID()))
        {
            uint32 const incoming = damage;
            damage = uint32(damage * (1.0f - AscensionTalentSummonDefensePercent(player, 0, victim->GetEntry()) / 100.0f));
            uint32 const prevented = incoming - damage;
            if (prevented)
            {
                uint32 const missing = victim->GetMaxHealth() - victim->GetHealth();
                uint32 const heal = std::min(missing, std::max<uint32>(1u, uint32(float(prevented) * 0.10f)));
                if (heal)
                    victim->ModifyHealth(int32(heal));
            }
        }
        if (player->getClass() == 12 && player->HealthBelowPct(35) && player->HasAura(996008))
            damage = uint32(damage * 0.98f);
        if (player->HasAura(300277) && player->HasAura(681326))
            AddPct(damage, Amount(681326));
        if (player->HasAura(Herald) && player->HasAura(300287))
            AddPct(damage, Amount(300287, 1));
    }
};
}  // namespace
void AddSC_AscensionCultistContracts()
{
    new AscensionCultist::cultist_spell_contracts();
    new cultist_scaling();
}
