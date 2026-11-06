// Original Wrath implementations of the requested MoP capstone mechanics.
#include "CapstoneTalents.h"
#include "../../AscensionHookGuard.h"
#include "CapstoneCombatMath.h"
#include "CapstoneElementalBlastScaling.h"
#include "ShadowReflectionTimeline.h"
#include "CellImpl.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "PathGenerator.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Totem.h"
#include "TemporarySummon.h"

#include <set>
#include <chrono>

namespace CapstoneTalents
{
enum ExistingSpells : uint32
{
    SPELL_WARLOCK_INFERNO = 1122,
    SPELL_WARLOCK_RITUAL_OF_DOOM = 18540,
    SPELL_BLOOD_PLAGUE = 55078,
    SPELL_FROST_FEVER = 55095,
    SPELL_DEATH_COIL_DAMAGE = 47632,
    SPELL_FROST_STRIKE_FIRST_RANK = 49143,
    SPELL_RUNE_STRIKE = 56815,
    SPELL_IMMOLATION_AURA = 50589,
    SPELL_IMMOLATION_AURA_DAMAGE = 50590,
    SPELL_DIVINE_AEGIS_FIRST_RANK = 47509,
    SPELL_MOONKIN_FORM = 24858,
    SPELL_ECLIPSE_LUNAR = 48518,
    SPELL_ECLIPSE_SOLAR = 48517,
    SPELL_BRAIN_FREEZE_FIRST_RANK = 44546,
    SPELL_BRAIN_FREEZE_READY = 57761,
    SPELL_EVOCATION = 12051,
    SPELL_GLYPH_OF_EVOCATION = 56380,
    SPELL_WATERBOLT = 31707,
    SPELL_WATERBOLT_PERMANENT = 72898,
    SPELL_CAT_FORM = 768,
    SPELL_SHADOWFIEND = 34433,
    SPELL_GLYPH_OF_FELHUNTER = 56249,
    SPELL_MIRROR_IMAGE_CLONE = 60352,
};

enum ShadowReflectionData : uint32
{
    // Private WotLK creature entry; the MoP source entry 77726 belongs to a
    // different server database and has no template in this world DB.
    NPC_SHADOW_REFLECTION = 990777,
    SHADOW_REFLECTION_TARGET = 1,
};

struct SupremacySummon
{
    uint32 Original;
    uint32 Replacement;
    uint32 CreatureEntry;
};

constexpr std::array<SupremacySummon, 7> SupremacySummons = {{
    {GetSupremacyBaseSummonSpell(NPC_SUPREMACY_FEL_IMP), SPELL_SUPREMACY_FEL_IMP, NPC_SUPREMACY_FEL_IMP},
    {GetSupremacyBaseSummonSpell(NPC_SUPREMACY_VOIDLORD), SPELL_SUPREMACY_VOIDLORD, NPC_SUPREMACY_VOIDLORD},
    {GetSupremacyBaseSummonSpell(NPC_SUPREMACY_SHIVARRA), SPELL_SUPREMACY_SHIVARRA, NPC_SUPREMACY_SHIVARRA},
    {GetSupremacyBaseSummonSpell(NPC_SUPREMACY_OBSERVER), SPELL_SUPREMACY_OBSERVER, NPC_SUPREMACY_OBSERVER},
    {GetSupremacyBaseSummonSpell(NPC_SUPREMACY_WRATHGUARD), SPELL_SUPREMACY_WRATHGUARD, NPC_SUPREMACY_WRATHGUARD},
    {SPELL_WARLOCK_INFERNO, SPELL_SUPREMACY_ABYSSAL, NPC_SUPREMACY_ABYSSAL},
    {SPELL_WARLOCK_RITUAL_OF_DOOM, SPELL_SUPREMACY_TERRORGUARD, NPC_SUPREMACY_TERRORGUARD},
}};

struct npc_capstone_supremacy_abyssal : public ScriptedAI
{
    explicit npc_capstone_supremacy_abyssal(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _ready = me->GetEntry() == NPC_SUPREMACY_ABYSSAL
            && sSpellMgr->GetSpellInfo(SPELL_SUPREMACY_ABYSSAL_IMMOLATION)
            && sSpellMgr->GetSpellInfo(SPELL_SUPREMACY_ABYSSAL_IMMOLATION_DAMAGE);
        if (_ready && !me->HasAura(SPELL_SUPREMACY_ABYSSAL_IMMOLATION))
            DoCastSelf(SPELL_SUPREMACY_ABYSSAL_IMMOLATION, true);
        _ready = _ready && me->HasAura(SPELL_SUPREMACY_ABYSSAL_IMMOLATION);
        if (!_ready)
        {
            LOG_ERROR("spells.capstone", "Abyssal {} requires creature {} and active Immolation {} triggering {}.",
                me->GetEntry(), uint32(NPC_SUPREMACY_ABYSSAL), uint32(SPELL_SUPREMACY_ABYSSAL_IMMOLATION),
                uint32(SPELL_SUPREMACY_ABYSSAL_IMMOLATION_DAMAGE));
            me->DespawnOrUnsummon();
        }
    }

    bool AcquireTarget()
    {
        if (Unit* target = me->SelectNearestTarget(15.0f))
        {
            AttackStart(target);
            return true;
        }
        return false;
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!_ready || (!UpdateVictim() && !AcquireTarget()))
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _ready = false;
};

struct npc_capstone_supremacy_terrorguard : public ScriptedAI
{
    explicit npc_capstone_supremacy_terrorguard(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _ready = me->GetEntry() == NPC_SUPREMACY_TERRORGUARD
            && sSpellMgr->GetSpellInfo(SPELL_SUPREMACY_DOOM_BOLT);
        if (!_ready)
        {
            LOG_ERROR("spells.capstone", "Terrorguard {} requires creature {} and Doom Bolt {}.",
                me->GetEntry(), uint32(NPC_SUPREMACY_TERRORGUARD), uint32(SPELL_SUPREMACY_DOOM_BOLT));
            me->DespawnOrUnsummon();
        }
    }

    void AttackStart(Unit* target) override
    {
        if (!_ready || !target || !me->IsValidAttackTarget(target))
            return;

        SpellInfo const* bolt = sSpellMgr->GetSpellInfo(SPELL_SUPREMACY_DOOM_BOLT);
        if (!bolt)
        {
            _ready = false;
            LOG_ERROR("spells.capstone", "Terrorguard attack requires Doom Bolt {}.",
                uint32(SPELL_SUPREMACY_DOOM_BOLT));
            me->DespawnOrUnsummon();
            return;
        }

        // Native ranged chase maintains distance and paths around line-of-sight obstructions.
        AttackStartCaster(target, bolt->GetMaxRange(false));
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!_ready || !UpdateVictim() || me->IsActionPreventedByCasting())
            return;

        DoCastVictim(SPELL_SUPREMACY_DOOM_BOLT);
    }

private:
    bool _ready = false;
};

class spell_capstone_supremacy_doom_bolt : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_doom_bolt);

    void ExecuteBonus()
    {
        if (Unit* target = GetHitUnit())
            if (target->HealthBelowPct(GetSpellInfo()->Effects[EFFECT_1].CalcValue()))
                SetHitDamage(GetHitDamage() * 1.2f);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_supremacy_doom_bolt::ExecuteBonus);
    }
};

class spell_capstone_supremacy_suffering : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_suffering);

    SpellCastResult CheckTarget()
    {
        Unit* target = GetExplTargetUnit();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;
        if (Creature* creature = target->ToCreature())
            if (creature->GetMap()->IsRaid() && (creature->IsDungeonBoss() || creature->isWorldBoss()))
                return SPELL_FAILED_IMMUNE;
        return SPELL_CAST_OK;
    }

    SpellCastResult CheckAutoCast()
    {
        Unit* target = GetExplTargetUnit();
        return target && target->GetVictim() != GetCaster()
            ? SPELL_CAST_OK : SPELL_FAILED_DONT_REPORT;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_supremacy_suffering::CheckTarget);
        OnCheckAutoCast += SpellCheckCastFn(spell_capstone_supremacy_suffering::CheckAutoCast);
    }
};

class spell_capstone_supremacy_shadow_bulwark : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_shadow_bulwark);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_SUPREMACY_BULWARK_OWNER, SPELL_SUPREMACY_BULWARK_HEAL});
    }

    SpellCastResult CheckAutoCast()
    {
        Unit* caster = GetCaster()->ToUnit();
        return caster && caster->GetHealthPct() < 20.0f ? SPELL_CAST_OK : SPELL_FAILED_DONT_REPORT;
    }

    SpellCastResult CheckOwner()
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* owner = GetCaster()->GetOwner();
        return caster && caster->IsPet() && GetDemonStatEntry(caster->GetEntry()) == NPC_VOIDWALKER
            && owner && owner->IsPlayer() && owner->GetGuardianPet() == caster
            ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
    }

    void GrantHealth()
    {
        if (!GetHitAura())
            return;
        Unit* caster = GetCaster()->ToUnit();
        if (!caster)
            return;
        if (Unit* owner = caster->GetOwner())
        {
            caster->CastSpell(owner, SPELL_SUPREMACY_BULWARK_OWNER, true);
            caster->CastSpell(caster, SPELL_SUPREMACY_BULWARK_HEAL, true);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_supremacy_shadow_bulwark::CheckOwner);
        OnCheckAutoCast += SpellCheckCastFn(spell_capstone_supremacy_shadow_bulwark::CheckAutoCast);
        AfterHit += SpellHitFn(spell_capstone_supremacy_shadow_bulwark::GrantHealth);
    }
};

class spell_capstone_supremacy_bulwark_owner : public AuraScript
{
    PrepareAuraScript(spell_capstone_supremacy_bulwark_owner);

    void UpdatePet(AuraEffect const*, AuraEffectHandleModes)
    {
        if (Guardian* pet = GetTarget()->GetGuardianPet())
            if (GetDemonStatEntry(pet->GetEntry()) == NPC_VOIDWALKER)
                pet->UpdateMaxHealth();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_supremacy_bulwark_owner::UpdatePet,
            EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_supremacy_bulwark_owner::UpdatePet,
            EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_supremacy_void_shield : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_void_shield);

    SpellCastResult CheckAutoCast()
    {
        // Original PetAI casts this defensively when the pet is attacked.
        return SPELL_FAILED_DONT_REPORT;
    }

    void FillLayers()
    {
        if (Aura* aura = GetHitAura())
            aura->SetStackAmount(3);
    }

    void Register() override
    {
        OnCheckAutoCast += SpellCheckCastFn(spell_capstone_supremacy_void_shield::CheckAutoCast);
        AfterHit += SpellHitFn(spell_capstone_supremacy_void_shield::FillLayers);
    }
};

class spell_capstone_supremacy_void_shield_aura : public AuraScript
{
    PrepareAuraScript(spell_capstone_supremacy_void_shield_aura);

    bool _droppingLayer = false;

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_SUPREMACY_VOID_DISCHARGE, SPELL_SUPREMACY_VOID_CHARGE_FIRST,
            SPELL_SUPREMACY_VOID_CHARGE_SECOND, SPELL_SUPREMACY_VOID_CHARGE_THIRD});
    }

    void ClearVisuals(AuraEffect const*, AuraEffectHandleModes)
    {
        for (uint32 spell : {SPELL_SUPREMACY_VOID_CHARGE_FIRST, SPELL_SUPREMACY_VOID_CHARGE_SECOND,
            SPELL_SUPREMACY_VOID_CHARGE_THIRD})
            GetTarget()->RemoveAurasDueToSpell(spell);
    }

    void RefreshVisuals(AuraEffect const* effect, AuraEffectHandleModes mode)
    {
        if (_droppingLayer)
            return;
        ClearVisuals(effect, mode);
        for (uint32 spell : {SPELL_SUPREMACY_VOID_CHARGE_FIRST, SPELL_SUPREMACY_VOID_CHARGE_SECOND,
            SPELL_SUPREMACY_VOID_CHARGE_THIRD})
            GetTarget()->CastSpell(GetTarget(), spell, true);
    }

    void DropLayer(AuraEffect const*, ProcEventInfo&)
    {
        GetAura()->AddProcCooldown(std::chrono::steady_clock::now() + std::chrono::milliseconds(1000));
        uint8 const previous = GetAura()->GetStackAmount();
        _droppingLayer = true;
        GetAura()->ModStackAmount(-1);
        _droppingLayer = false;
        if (previous == 3)
            GetTarget()->RemoveAurasDueToSpell(SPELL_SUPREMACY_VOID_CHARGE_THIRD);
        else if (previous == 2)
            GetTarget()->RemoveAurasDueToSpell(SPELL_SUPREMACY_VOID_CHARGE_SECOND);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_capstone_supremacy_void_shield_aura::RefreshVisuals,
            EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        OnEffectRemove += AuraEffectApplyFn(spell_capstone_supremacy_void_shield_aura::ClearVisuals,
            EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, AURA_EFFECT_HANDLE_REAL);
        AfterEffectProc += AuraEffectProcFn(spell_capstone_supremacy_void_shield_aura::DropLayer,
            EFFECT_1, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

class spell_capstone_supremacy_cauterize_master : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_cauterize_master);

    bool Load() override
    {
        Unit* caster = GetCaster()->ToUnit();
        return caster && caster->IsPet() && caster->GetEntry() == NPC_SUPREMACY_FEL_IMP;
    }

    SpellCastResult CheckOwner()
    {
        Unit* owner = GetCaster()->GetOwner();
        return owner && owner->IsAlive() ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
    }

    void BurnOwner(SpellEffIndex)
    {
        Unit* owner = GetHitUnit();
        if (owner && owner == GetCaster()->GetOwner() && owner->IsAlive())
            SetHitDamage(GetHitDamage() + owner->CountPctFromMaxHealth(GetEffectValue()));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_supremacy_cauterize_master::CheckOwner);
        OnEffectHitTarget += SpellEffectFn(spell_capstone_supremacy_cauterize_master::BurnOwner,
            EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_capstone_supremacy_mortal_cleave : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_mortal_cleave);

    uint32 _targets = 0;

    void CountTargets(std::list<WorldObject*>& targets)
    {
        _targets = uint32(targets.size());
    }

    void DivideDamage()
    {
        if (_targets)
            SetHitDamage(GetHitDamage() / _targets);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_supremacy_mortal_cleave::CountTargets,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnHit += SpellHitFn(spell_capstone_supremacy_mortal_cleave::DivideDamage);
    }
};

class spell_capstone_supremacy_clone_magic : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_clone_magic);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_SUPREMACY_CLONE_HEAL, SPELL_SUPREMACY_CLONE_MANA});
    }

    bool Load() override
    {
        Unit* caster = GetCaster()->ToUnit();
        return caster && caster->IsPet() && caster->GetEntry() == NPC_SUPREMACY_OBSERVER;
    }

    SpellCastResult CheckAutoCast()
    {
        Unit* target = GetExplTargetUnit();
        Unit* caster = GetCaster()->ToUnit();
        if (!target || !target->IsAlive() || !caster || !caster->IsValidAttackTarget(target))
            return SPELL_FAILED_DONT_REPORT;
        for (auto const& entry : target->GetOwnedAuras())
        {
            Aura* aura = entry.second;
            AuraApplication const* application = aura->GetApplicationOfTarget(target->GetGUID());
            if (!application || !application->IsPositive() || aura->IsPassive()
                || aura->GetSpellInfo()->HasAttribute(SPELL_ATTR4_CANNOT_BE_STOLEN)
                || !(aura->GetSpellInfo()->GetDispelMask() & SpellInfo::GetDispelMask(DISPEL_MAGIC)))
                continue;
            uint8 const charges = aura->GetSpellInfo()->HasAttribute(SPELL_ATTR7_DISPEL_REMOVES_CHARGES)
                ? aura->GetCharges() : aura->GetStackAmount();
            if (charges && aura->CalcDispelChance(target, !target->IsFriendlyTo(caster)))
                return SPELL_CAST_OK;
        }
        return SPELL_FAILED_DONT_REPORT;
    }

    void RewardSuccessfulSteal()
    {
        if (!GetDispelCount())
            return;
        Unit* caster = GetCaster()->ToUnit();
        if (!caster)
            return;
        caster->CastSpell(caster, SPELL_SUPREMACY_CLONE_HEAL, true);
        caster->CastSpell(caster, SPELL_SUPREMACY_CLONE_MANA, true);
        if (Unit* owner = caster->GetOwner())
            if (owner->IsAlive() && owner->HasAura(SPELL_GLYPH_OF_FELHUNTER))
                caster->CastSpell(owner, SPELL_SUPREMACY_CLONE_HEAL, true);
    }

    void Register() override
    {
        OnCheckAutoCast += SpellCheckCastFn(spell_capstone_supremacy_clone_magic::CheckAutoCast);
        OnHit += SpellHitFn(spell_capstone_supremacy_clone_magic::RewardSuccessfulSteal);
    }
};

class spell_capstone_supremacy_clone_heal : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_clone_heal);

    void AddOwnerSpellPower(SpellEffIndex)
    {
        if (Unit* owner = GetCaster()->GetOwner())
            SetEffectValue(GetEffectValue() + int32(owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ALL) * 0.15f));
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_capstone_supremacy_clone_heal::AddOwnerSpellPower,
            EFFECT_0, SPELL_EFFECT_HEAL);
    }
};

class spell_capstone_supremacy_sear_magic : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_sear_magic);

    bool _autoCastOwner = false;

    bool HasDispellableMagic(Unit* target)
    {
        if (!target || !target->IsAlive())
            return false;
        DispelChargesList dispels;
        target->GetDispellableAuraList(GetCaster()->ToUnit(), SpellInfo::GetDispelMask(DISPEL_MAGIC),
            dispels, GetSpellInfo());
        return !dispels.empty();
    }

    SpellCastResult CheckAutoCast()
    {
        Unit* owner = GetCaster()->GetOwner();
        if (!owner || !owner->IsAlive() || GetExplTargetUnit() != owner)
            return SPELL_FAILED_DONT_REPORT;
        DispelChargesList dispels;
        owner->GetDispellableAuraList(GetCaster()->ToUnit(), SpellInfo::GetDispelMask(DISPEL_MAGIC),
            dispels, GetSpellInfo());
        for (auto const& entry : dispels)
        {
            Aura const* aura = entry.first;
            if (aura->HasEffectType(SPELL_AURA_MOD_STUN) || aura->HasEffectType(SPELL_AURA_MOD_SILENCE)
                || aura->HasEffectType(SPELL_AURA_MOD_CONFUSE) || aura->HasEffectType(SPELL_AURA_MOD_FEAR))
            {
                _autoCastOwner = true;
                GetSpell()->m_targets.SetDst(*owner);
                return SPELL_CAST_OK;
            }
        }
        return SPELL_FAILED_DONT_REPORT;
    }

    void SelectAllies(std::list<WorldObject*>& targets)
    {
        targets.remove_if([this](WorldObject* object) { return !HasDispellableMagic(object->ToUnit()); });
        Unit* primary = _autoCastOwner ? GetCaster()->GetOwner() : GetExplTargetUnit();
        bool const includesPrimary = primary && std::find(targets.begin(), targets.end(), primary) != targets.end();
        if (includesPrimary)
            targets.remove(primary);
        Acore::Containers::RandomResize(targets, includesPrimary ? 2 : 3);
        if (includesPrimary)
            targets.push_front(primary);
    }

    void Register() override
    {
        OnCheckAutoCast += SpellCheckCastFn(spell_capstone_supremacy_sear_magic::CheckAutoCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_supremacy_sear_magic::SelectAllies,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

class spell_capstone_supremacy_summon : public SpellScript
{
    PrepareSpellScript(spell_capstone_supremacy_summon);

    bool Validate(SpellInfo const* info) override
    {
        for (SupremacySummon const& summon : SupremacySummons)
            if (summon.Replacement == info->Id)
            {
                CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(summon.CreatureEntry);
                if (!creature || !creature->GetFirstValidModel())
                {
                    LOG_ERROR("spells.capstone",
                        "Supremacy summon {} requires original creature {} and its ported model.",
                        info->Id, summon.CreatureEntry);
                    return false;
                }
                if (HasSupremacyDualWield(summon.CreatureEntry)
                    && !ValidateSpellInfo({SPELL_SUPREMACY_DUAL_WIELD}))
                    return false;
                if (summon.CreatureEntry == NPC_SUPREMACY_ABYSSAL
                    && !ValidateSpellInfo({SPELL_SUPREMACY_ABYSSAL_GUARDIAN, SPELL_SUPREMACY_ABYSSAL_AWAKENING,
                        SPELL_SUPREMACY_ABYSSAL_IMMOLATION, SPELL_SUPREMACY_ABYSSAL_IMMOLATION_DAMAGE}))
                    return false;
                if (summon.CreatureEntry == NPC_SUPREMACY_TERRORGUARD
                    && !ValidateSpellInfo({SPELL_SUPREMACY_TERRORGUARD_GUARDIAN, SPELL_SUPREMACY_DOOM_BOLT}))
                    return false;
                return ValidateSpellInfo({summon.Original, SPELL_SUPREMACY_POWER});
            }
        return false;
    }

    SpellCastResult CheckEligibility()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || !player->HasActiveSpell(SPELL_GRIMOIRE_OF_SUPREMACY))
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        for (SupremacySummon const& summon : SupremacySummons)
            if (summon.Replacement == GetSpellInfo()->Id && player->HasActiveSpell(summon.Original))
                return player->HasSpellCooldown(summon.Original) ? SPELL_FAILED_NOT_READY : SPELL_CAST_OK;
        return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
    }

    void RememberGuardians()
    {
        if (GetSpellInfo()->Id != SPELL_SUPREMACY_TERRORGUARD)
            return;

        _existingGuardians.clear();
        std::list<Creature*> guardians;
        GetCaster()->ToUnit()->GetAllMinionsByEntry(guardians, NPC_SUPREMACY_TERRORGUARD);
        for (Creature* guardian : guardians)
            _existingGuardians.insert(guardian->GetGUID());
    }

    void BeginGuardianAttack()
    {
        if (GetSpellInfo()->Id != SPELL_SUPREMACY_TERRORGUARD)
            return;

        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        // Preserve the spent guardian cooldown when its temporary replacement is removed.
        player->AddSpellCooldown(SPELL_WARLOCK_RITUAL_OF_DOOM, 0, GetSpellInfo()->CategoryRecoveryTime, true);
        std::list<Creature*> guardians;
        player->GetAllMinionsByEntry(guardians, NPC_SUPREMACY_TERRORGUARD);
        Unit* target = GetExplTargetUnit();
        for (Creature* guardian : guardians)
            if (!_existingGuardians.count(guardian->GetGUID()) && guardian->AI() && target)
            {
                guardian->AI()->AttackStart(target);
                return;
            }

        LOG_ERROR("spells.capstone", "Terrorguard summon {} did not produce creature {} with a valid target and AI.",
            uint32(SPELL_SUPREMACY_TERRORGUARD), uint32(NPC_SUPREMACY_TERRORGUARD));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_supremacy_summon::CheckEligibility);
        BeforeCast += SpellCastFn(spell_capstone_supremacy_summon::RememberGuardians);
        AfterCast += SpellCastFn(spell_capstone_supremacy_summon::BeginGuardianAttack);
    }

private:
    std::set<ObjectGuid> _existingGuardians;
};

class capstone_supremacy_player : public PlayerScript
{
public:
    capstone_supremacy_player() : PlayerScript("capstone_supremacy_player",
        {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_ON_FORGOT_SPELL,
         PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED, PLAYERHOOK_ON_TALENTS_RESET}) { }

    static void SyncReplacements(Player* player, bool removingTalent = false)
    {
        // A talent saved without its spell (auto-assigned or restored builds) would otherwise never enable the redirect.
        if (!removingTalent && !player->HasActiveSpell(SPELL_GRIMOIRE_OF_SUPREMACY)
            && player->HasTalent(SPELL_GRIMOIRE_OF_SUPREMACY, player->GetActiveSpec()))
            player->learnSpell(SPELL_GRIMOIRE_OF_SUPREMACY, true);
        bool const eligible = !removingTalent && player->HasActiveSpell(SPELL_GRIMOIRE_OF_SUPREMACY);
        for (SupremacySummon const& summon : SupremacySummons)
        {
            bool const available = eligible && player->HasActiveSpell(summon.Original);
            if (available)
            {
                if (!player->HasActiveSpell(summon.Replacement))
                    // Added silently: the client learns of the stand-in only through the supersede packet, which swaps it
                    // into the original's slot. A separate learn packet would list it a second time.
                    player->_addSpell(summon.Replacement, SPEC_MASK_ALL, true, false, false);
                player->SetTemporarySpellReplacement(summon.Original, summon.Replacement);
            }
            else
            {
                player->SetTemporarySpellReplacement(summon.Original, 0);
                player->removeSpell(summon.Replacement, SPEC_MASK_ALL, true, false);
                if (summon.CreatureEntry == NPC_SUPREMACY_ABYSSAL
                    || summon.CreatureEntry == NPC_SUPREMACY_TERRORGUARD)
                    player->RemoveAllMinionsByEntry(summon.CreatureEntry);
            }
            if (Pet* pet = player->GetPet())
                if (pet->GetEntry() == summon.CreatureEntry)
                {
                    if (!available)
                        player->RemovePet(pet, PET_SAVE_AS_DELETED, false);
                    else
                        pet->ApplySupremacyCombatPassives();
                }
        }
    }

    void OnPlayerLogin(Player* player) override
    {
        ASCENSION_GUARD("capstone::OnPlayerLogin(replacements)", [&] { SyncReplacements(player); });
    }

    void OnPlayerLearnSpell(Player* player, uint32 spell) override
    {
        if (spell == SPELL_GRIMOIRE_OF_SUPREMACY)
            SyncReplacements(player);
        else
            for (SupremacySummon const& summon : SupremacySummons)
                if (spell == summon.Original)
                {
                    SyncReplacements(player);
                    break;
                }
    }

    void OnPlayerForgotSpell(Player* player, uint32 spell) override
    {
        if (spell == SPELL_GRIMOIRE_OF_SUPREMACY)
            SyncReplacements(player, true);
        else
            for (SupremacySummon const& summon : SupremacySummons)
                if (spell == summon.Original)
                {
                    SyncReplacements(player);
                    break;
                }
    }

    void OnPlayerAfterSpecSlotChanged(Player* player, uint8) override { SyncReplacements(player); }
    void OnPlayerTalentsReset(Player* player, bool) override { SyncReplacements(player); }
};

static constexpr uint32 CONTROLLED_GUARDIAN_SUMMON_PROPERTIES = 1561;

class spell_capstone_elemental_blast : public SpellScript
{
    PrepareSpellScript(spell_capstone_elemental_blast);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_ELEMENTAL_BLAST_CRIT, SPELL_ELEMENTAL_BLAST_HASTE,
            SPELL_ELEMENTAL_BLAST_ELEMENTAL_POWER, SPELL_ELEMENTAL_BLAST_AGILITY,
            SPELL_ELEMENTAL_BLAST_FROST_VISUAL, SPELL_ELEMENTAL_BLAST_NATURE_VISUAL});
    }

    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    void LaunchVisuals()
    {
        if (Unit* target = GetExplTargetUnit())
        {
            GetCaster()->CastSpell(target, SPELL_ELEMENTAL_BLAST_FROST_VISUAL, true);
            GetCaster()->CastSpell(target, SPELL_ELEMENTAL_BLAST_NATURE_VISUAL, true);
        }
    }

    void GrantRandomStat()
    {
        if (GetSpell()->IsTriggered())
            return;

        constexpr std::array<uint32, 4> bonuses = {SPELL_ELEMENTAL_BLAST_CRIT, SPELL_ELEMENTAL_BLAST_HASTE,
            SPELL_ELEMENTAL_BLAST_ELEMENTAL_POWER, SPELL_ELEMENTAL_BLAST_AGILITY};
        Unit* caster = GetCaster()->ToUnit();
        for (uint32 spell : bonuses)
            caster->RemoveAurasDueToSpell(spell);
        caster->CastSpell(caster, bonuses[urand(0, 3)], true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_elemental_blast::LaunchVisuals);
        AfterHit += SpellHitFn(spell_capstone_elemental_blast::GrantRandomStat);
    }
};

class spell_capstone_elemental_blast_stat : public AuraScript
{
    PrepareAuraScript(spell_capstone_elemental_blast_stat);

    void CalculateAmount(AuraEffect const*, int32& amount, bool& recalculate)
    {
        // A level-0 unit would index ElementalBlastScalingByLevel[-1] (unsigned wrap); clamp to 1..80.
        uint32 level = std::max<uint32>(1, std::min<uint32>(GetUnitOwner()->GetLevel(), 80));
        ElementalBlastScaling const& scaling = ElementalBlastScalingByLevel[level - 1];
        amount = GetId() == SPELL_ELEMENTAL_BLAST_ELEMENTAL_POWER
            ? int32(scaling.ElementalPercentUnits) : int32(scaling.Rating);
        recalculate = false;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_capstone_elemental_blast_stat::CalculateAmount,
            EFFECT_0, SPELL_AURA_ANY);
    }
};

class spell_capstone_primal_summon : public SpellScript
{
    PrepareSpellScript(spell_capstone_primal_summon);

    bool Validate(SpellInfo const*) override
    {
        for (uint32 entry : {uint32(NPC_PRIMAL_FIRE_ELEMENTAL), uint32(NPC_PRIMAL_EARTH_ELEMENTAL)})
        {
            CreatureTemplate const* summon = sObjectMgr->GetCreatureTemplate(entry);
            if (!summon || !summon->GetFirstValidModel())
            {
                LOG_ERROR("spells.capstone", "Primal Elementalist requires original creature {} and its ported model.", entry);
                return false;
            }
        }
        return ValidateSpellInfo({SPELL_PRIMAL_POWER});
    }

    bool Load() override
    {
        Unit* caster = GetCaster()->ToUnit();
        return caster && caster->IsTotem() && caster->GetOwner()
            && caster->GetOwner()->HasAura(SPELL_PRIMAL_ELEMENTALIST);
    }

    void ReplaceSummon(SpellEffIndex effect)
    {
        PreventHitDefaultEffect(effect);
        Totem* totem = GetCaster()->ToTotem();
        Unit* owner = totem->GetOwner();
        SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(CONTROLLED_GUARDIAN_SUMMON_PROPERTIES);
        if (!owner || !properties)
            return;

        uint32 entry = GetSpellInfo()->Effects[effect].MiscValue == NPC_FIRE_ELEMENTAL
            ? uint32(NPC_PRIMAL_FIRE_ELEMENTAL) : uint32(NPC_PRIMAL_EARTH_ELEMENTAL);
        TempSummon* creature = totem->GetMap()->SummonCreature(entry, *totem, properties,
            totem->GetTotemDuration(), owner, GetSpellInfo()->Id);
        if (!creature)
        {
            LOG_ERROR("spells.capstone", "Primal Elementalist failed to summon creature {}.", entry);
            return;
        }

        totem->SetDependentGuardianGUID(creature->GetGUID());
        GetSpell()->ExecuteLogEffectSummonObject(effect, creature);
        creature->SetReactState(REACT_DEFENSIVE);
        creature->CastSpell(creature, SPELL_PRIMAL_POWER, true);
        if (Player* player = owner->ToPlayer())
        {
            if (Unit* target = player->GetSelectedUnit())
                if (target->IsAlive() && owner->IsValidAttackTarget(target))
                    creature->AI()->AttackStart(target);
            player->CharmSpellInitialize();
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_capstone_primal_summon::ReplaceSummon, EFFECT_2, SPELL_EFFECT_SUMMON);
    }
};

class spell_capstone_primal_elementalist : public AuraScript
{
    PrepareAuraScript(spell_capstone_primal_elementalist);

    void RemovePrimalGuardians(AuraEffect const*, AuraEffectHandleModes)
    {
        GetTarget()->RemoveAllMinionsByEntry(NPC_PRIMAL_FIRE_ELEMENTAL);
        GetTarget()->RemoveAllMinionsByEntry(NPC_PRIMAL_EARTH_ELEMENTAL);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_primal_elementalist::RemovePrimalGuardians,
            EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_primal_immolate : public AuraScript
{
    PrepareAuraScript(spell_capstone_primal_immolate);

    void SnapshotCriticalChance(AuraEffect const*, AuraEffectHandleModes)
    {
        if (Unit* caster = GetCaster())
        {
            float crit = caster->SpellDoneCritChance(nullptr, GetSpellInfo(), GetSpellInfo()->GetSchoolMask(), BASE_ATTACK, true);
            crit = GetTarget()->SpellTakenCritChance(caster, GetSpellInfo(), GetSpellInfo()->GetSchoolMask(), crit, BASE_ATTACK, true);
            GetAura()->GetEffect(EFFECT_0)->SetCritChance(crit);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_primal_immolate::SnapshotCriticalChance,
            EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

class spell_capstone_mindbender : public SpellScript
{
    PrepareSpellScript(spell_capstone_mindbender);

    bool Validate(SpellInfo const*) override
    {
        CreatureTemplate const* summon = sObjectMgr->GetCreatureTemplate(NPC_MINDBENDER);
        if (!summon || !summon->GetFirstValidModel())
        {
            LOG_ERROR("spells.capstone", "Mindbender requires original creature {} and its ported model.",
                uint32(NPC_MINDBENDER));
            return false;
        }
        return ValidateSpellInfo({SPELL_MINDBENDER_MANA_LEECH, SPELL_MINDBENDER_MANA_RETURN});
    }

    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    SpellCastResult CheckTarget()
    {
        Unit* target = GetExplTargetUnit();
        Unit* caster = GetCaster()->ToUnit();
        return target && caster && target->IsAlive() && caster->IsValidAttackTarget(target)
            ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
    }

    void BeginAttack()
    {
        Unit* owner = GetCaster()->ToUnit();
        Guardian* summon = owner->GetGuardianPet();
        Unit* target = GetHitUnit();
        if (!summon || summon->GetEntry() != NPC_MINDBENDER || !target || !target->IsAlive())
            return;

        summon->SetReactState(REACT_DEFENSIVE);
        summon->CastSpell(summon, SPELL_MINDBENDER_MANA_LEECH, true);
        summon->AI()->AttackStart(target);
        if (Player* player = owner->ToPlayer())
            player->CharmSpellInitialize();
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_mindbender::CheckTarget);
        AfterHit += SpellHitFn(spell_capstone_mindbender::BeginAttack);
    }
};

class spell_capstone_mindbender_mana : public AuraScript
{
    PrepareAuraScript(spell_capstone_mindbender_mana);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_MINDBENDER_MANA_RETURN});
    }

    bool CheckProc(ProcEventInfo& event)
    {
        Unit* summon = GetTarget();
        Unit* owner = summon->GetOwner();
        return summon->GetEntry() == NPC_MINDBENDER && owner && owner->IsAlive() && owner->IsPlayer()
            && owner->getPowerType() == POWER_MANA && event.GetActor() == summon
            && (event.GetTypeMask() & PROC_FLAG_DONE_MELEE_AUTO_ATTACK)
            && (event.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB));
    }

    void ReturnMana(AuraEffect const* effect, ProcEventInfo&)
    {
        PreventDefaultAction();
        Unit* summon = GetTarget();
        if (Unit* owner = summon->GetOwner())
        {
            int32 amount = int32(uint64(owner->GetMaxPower(POWER_MANA)) * 175 / 10000);
            summon->CastCustomSpell(owner, SPELL_MINDBENDER_MANA_RETURN, &amount, nullptr, nullptr, true,
                nullptr, effect);
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_capstone_mindbender_mana::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_capstone_mindbender_mana::ReturnMana, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

class capstone_mindbender_player : public PlayerScript
{
public:
    capstone_mindbender_player() : PlayerScript("capstone_mindbender_player",
        {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_ON_FORGOT_SPELL,
         PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED, PLAYERHOOK_ON_TALENTS_RESET}) { }

    static void SyncReplacement(Player* player)
    {
        player->SetTemporarySpellReplacement(SPELL_SHADOWFIEND,
            player->HasActiveSpell(SPELL_MINDBENDER) ? uint32(SPELL_MINDBENDER) : 0);
    }

    static void ClearSummon(Player* player)
    {
        if (Guardian* summon = player->GetGuardianPet())
            if (summon->GetEntry() == NPC_MINDBENDER)
                summon->UnSummon();
    }

    void OnPlayerLogin(Player* player) override
    {
        ASCENSION_GUARD("capstone::OnPlayerLogin(replacement)", [&] { SyncReplacement(player); });
    }

    void OnPlayerLearnSpell(Player* player, uint32 spell) override
    {
        if (spell == SPELL_MINDBENDER || spell == SPELL_SHADOWFIEND)
            SyncReplacement(player);
    }

    void OnPlayerForgotSpell(Player* player, uint32 spell) override
    {
        if (spell == SPELL_MINDBENDER)
        {
            ClearSummon(player);
            player->SetTemporarySpellReplacement(SPELL_SHADOWFIEND, 0);
        }
    }

    void OnPlayerAfterSpecSlotChanged(Player* player, uint8) override
    {
        ClearSummon(player);
        SyncReplacement(player);
    }

    void OnPlayerTalentsReset(Player* player, bool) override
    {
        ClearSummon(player);
        SyncReplacement(player);
    }
};

class spell_capstone_king_of_the_jungle : public SpellScript
{
    PrepareSpellScript(spell_capstone_king_of_the_jungle);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_CAT_FORM, SPELL_KING_OF_THE_JUNGLE_SANCTUARY});
    }

    void EnterCat()
    {
        Unit* caster = GetCaster()->ToUnit();
        if (caster && caster->GetShapeshiftForm() != FORM_CAT)
            caster->CastSpell(caster, SPELL_CAT_FORM, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_king_of_the_jungle::EnterCat);
    }
};

class spell_capstone_king_of_the_jungle_prowl : public SpellScript
{
    PrepareSpellScript(spell_capstone_king_of_the_jungle_prowl);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_KING_OF_THE_JUNGLE_SANCTUARY});
    }

    void ApplySanctuary()
    {
        Unit* caster = GetCaster()->ToUnit();
        if (caster && caster->GetShapeshiftForm() == FORM_CAT && caster->HasAura(SPELL_INCARNATION_KING_OF_THE_JUNGLE))
            caster->CastSpell(caster, SPELL_KING_OF_THE_JUNGLE_SANCTUARY, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_king_of_the_jungle_prowl::ApplySanctuary);
    }
};

class spell_capstone_execution_sentence : public SpellScript
{
    PrepareSpellScript(spell_capstone_execution_sentence);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_EXECUTION_SENTENCE_DAMAGE, SPELL_STAY_OF_EXECUTION});
    }

    SpellCastResult CheckSentenceTarget()
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetExplTargetUnit();
        return caster && target && target->IsAlive()
            && (caster->IsValidAssistTarget(target) || caster->IsValidAttackTarget(target))
            ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
    }

    void ApplySentence(SpellEffIndex)
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetHitUnit();
        if (caster && target)
        {
            TriggerCastFlags flags = TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_NO_PERIODIC_RESET);
            caster->CastSpell(target, caster->IsValidAssistTarget(target)
                ? SPELL_STAY_OF_EXECUTION : SPELL_EXECUTION_SENTENCE_DAMAGE, flags);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_execution_sentence::CheckSentenceTarget);
        OnEffectHitTarget += SpellEffectFn(spell_capstone_execution_sentence::ApplySentence, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class capstone_execution_sentence_aura_base : public AuraScript
{
protected:
    int32 _base = 0;
    bool _burstDone = false;

    void Snapshot(AuraEffect const*, AuraEffectHandleModes)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;
        AuraEffect* periodic = GetAura()->GetEffect(EFFECT_0);
        _base = periodic->CalculateAmount(caster);
        _burstDone = false;
        periodic->SetAmount(_base);
        periodic->ResetPeriodic(true);
        float crit = caster->SpellDoneCritChance(nullptr, GetSpellInfo(), GetSpellInfo()->GetSchoolMask(), BASE_ATTACK, true);
        crit = GetTarget()->SpellTakenCritChance(caster, GetSpellInfo(), GetSpellInfo()->GetSchoolMask(), crit, BASE_ATTACK, true);
        periodic->SetCritChance(crit);
    }

    void Ramp(AuraEffect* effect)
    {
        effect->SetAmount(ExecutionSentenceAmount(_base, effect->GetTickNumber()));
        if (effect->GetTickNumber() >= 10)
            _burstDone = true;
    }

    void DispelBurst(AuraEffect const*, AuraEffectHandleModes)
    {
        AuraApplication const* application = GetTargetApplication();
        if (_burstDone || application->GetRemoveMode() != AURA_REMOVE_BY_ENEMY_SPELL || !GetTarget()->IsAlive())
            return;
        Unit* caster = GetCaster();
        if (!caster)
            return;
        _burstDone = true;
        AuraEffect* effect = GetAura()->GetEffect(EFFECT_0);
        effect->SetAmount(ExecutionSentenceAmount(_base, 10));
        effect->PeriodicTick(const_cast<AuraApplication*>(application), caster);
    }

};

class spell_capstone_execution_sentence_damage_aura : public capstone_execution_sentence_aura_base
{
    PrepareAuraScript(spell_capstone_execution_sentence_damage_aura);

    bool Validate(SpellInfo const* info) override
    {
        return info->Id == SPELL_EXECUTION_SENTENCE_DAMAGE
            && info->Effects[EFFECT_0].ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE;
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_execution_sentence_damage_aura::Snapshot,
            EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_capstone_execution_sentence_damage_aura::Ramp,
            EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_execution_sentence_damage_aura::DispelBurst,
            EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_execution_sentence_heal_aura : public capstone_execution_sentence_aura_base
{
    PrepareAuraScript(spell_capstone_execution_sentence_heal_aura);

    bool Validate(SpellInfo const* info) override
    {
        return info->Id == SPELL_STAY_OF_EXECUTION
            && info->Effects[EFFECT_0].ApplyAuraName == SPELL_AURA_PERIODIC_HEAL;
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_execution_sentence_heal_aura::Snapshot,
            EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_capstone_execution_sentence_heal_aura::Ramp,
            EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_execution_sentence_heal_aura::DispelBurst,
            EFFECT_0, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
    }
};

static bool IsInOwnPowerRune(Unit* caster, uint32 excluded = 0)
{
    if (!caster || !caster->IsAlive())
        return false;
    for (uint32 id : {SPELL_RUNE_OF_POWER_FIRST, SPELL_RUNE_OF_POWER_SECOND})
        if (id != excluded)
            if (DynamicObject* rune = caster->GetDynObject(id))
                if (rune->GetDuration() > 0 && caster->GetExactDist(rune) <= 5.0f)
                    return true;
    return false;
}

class spell_capstone_rune_of_power : public SpellScript
{
    PrepareSpellScript(spell_capstone_rune_of_power);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_RUNE_OF_POWER_FIRST, SPELL_RUNE_OF_POWER_SECOND, SPELL_RUNE_OF_POWER_BUFF});
    }

    void PlaceRune()
    {
        Unit* caster = GetCaster()->ToUnit();
        WorldLocation const* destination = GetExplTargetDest();
        if (!caster || !destination)
            return;

        DynamicObject* first = caster->GetDynObject(SPELL_RUNE_OF_POWER_FIRST);
        DynamicObject* second = caster->GetDynObject(SPELL_RUNE_OF_POWER_SECOND);
        uint32 slot = !first ? SPELL_RUNE_OF_POWER_FIRST : !second ? SPELL_RUNE_OF_POWER_SECOND
            : first->GetDuration() <= second->GetDuration() ? SPELL_RUNE_OF_POWER_FIRST : SPELL_RUNE_OF_POWER_SECOND;
        caster->RemoveDynObject(slot);
        caster->CastSpell(destination->GetPositionX(), destination->GetPositionY(), destination->GetPositionZ(), slot, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_rune_of_power::PlaceRune);
    }
};

class spell_capstone_rune_of_power_ground : public AuraScript
{
    PrepareAuraScript(spell_capstone_rune_of_power_ground);

    bool Load() override
    {
        return GetAura()->GetType() == DYNOBJ_AURA_TYPE;
    }

    bool CheckOwner(Unit* target)
    {
        Unit* caster = GetCaster();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        return target == caster && player && player->IsAlive() && player->HasActiveSpell(SPELL_RUNE_OF_POWER)
            && target->GetExactDist(GetDynobjOwner()) <= 5.0f;
    }

    void Enter(AuraEffect const*, AuraEffectHandleModes)
    {
        Unit* caster = GetTarget();
        if (!caster->HasAura(SPELL_RUNE_OF_POWER_BUFF))
            caster->CastSpell(caster, SPELL_RUNE_OF_POWER_BUFF, true);
    }

    void Leave(AuraEffect const*, AuraEffectHandleModes)
    {
        if (!IsInOwnPowerRune(GetTarget(), GetId()))
            GetTarget()->RemoveAurasDueToSpell(SPELL_RUNE_OF_POWER_BUFF);
    }

    void Maintain(AuraEffect const*)
    {
        if (CheckOwner(GetTarget()))
            Enter(nullptr, AURA_EFFECT_HANDLE_REAL);
        else if (!IsInOwnPowerRune(GetTarget()))
            GetTarget()->RemoveAurasDueToSpell(SPELL_RUNE_OF_POWER_BUFF);
    }

    void Register() override
    {
        DoCheckAreaTarget += AuraCheckAreaTargetFn(spell_capstone_rune_of_power_ground::CheckOwner);
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_rune_of_power_ground::Enter,
            EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_rune_of_power_ground::Leave,
            EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_capstone_rune_of_power_ground::Maintain,
            EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_capstone_rune_of_power_buff : public AuraScript
{
    PrepareAuraScript(spell_capstone_rune_of_power_buff);

    void RecalculateMana(AuraEffect const*, AuraEffectHandleModes)
    {
        if (Player* player = GetTarget()->ToPlayer())
            player->UpdateManaRegen();
    }

    void GlyphAmount(AuraEffect const*, int32& amount, bool&)
    {
        amount = GetUnitOwner()->HasAura(SPELL_GLYPH_OF_EVOCATION) ? 1 : 0;
    }

    void UpdateGlyph(AuraEffect* effect)
    {
        int32 amount = GetUnitOwner()->HasAura(SPELL_GLYPH_OF_EVOCATION) ? 1 : 0;
        if (effect->GetAmount() != amount)
            effect->ChangeAmount(amount);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_rune_of_power_buff::RecalculateMana,
            EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_rune_of_power_buff::RecalculateMana,
            EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_capstone_rune_of_power_buff::GlyphAmount,
            EFFECT_2, SPELL_AURA_OBS_MOD_HEALTH);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_capstone_rune_of_power_buff::UpdateGlyph,
            EFFECT_2, SPELL_AURA_OBS_MOD_HEALTH);
    }
};

class capstone_rune_player : public PlayerScript
{
public:
    capstone_rune_player() : PlayerScript("capstone_rune_player",
        {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEARN_SPELL, PLAYERHOOK_ON_FORGOT_SPELL,
         PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED, PLAYERHOOK_ON_TALENTS_RESET}) { }

    static void SyncReplacement(Player* player)
    {
        player->SetTemporarySpellReplacement(SPELL_EVOCATION,
            player->HasActiveSpell(SPELL_RUNE_OF_POWER) ? uint32(SPELL_RUNE_OF_POWER) : 0);
    }

    static void ClearRunes(Player* player)
    {
        player->RemoveDynObject(SPELL_RUNE_OF_POWER_FIRST);
        player->RemoveDynObject(SPELL_RUNE_OF_POWER_SECOND);
        player->RemoveAurasDueToSpell(SPELL_RUNE_OF_POWER_BUFF);
    }

    void OnPlayerLogin(Player* player) override
    {
        ASCENSION_GUARD("capstone::OnPlayerLogin(replacement)", [&] { SyncReplacement(player); });
    }

    void OnPlayerLearnSpell(Player* player, uint32 spell) override
    {
        if (spell == SPELL_RUNE_OF_POWER || spell == SPELL_EVOCATION)
            SyncReplacement(player);
    }

    void OnPlayerForgotSpell(Player* player, uint32 spell) override
    {
        if (spell == SPELL_RUNE_OF_POWER)
        {
            ClearRunes(player);
            player->SetTemporarySpellReplacement(SPELL_EVOCATION, 0);
        }
    }

    void OnPlayerAfterSpecSlotChanged(Player* player, uint8) override
    {
        ClearRunes(player);
        SyncReplacement(player);
    }

    void OnPlayerTalentsReset(Player* player, bool) override
    {
        ClearRunes(player);
        SyncReplacement(player);
    }
};

class capstone_rune_pet_damage : public UnitScript
{
public:
    capstone_rune_pet_damage() : UnitScript("capstone_rune_pet_damage", true,
        {UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN}) { }

    void ModifySpellDamageTaken(Unit*, Unit* caster, int32& damage, SpellInfo const* info) override
    {
        if (caster && info && damage > 0 && (info->Id == SPELL_WATERBOLT || info->Id == SPELL_WATERBOLT_PERMANENT))
            if (Unit* owner = caster->GetOwner())
                if (owner->HasAura(SPELL_RUNE_OF_POWER_BUFF))
                    damage = AddHealingDebt(damage, uint32(std::uint64_t(damage) * 15 / 100));
    }
};

class HaloTargetCheck
{
public:
    HaloTargetCheck(Player* caster, Position const& center, float previousRadius, float radius)
        : _caster(caster), _center(center), _previousRadius(previousRadius), _radius(radius) { }

    bool operator()(Unit* target) const
    {
        return target->IsAlive() && HaloWaveCrosses(_center.GetExactDist(target), target->GetCombatReach(),
            _previousRadius, _radius) && (_caster->IsValidAssistTarget(target) || _caster->IsValidAttackTarget(target))
            && target->IsWithinLOS(_center.GetPositionX(), _center.GetPositionY(), _center.GetPositionZ());
    }

private:
    Player* _caster;
    Position const& _center;
    float _previousRadius;
    float _radius;
};

class HaloWaveEvent : public BasicEvent
{
public:
    HaloWaveEvent(Player* caster, uint64 start)
        : _caster(caster->GetGUID()), _center(caster->GetPosition()), _map(caster->GetMapId()),
          _instance(caster->GetInstanceId()), _phase(caster->GetPhaseMask()), _start(start) { }

    bool Execute(uint64 now, uint32) override
    {
        Player* caster = ObjectAccessor::FindConnectedPlayer(_caster);
        if (!caster || !caster->IsInWorld() || caster->GetMapId() != _map
            || caster->GetInstanceId() != _instance || caster->GetPhaseMask() != _phase)
            return true;

        float radius = std::min(30.0f, float(now - _start) * 14.0f / IN_MILLISECONDS);
        std::list<Unit*> targets;
        HaloTargetCheck check(caster, _center, _previousRadius, radius);
        Acore::UnitListSearcher<HaloTargetCheck> searcher(caster, targets, check);
        Cell::VisitObjects(_center.GetPositionX(), _center.GetPositionY(), caster->GetMap(), searcher,
            caster->GetMap()->GetVisibilityRange());
        if (check(caster) && std::find(targets.begin(), targets.end(), caster) == targets.end())
            targets.push_back(caster);

        for (Unit* target : targets)
        {
            if (!_hit.insert(target->GetGUID()).second)
                continue;

            int32 multiplier = int32(std::round(HaloMultiplier(_center.GetExactDist(target)) * 10000));
            TriggerCastFlags flags = TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_DISALLOW_PROC_EVENTS);
            CastSpellExtraArgs args(flags);
            args.AddSpellMod(SPELLVALUE_BASE_POINT1, multiplier);
            caster->CastSpell(target, caster->IsValidAssistTarget(target) ? SPELL_HALO_HEAL : SPELL_HALO_DAMAGE, args);
        }

        _previousRadius = radius;
        if (radius >= 30.0f)
            return true;

        caster->m_Events.AddEvent(this, now + 50, false);
        return false;
    }

private:
    ObjectGuid _caster;
    Position _center;
    uint32 _map;
    uint32 _instance;
    uint32 _phase;
    uint64 _start;
    float _previousRadius = 0.0f;
    std::set<ObjectGuid> _hit;
};

class spell_capstone_halo : public SpellScript
{
    PrepareSpellScript(spell_capstone_halo);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_HALO_HEAL, SPELL_HALO_DAMAGE});
    }

    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    void ReleaseWave()
    {
        Player* caster = GetCaster()->ToPlayer();
        caster->m_Events.AddEvent(new HaloWaveEvent(caster, caster->m_Events.CalculateTime(0)),
            caster->m_Events.CalculateTime(1));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_halo::ReleaseWave);
    }
};

class spell_capstone_halo_impact : public SpellScript
{
    PrepareSpellScript(spell_capstone_halo_impact);

    void ScaleImpact()
    {
        int64 multiplier = int64(GetSpellValue()->EffectBasePoints[EFFECT_1]) + 1;
        SetHitHeal(int32(int64(GetHitHeal()) * multiplier / 10000));
        SetHitDamage(int32(int64(GetHitDamage()) * multiplier / 10000));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_halo_impact::ScaleImpact);
    }
};

static void PulseBindingShot(Unit* caster)
{
    if (DynamicObject* arrow = caster->GetDynObject(SPELL_BINDING_SHOT))
        if (caster->IsAlive() && arrow->GetDuration() > 0)
            caster->CastSpell(arrow->GetPositionX(), arrow->GetPositionY(), arrow->GetPositionZ(),
                SPELL_BINDING_SHOT_TETHER, true);
}

class spell_capstone_binding_shot_cast : public SpellScript
{
    PrepareSpellScript(spell_capstone_binding_shot_cast);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_BINDING_SHOT_TETHER, SPELL_BINDING_SHOT_STUN, SPELL_BINDING_SHOT_IMMUNITY});
    }

    void BindInitialTargets()
    {
        PulseBindingShot(GetCaster());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_binding_shot_cast::BindInitialTargets);
    }
};

class spell_capstone_binding_shot_zone : public AuraScript
{
    PrepareAuraScript(spell_capstone_binding_shot_zone);

    bool Load() override
    {
        return GetAura()->GetType() == UNIT_AURA_TYPE;
    }

    void Pulse(AuraEffect const*)
    {
        PulseBindingShot(GetTarget());
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_capstone_binding_shot_zone::Pulse,
            EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_capstone_binding_shot_targets : public SpellScript
{
    PrepareSpellScript(spell_capstone_binding_shot_targets);

    void Filter(std::list<WorldObject*>& targets)
    {
        DynamicObject* arrow = GetCaster()->GetDynObject(SPELL_BINDING_SHOT);
        targets.remove_if([arrow](WorldObject* object)
        {
            Unit* target = object->ToUnit();
            return !arrow || !target || !target->IsAlive() || target->HasAura(SPELL_BINDING_SHOT_IMMUNITY)
                || target->GetExactDist2d(arrow) > 5.0f;
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_binding_shot_targets::Filter,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

class spell_capstone_binding_shot_tether : public AuraScript
{
    PrepareAuraScript(spell_capstone_binding_shot_tether);

    ObjectGuid _arrow;

    bool Load() override
    {
        Unit* caster = GetCaster();
        DynamicObject* arrow = caster ? caster->GetDynObject(SPELL_BINDING_SHOT) : nullptr;
        if (!arrow)
            return false;
        _arrow = arrow->GetGUID();
        return true;
    }

    void CheckTether(AuraEffect const*)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        DynamicObject* arrow = caster ? caster->GetDynObject(SPELL_BINDING_SHOT) : nullptr;
        if (!arrow || arrow->GetGUID() != _arrow || arrow->GetDuration() <= 0 || !caster->IsAlive()
            || !target->IsAlive() || !caster->IsValidAttackTarget(target))
            return;

        if (target->GetExactDist2d(arrow) > 5.0f && !target->HasAura(SPELL_BINDING_SHOT_IMMUNITY))
        {
            caster->CastSpell(target, SPELL_BINDING_SHOT_IMMUNITY, true);
            caster->CastSpell(target, SPELL_BINDING_SHOT_STUN, true);
            Remove();
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_capstone_binding_shot_tether::CheckTether,
            EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

struct LynxChargeState
{
    bool Cancelled = false;
    bool Finished = false;
};

class LynxArrivalEvent : public BasicEvent
{
public:
    LynxArrivalEvent(ObjectGuid owner, ObjectGuid pet, ObjectGuid target,
        std::shared_ptr<LynxChargeState> state, uint64 deadline)
        : _owner(owner), _pet(pet), _target(target), _state(std::move(state)), _deadline(deadline) { }

    bool Execute(uint64 now, uint32) override
    {
        Player* owner = ObjectAccessor::FindConnectedPlayer(_owner);
        Pet* pet = owner ? owner->GetPet() : nullptr;
        if (_state->Cancelled || !pet || pet->GetGUID() != _pet || !pet->IsAlive()
            || pet->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_CONTROLLED | UNIT_STATE_IN_FLIGHT | UNIT_STATE_POSSESSED))
            return Finish();

        Unit* target = ObjectAccessor::GetUnit(*pet, _target);
        if (!target || !target->IsAlive() || !pet->IsValidAttackTarget(target))
            return Finish();

        if (pet->IsWithinMeleeRange(target) && pet->IsWithinLOSInMap(target))
        {
            pet->CastSpell(target, SPELL_LYNX_RUSH_BLEED, true);
            return Finish();
        }
        if (now >= _deadline || pet->movespline->Finalized())
            return Finish();

        pet->m_Events.AddEvent(this, now + 50, false);
        return false;
    }

private:
    bool Finish()
    {
        _state->Finished = true;
        return true;
    }

    ObjectGuid _owner;
    ObjectGuid _pet;
    ObjectGuid _target;
    std::shared_ptr<LynxChargeState> _state;
    uint64 _deadline;
};

class LynxTargetCheck
{
public:
    explicit LynxTargetCheck(Pet* pet) : _pet(pet) { }

    bool operator()(Unit* target) const
    {
        return target != _pet && target->IsAlive() && _pet->IsValidAttackTarget(target)
            && _pet->IsWithinCombatRange(target, 10.0f) && _pet->CanSeeOrDetect(target);
    }

private:
    Pet* _pet;
};

static bool MakeLynxPath(Pet* pet, Unit* target, PathGenerator& path)
{
    float x, y, z;
    // Use combat reach, not just the model center, for large encounter mobs.
    target->GetNearPoint(pet, x, y, z, pet->GetCombatReach(),
        std::max(0.0f, target->GetCombatReach() - target->GetObjectSize()), target->GetAngle(pet));
    path.SetPathLengthLimit(20.0f);
    return path.CalculatePath(x, y, z) && !(path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE));
}

class spell_capstone_lynx_rush_aura : public AuraScript
{
    PrepareAuraScript(spell_capstone_lynx_rush_aura);

    uint8 _attempts = 0;
    ObjectGuid _primary;
    std::shared_ptr<LynxChargeState> _pending;

    bool Load() override
    {
        return GetUnitOwner()->IsPet();
    }

    bool Validate(SpellInfo const* info) override
    {
        return ValidateSpellInfo({SPELL_LYNX_RUSH_BLEED})
            && info->Effects[EFFECT_0].CalcValue() == 9 && info->Effects[EFFECT_0].Amplitude == 500;
    }

    void Strike(Unit* target)
    {
        // Strike() is called directly from the cast script right after AddAura, before the aura application exists, so
        // GetTarget() is null there (that dereference crashed the realm). GetUnitOwner() is valid from construction.
        Pet* pet = GetUnitOwner() ? GetUnitOwner()->ToPet() : nullptr;
        if (!pet || !target || _attempts >= 9 || !pet->IsAlive()
            || pet->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_CONTROLLED | UNIT_STATE_IN_FLIGHT | UNIT_STATE_POSSESSED))
            return;

        ++_attempts;
        if (pet->IsWithinMeleeRange(target) && pet->IsWithinLOSInMap(target))
        {
            pet->CastSpell(target, SPELL_LYNX_RUSH_BLEED, true);
            return;
        }

        PathGenerator path(pet);
        if (!MakeLynxPath(pet, target, path))
            return;

        _pending = std::make_shared<LynxChargeState>();
        pet->GetMotionMaster()->MoveCharge(path, SPEED_CHARGE, target->GetGUID());
        pet->m_Events.AddEvent(new LynxArrivalEvent(pet->GetOwnerGUID(), pet->GetGUID(), target->GetGUID(),
            _pending, pet->m_Events.CalculateTime(1000)), pet->m_Events.CalculateTime(50));
    }

    void Tick(AuraEffect const*)
    {
        Pet* pet = GetTarget()->ToPet();
        if (!pet || _attempts >= 9 || (_pending && !_pending->Finished))
            return;

        std::list<Unit*> targets;
        LynxTargetCheck check(pet);
        Acore::UnitListSearcher<LynxTargetCheck> searcher(pet, targets, check);
        Cell::VisitObjects(pet, searcher, pet->GetMap()->GetVisibilityRange());
        if (Unit* primary = ObjectAccessor::GetUnit(*pet, _primary))
            if (check(primary) && std::find(targets.begin(), targets.end(), primary) == targets.end())
                targets.push_back(primary);

        if (!targets.empty())
            Strike(Acore::Containers::SelectRandomContainerElement(targets));
    }

    void Remove(AuraEffect const*, AuraEffectHandleModes)
    {
        // A final charge already in flight can finish after natural expiry.
        // Dispel, cancellation, death and dismissal cancel its pending hit.
        if (_pending && GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            _pending->Cancelled = true;
    }

public:
    void Begin(Unit* target)
    {
        _primary = target->GetGUID();
        Strike(target);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_capstone_lynx_rush_aura::Tick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_lynx_rush_aura::Remove,
            EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_lynx_rush_cast : public SpellScript
{
    PrepareSpellScript(spell_capstone_lynx_rush_cast);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_LYNX_RUSH_AURA, SPELL_LYNX_RUSH_BLEED});
    }

    SpellCastResult CheckPet()
    {
        Player* player = GetCaster()->ToPlayer();
        Pet* pet = player ? player->GetPet() : nullptr;
        Unit* target = GetExplTargetUnit();
        if (!pet || !pet->IsAlive())
            return SPELL_FAILED_NO_PET;
        if (!target || !target->IsAlive() || !pet->IsValidAttackTarget(target))
            return SPELL_FAILED_BAD_TARGETS;
        if (pet->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_CONTROLLED | UNIT_STATE_IN_FLIGHT | UNIT_STATE_POSSESSED))
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        if (!pet->IsWithinCombatRange(target, 10.0f))
            return SPELL_FAILED_OUT_OF_RANGE;
        PathGenerator path(pet);
        if (!pet->IsWithinMeleeRange(target) && !MakeLynxPath(pet, target, path))
            return SPELL_FAILED_NOPATH;
        return SPELL_CAST_OK;
    }

    void BeginRush(SpellEffIndex)
    {
        Player* player = GetCaster()->ToPlayer();
        Pet* pet = player ? player->GetPet() : nullptr;
        if (!pet || !GetHitUnit())
            return;

        if (Aura* aura = player->AddAura(SPELL_LYNX_RUSH_AURA, pet))
        {
            if (auto* script = aura->GetScript<spell_capstone_lynx_rush_aura>("spell_capstone_lynx_rush_aura"))
            {
                script->Begin(GetHitUnit());
                return;
            }
            aura->Remove();
        }

        LOG_ERROR("spells.capstone", "Lynx Rush requires its registered pet aura script; check spell {} bindings.",
            uint32(SPELL_LYNX_RUSH_AURA));
        FinishCast(SPELL_FAILED_ERROR);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_lynx_rush_cast::CheckPet);
        OnEffectHitTarget += SpellEffectFn(spell_capstone_lynx_rush_cast::BeginRush, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_capstone_lynx_rush_bleed : public AuraScript
{
    PrepareAuraScript(spell_capstone_lynx_rush_bleed);

    void SnapshotCriticalChance(AuraEffect const*, AuraEffectHandleModes)
    {
        if (Unit* pet = GetCaster())
            GetAura()->GetEffect(EFFECT_0)->SetCritChance(pet->GetUnitCriticalChance(BASE_ATTACK, GetTarget()));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_lynx_rush_bleed::SnapshotCriticalChance,
            EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
    }
};

class spell_capstone_holy_prism : public SpellScript
{
    PrepareSpellScript(spell_capstone_holy_prism);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_HOLY_PRISM_DAMAGE, SPELL_HOLY_PRISM_AREA_HEAL,
            SPELL_HOLY_PRISM_HEAL, SPELL_HOLY_PRISM_AREA_DAMAGE});
    }

    SpellCastResult CheckTarget()
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target || !target->IsAlive())
            return SPELL_FAILED_BAD_TARGETS;

        return caster->IsValidAssistTarget(target) || caster->IsValidAttackTarget(target)
            ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
    }

    void ReleasePrism(SpellEffIndex)
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        bool friendly = caster->IsValidAssistTarget(target);
        TriggerCastFlags flags = TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_DISALLOW_PROC_EVENTS);
        caster->CastSpell(target, friendly ? SPELL_HOLY_PRISM_HEAL : SPELL_HOLY_PRISM_DAMAGE, flags);
        caster->CastSpell(target, friendly ? SPELL_HOLY_PRISM_AREA_DAMAGE : SPELL_HOLY_PRISM_AREA_HEAL, flags);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_capstone_holy_prism::CheckTarget);
        OnEffectHitTarget += SpellEffectFn(spell_capstone_holy_prism::ReleasePrism, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_capstone_holy_prism_allies : public SpellScript
{
    PrepareSpellScript(spell_capstone_holy_prism_allies);

    void SelectAllies(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster()->ToUnit();
        targets.remove_if([caster](WorldObject* object)
        {
            Unit* target = object->ToUnit();
            return !caster || !target || !target->IsAlive() || !caster->IsValidAssistTarget(target);
        });
        targets.sort(Acore::HealthPctOrderPred());
        if (targets.size() > 5)
            targets.resize(5);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_holy_prism_allies::SelectAllies,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

class spell_capstone_holy_prism_enemies : public SpellScript
{
    PrepareSpellScript(spell_capstone_holy_prism_enemies);

    void SelectEnemies(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster()->ToUnit();
        targets.remove_if([caster](WorldObject* object)
        {
            Unit* target = object->ToUnit();
            return !caster || !target || !target->IsAlive() || !caster->IsValidAttackTarget(target);
        });
        Acore::Containers::RandomResize(targets, 5);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_holy_prism_enemies::SelectEnemies,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

class spell_capstone_frost_bomb_cast : public SpellScript
{
    PrepareSpellScript(spell_capstone_frost_bomb_cast);

    void AdjustCooldown()
    {
        if (Player* player = GetCaster()->ToPlayer())
        {
            uint32 remaining = player->GetSpellCooldownDelay(GetSpellInfo()->Id);
            uint32 hasted = HastedTimer(remaining, player->GetFloatValue(UNIT_MOD_CAST_SPEED));
            player->ModifySpellCooldown(GetSpellInfo()->Id, int32(hasted) - int32(remaining));
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_frost_bomb_cast::AdjustCooldown);
    }
};

class spell_capstone_frost_bomb_aura : public AuraScript
{
    PrepareAuraScript(spell_capstone_frost_bomb_aura);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_FROST_BOMB_PRIMARY, SPELL_FROST_BOMB_SPLASH,
            SPELL_BRAIN_FREEZE_FIRST_RANK, SPELL_BRAIN_FREEZE_READY});
    }

    void Detonate(AuraEffect const*, AuraEffectHandleModes)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE || !GetTarget()->IsAlive())
            return;

        if (Unit* caster = GetCaster())
        {
            // Expiration owns the explosion. Dispels, death, refresh and manual
            // removal do not detonate it. Both hits are independently resolved.
            caster->CastSpell(GetTarget(), SPELL_FROST_BOMB_PRIMARY, true);
            caster->CastSpell(GetTarget(), SPELL_FROST_BOMB_SPLASH, true);
            if (caster->GetAuraEffectOfRankedSpell(SPELL_BRAIN_FREEZE_FIRST_RANK, EFFECT_0))
                caster->CastSpell(caster, SPELL_BRAIN_FREEZE_READY, true);
        }
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_frost_bomb_aura::Detonate,
            EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_frost_bomb_damage : public SpellScript
{
    PrepareSpellScript(spell_capstone_frost_bomb_damage);

    void BalancePlayerDamage()
    {
        if (Unit* target = GetHitUnit())
            SetHitDamage(FrostBombDamage(GetHitDamage(), target->IsPlayer()));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_frost_bomb_damage::BalancePlayerDamage);
    }
};

class spell_capstone_frost_bomb_splash : public SpellScript
{
    PrepareSpellScript(spell_capstone_frost_bomb_splash);

    void ExcludePrimary(std::list<WorldObject*>& targets)
    {
        // The primary receives its separate, larger hit. It remains in the
        // independent slow effect's target list, so it is still slowed.
        targets.remove(GetExplTargetUnit());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_frost_bomb_splash::ExcludePrimary,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

enum RequiredCreatures : uint32
{
    NPC_WATER_ASCENDANT = 990502,
};

class spell_capstone_restoration_ascendance : public AuraScript
{
    PrepareAuraScript(spell_capstone_restoration_ascendance);

    int32 _pending = 0;

    bool Load() override
    {
        return GetUnitOwner()->IsPlayer();
    }

    bool Validate(SpellInfo const*) override
    {
        CreatureTemplate const* form = sObjectMgr->GetCreatureTemplate(NPC_WATER_ASCENDANT);
        if (!form || !form->GetFirstValidModel())
        {
            LOG_ERROR("spells.capstone", "Ascendance requires the original Water Ascendant 59349 model mapped to creature {}.",
                uint32(NPC_WATER_ASCENDANT));
            return false;
        }
        return ValidateSpellInfo({SPELL_RESTORATIVE_MISTS});
    }

    bool CheckProc(ProcEventInfo& event)
    {
        SpellInfo const* info = event.GetSpellInfo();
        return event.GetActor() == GetTarget() && info && info->IsPositive()
            && info->Id != SPELL_RESTORATIVE_MISTS
            && event.GetHealInfo() && event.GetHealInfo()->GetHeal();
    }

    void QueueHealing(AuraEffect const*, ProcEventInfo& event)
    {
        PreventDefaultAction();
        _pending = AddHealingDebt(_pending, event.GetHealInfo()->GetHeal());
    }

    void Redistribute(AuraEffect const*)
    {
        // Aggregate every heal rather than dropping subsequent heals during
        // a one-second proc cooldown. The original aura ticks once per second.
        int32 amount = _pending;
        _pending = 0;
        if (amount && GetTarget()->IsAlive())
            GetTarget()->CastCustomSpell(SPELL_RESTORATIVE_MISTS, SPELLVALUE_BASE_POINT0,
                amount, GetTarget(), true);
    }

    void Expire(AuraEffect const* effect, AuraEffectHandleModes)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            Redistribute(effect);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_capstone_restoration_ascendance::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_capstone_restoration_ascendance::QueueHealing,
            EFFECT_2, SPELL_AURA_DUMMY);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_capstone_restoration_ascendance::Redistribute,
            EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_restoration_ascendance::Expire,
            EFFECT_1, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_restorative_mists : public SpellScript
{
    PrepareSpellScript(spell_capstone_restorative_mists);

    uint32 _targetCount = 0;

    void SelectAllies(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster()->ToUnit();
        targets.remove_if([caster](WorldObject* object)
        {
            Unit* target = object->ToUnit();
            return !caster || !target || !target->IsAlive() || !caster->IsValidAssistTarget(target);
        });
        _targetCount = uint32(targets.size());
    }

    void DivideHealing()
    {
        SetHitHeal(_targetCount ? GetHitHeal() / _targetCount : 0);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_restorative_mists::SelectAllies,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
        OnHit += SpellHitFn(spell_capstone_restorative_mists::DivideHealing);
    }
};

class spell_capstone_chosen_of_elune : public SpellScript
{
    PrepareSpellScript(spell_capstone_chosen_of_elune);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_MOONKIN_FORM, SPELL_ECLIPSE_LUNAR, SPELL_ECLIPSE_SOLAR});
    }

    void EnterMoonkin()
    {
        if (Unit* caster = GetCaster()->ToUnit())
            caster->CastSpell(caster, SPELL_MOONKIN_FORM, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_chosen_of_elune::EnterMoonkin);
    }
};

class capstone_eclipse_damage : public UnitScript
{
public:
    capstone_eclipse_damage() : UnitScript("capstone_eclipse_damage", true,
        {UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK}) { }

    static bool Applies(Unit const* caster, SpellInfo const* info)
    {
        return caster && info && !info->IsPositive()
            && !info->HasAttribute(SPELL_ATTR3_IGNORE_CASTER_MODIFIERS)
            && (info->GetSchoolMask() & (SPELL_SCHOOL_MASK_ARCANE | SPELL_SCHOOL_MASK_NATURE))
            && caster->GetShapeshiftForm() == FORM_MOONKIN
            && caster->HasAura(SPELL_INCARNATION_CHOSEN_OF_ELUNE)
            && (caster->HasAura(SPELL_ECLIPSE_LUNAR) || caster->HasAura(SPELL_ECLIPSE_SOLAR));
    }

    void ModifySpellDamageTaken(Unit*, Unit* caster, int32& damage, SpellInfo const* info) override
    {
        if (damage > 0 && Applies(caster, info))
            damage = AddHealingDebt(damage, uint32(std::uint64_t(damage) * 25 / 100));
    }

    void ModifyPeriodicDamageAurasTick(Unit*, Unit* caster, uint32& damage, SpellInfo const* info) override
    {
        // This shared core hook also receives periodic heals. Positive spells
        // are explicitly excluded, as are fixed-amount transfer helpers.
        if (Applies(caster, info))
            damage = uint32(std::min<std::uint64_t>(std::uint64_t(damage) * 125 / 100,
                std::numeric_limits<uint32>::max()));
    }
};

class capstone_spell_mod_rules : public GlobalScript
{
public:
    capstone_spell_mod_rules() : GlobalScript("capstone_spell_mod_rules",
        {GLOBALHOOK_ON_IS_AFFECTED_BY_SPELL_MOD_CHECK, GLOBALHOOK_ON_BEFORE_SET_BOSS_STATE}) { }

    void OnBeforeSetBossState(uint32, EncounterState state, EncounterState previous, Map* instance) override
    {
        // MoP AuraInterruptFlags2::EndOfEncounter has no Wrath DBC slot.
        // Preserve it through the native encounter-state event instead.
        if (!instance || previous != IN_PROGRESS
            || (state != DONE && state != FAIL && state != NOT_STARTED))
            return;

        for (auto const& entry : instance->GetPlayers())
            if (Player* player = entry.GetSource())
                player->RemoveAurasDueToSpell(SPELL_NATURES_VIGIL);
    }

    bool OnIsAffectedBySpellModCheck(SpellInfo const* modifier, SpellInfo const* spell,
        SpellModifier const* mod) override
    {
        // Returning false bypasses only the ordinary family-mask check. Wrath
        // Immolation Aura lacks family flags, unlike its MoP counterpart.
        if (modifier->Id == SPELL_MANNOROTHS_FURY
            && (mod->op == SPELLMOD_RADIUS || mod->op == SPELLMOD_DAMAGE)
            && (spell->Id == SPELL_IMMOLATION_AURA || spell->Id == SPELL_IMMOLATION_AURA_DAMAGE))
            return false;

        return true;
    }
};

class spell_capstone_natures_vigil : public AuraScript
{
    PrepareAuraScript(spell_capstone_natures_vigil);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_NATURES_VIGIL_HEAL, SPELL_NATURES_VIGIL_DAMAGE});
    }

    bool CheckProc(ProcEventInfo& event)
    {
        SpellInfo const* info = event.GetSpellInfo();
        if (event.GetActor() != GetTarget() || !event.GetActionTarget() || !info
            || info->Id == SPELL_NATURES_VIGIL_HEAL || info->Id == SPELL_NATURES_VIGIL_DAMAGE
            || info->IsTargetingArea() || info->IsAffectingArea())
            return false;

        for (SpellEffectInfo const& effect : info->GetEffects())
            if (effect.ChainTarget > 1)
                return false;

        return (event.GetHealInfo() && event.GetHealInfo()->GetHeal())
            || (event.GetDamageInfo() && event.GetDamageInfo()->GetDamage());
    }

    void Transfer(AuraEffect const* effect, ProcEventInfo& event)
    {
        PreventDefaultAction();
        Unit* caster = GetTarget();
        Unit* center = event.GetActionTarget();
        uint32 healing = event.GetHealInfo() ? event.GetHealInfo()->GetHeal() : 0;
        uint32 damage = event.GetDamageInfo() ? event.GetDamageInfo()->GetDamage() : 0;
        int32 amount = AddHealingDebt(0, uint32(std::uint64_t(healing ? healing : damage)
            * effect->GetAmount() / 100));
        if (!amount)
            return;

        caster->CastCustomSpell(SPELL_NATURES_VIGIL_HEAL, SPELLVALUE_BASE_POINT0,
            amount, center, true, nullptr, effect);
        if (healing)
            caster->CastCustomSpell(SPELL_NATURES_VIGIL_DAMAGE, SPELLVALUE_BASE_POINT0,
                amount, center, true, nullptr, effect);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_capstone_natures_vigil::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_capstone_natures_vigil::Transfer,
            EFFECT_2, SPELL_AURA_DUMMY);
    }
};

class spell_capstone_spirit_shell_heal : public SpellScript
{
    PrepareSpellScript(spell_capstone_spirit_shell_heal);

    bool Load() override
    {
        return GetCaster()->IsPlayer();
    }

    bool Validate(SpellInfo const* info) override
    {
        return info->HasEffect(SPELL_EFFECT_HEAL)
            && ValidateSpellInfo({SPELL_SPIRIT_SHELL, SPELL_SPIRIT_SHELL_ABSORB});
    }

    void ConvertHealing()
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetHitUnit();
        if (!caster || !target || !caster->HasAura(SPELL_SPIRIT_SHELL) || GetHitHeal() <= 0)
            return;

        SpellInfo const* info = GetSpellInfo();
        uint32 normal = uint32(GetHitHeal());
        float chance = caster->SpellDoneCritChance(target, info, info->GetSchoolMask(), BASE_ATTACK, false);
        chance = target->SpellTakenCritChance(caster, info, info->GetSchoolMask(), chance, BASE_ATTACK, false);
        uint32 critical = Unit::SpellCriticalHealingBonus(caster, info, normal, target);
        AuraEffect const* aegis = caster->GetAuraEffectOfRankedSpell(SPELL_DIVINE_AEGIS_FIRST_RANK, EFFECT_0);
        uint32 added = ExpectedSpiritShellHealing(normal, critical, chance, aegis ? aegis->GetAmount() : 0);
        AuraEffect const* existing = target->GetAuraEffect(SPELL_SPIRIT_SHELL_ABSORB, EFFECT_0, caster->GetGUID());
        int32 total = SpiritShellTotal(existing ? existing->GetAmount() : 0, added, caster->GetMaxHealth());

        // This runs before the ordinary random critical-heal and heal proc
        // branch. The shield includes expected Aegis instead of also creating
        // a second Aegis shield. Native hit procs such as Grace still run.
        PreventHitHeal();
        caster->CastCustomSpell(SPELL_SPIRIT_SHELL_ABSORB, SPELLVALUE_BASE_POINT0, total, target, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_spirit_shell_heal::ConvertHealing);
    }
};

class spell_capstone_shadow_blades : public AuraScript
{
    PrepareAuraScript(spell_capstone_shadow_blades);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_SHADOW_BLADE_MAIN_HAND, SPELL_SHADOW_BLADE_OFF_HAND});
    }

    void Register() override { }
};

class spell_capstone_shadow_reflection_aura : public AuraScript
{
    PrepareAuraScript(spell_capstone_shadow_reflection_aura);

public:
    void AttachClone(ObjectGuid guid) { _clone = guid; }

    bool RecordAction(uint32 spellId, uint8 comboPoints)
    {
        int32 const remaining = GetDuration();
        if (GetMaxDuration() != int32(ShadowReflectionTimeline::TotalDurationMs)
            || remaining > int32(ShadowReflectionTimeline::TotalDurationMs) || remaining < 0)
        {
            LOG_ERROR("spells", "Shadow Reflection aura has invalid duration {} of {}.", remaining, GetMaxDuration());
            return false;
        }

        uint32 const elapsed = ShadowReflectionTimeline::TotalDurationMs - uint32(remaining);
        if (elapsed >= ShadowReflectionTimeline::RecordingDurationMs)
            return false;
        if (!_timeline.Record(elapsed, spellId, comboPoints))
        {
            LOG_ERROR("spells", "Shadow Reflection cannot record spell {} with {} combo points at {} ms.",
                spellId, comboPoints, elapsed);
            return false;
        }
        return true;
    }

    std::optional<ReflectionAction> PopDueAction()
    {
        int32 const remaining = GetDuration();
        if (remaining < 0 || remaining > int32(ShadowReflectionTimeline::TotalDurationMs))
        {
            LOG_ERROR("spells", "Shadow Reflection replay has invalid duration {}.", remaining);
            return std::nullopt;
        }
        return _timeline.PopDue(ShadowReflectionTimeline::TotalDurationMs - uint32(remaining));
    }

    std::optional<ReflectionAction> PopExpiredAction()
    {
        return _timeline.PopDue(ShadowReflectionTimeline::TotalDurationMs);
    }

    void RemoveClone(AuraEffect const*, AuraEffectHandleModes);

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_capstone_shadow_reflection_aura::RemoveClone,
            EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

private:
    ShadowReflectionTimeline _timeline;
    ObjectGuid _clone;
};

struct npc_capstone_shadow_reflection : public ScriptedAI
{
    explicit npc_capstone_shadow_reflection(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetCanDualWield(true);
    }

    void SetGUID(ObjectGuid const& guid, int32 id = 0) override
    {
        if (id != SHADOW_REFLECTION_TARGET)
            return;
        _target = guid;
        if (Unit* target = ObjectAccessor::GetUnit(*me, _target))
            me->GetMotionMaster()->MoveFollow(target, 2.0f, 0.0f);
    }

    ObjectGuid GetGUID(int32 id = 0) const override
    {
        return id == SHADOW_REFLECTION_TARGET ? _target : ObjectGuid::Empty;
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        TempSummon* summon = me->ToTempSummon();
        Unit* owner = summon ? summon->GetSummonerUnit() : nullptr;
        Unit* target = ObjectAccessor::GetUnit(*me, _target);
        if (!owner || !owner->IsAlive() || !owner->HasAura(SPELL_SHADOW_REFLECTION)
            || !target || !target->IsAlive() || !me->CanSeeOrDetect(target))
        {
            if (owner && owner->HasAura(SPELL_SHADOW_REFLECTION))
            {
                owner->RemoveAurasDueToSpell(SPELL_SHADOW_REFLECTION);
                return;
            }
            me->DespawnOrUnsummon();
            return;
        }

        Aura* aura = owner->GetAura(SPELL_SHADOW_REFLECTION);
        auto* script = aura ? aura->GetScript<spell_capstone_shadow_reflection_aura>("spell_capstone_shadow_reflection_aura") : nullptr;
        if (!script || !DrainReplay(*script, target, false))
        {
            if (!script && !_loggedMissingScript)
            {
                _loggedMissingScript = true;  // UpdateAI runs every tick; log the missing script once per clone.
                LOG_ERROR("spells", "Shadow Reflection cannot replay without its recording script.");
            }
            owner->RemoveAurasDueToSpell(SPELL_SHADOW_REFLECTION);
        }
    }

    bool DrainReplay(spell_capstone_shadow_reflection_aura& script, Unit* target, bool expired)
    {
        TempSummon* summon = me->ToTempSummon();
        Unit* owner = summon ? summon->GetSummonerUnit() : nullptr;
        if (!owner)
            return false;

        while (std::optional<ReflectionAction> action = expired ? script.PopExpiredAction() : script.PopDueAction())
        {
            uint32 const root = sSpellMgr->GetFirstSpellInChain(action->SpellId);
            switch (root)
            {
                case 53:    // Backstab
                case 703:   // Garrote
                case 8676:  // Ambush
                case 1752:  // Sinister Strike
                case 51690: // Killing Spree
                case 51723: // Fan of Knives
                case 16511: // Hemorrhage
                case 1943:  // Rupture
                case 2098:  // Eviscerate
                case 32645: // Envenom
                case 1329:  // Mutilate; its two child strikes are never recorded separately
                case 408:  // Kidney Shot
                case 1766: // Kick
                case 1833: // Cheap Shot
                case 1776: // Gouge
                case 2094: // Blind
                    SyncCombatStats(*owner);
                    me->ClearComboPoints();
                    if (action->ComboPoints)
                        me->AddComboPoints(target, int8(action->ComboPoints));
                    me->CastSpell(target, action->SpellId, true);
                    me->ClearComboPoints();
                    break;
                default:
                    LOG_ERROR("spells", "Shadow Reflection rejected unregistered action {} (root {}).", action->SpellId, root);
                    return false;
            }
        }
        return true;
    }

private:
    void SyncCombatStats(Unit const& owner)
    {
        // WoD reflections use the rogue's current offensive stats when the
        // recorded action is replayed. Creature-template defaults are unrelated
        // to the summoner, so copy the live Wrath AP and weapon snapshot here.
        me->SetInt32Value(UNIT_FIELD_ATTACK_POWER, owner.GetInt32Value(UNIT_FIELD_ATTACK_POWER));
        me->SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS, owner.GetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS));
        me->SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, owner.GetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER));
        for (WeaponAttackType attack : { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK })
        {
            me->SetAttackTime(attack, owner.GetAttackTime(attack));
            for (WeaponDamageRange range : { MINDAMAGE, MAXDAMAGE })
                me->SetBaseWeaponDamage(attack, range, owner.GetWeaponDamageRange(attack, range));
        }
    }

    ObjectGuid _target;
    bool _loggedMissingScript = false;
};

void spell_capstone_shadow_reflection_aura::RemoveClone(AuraEffect const*, AuraEffectHandleModes)
{
    if (!_clone.IsEmpty())
    {
        if (Creature* clone = ObjectAccessor::GetCreature(*GetTarget(), _clone))
        {
            if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE && clone->IsAIEnabled)
            {
                auto* ai = dynamic_cast<npc_capstone_shadow_reflection*>(clone->AI());
                Unit* target = ai ? ObjectAccessor::GetUnit(*clone, ai->GetGUID(SHADOW_REFLECTION_TARGET)) : nullptr;
                if (ai && GetTarget()->IsAlive() && target && target->IsAlive() && clone->CanSeeOrDetect(target))
                    ai->DrainReplay(*this, target, true);
            }
            clone->DespawnOrUnsummon();
        }
        _clone.Clear();
    }
}

class spell_capstone_shadow_reflection_summon : public SpellScript
{
    PrepareSpellScript(spell_capstone_shadow_reflection_summon);

    bool _hit = false;
    bool _currentTargetHit = false;
    bool _initialTargetHit = false;

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_MIRROR_IMAGE_CLONE});
    }

    void CheckTargetHit(SpellMissInfo missInfo)
    {
        _currentTargetHit = false;
        _initialTargetHit = missInfo == SPELL_MISS_NONE;
    }

    void MarkAppliedEffect(SpellEffIndex)
    {
        _currentTargetHit = _initialTargetHit;
    }

    void RememberTargetHit()
    {
        if (!_currentTargetHit)
            return;
        if (Unit* target = GetHitUnit())
            if (target == GetExplTargetUnit())
                _hit = true;
    }

    void SummonReflection()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetExplTargetUnit();
        if (!player)
            return;
        if (!target || !_hit)
        {
            player->RemoveAurasDueToSpell(SPELL_SHADOW_REFLECTION);
            return;
        }
        Aura* aura = player->GetAura(SPELL_SHADOW_REFLECTION);
        auto* script = aura ? aura->GetScript<spell_capstone_shadow_reflection_aura>("spell_capstone_shadow_reflection_aura") : nullptr;
        if (!script)
        {
            LOG_ERROR("spells", "Shadow Reflection requires its recording aura before summoning.");
            if (aura)
                player->RemoveAurasDueToSpell(SPELL_SHADOW_REFLECTION);
            return;
        }
        // The aura owns the clone's lifetime; an independent timer could
        // despawn it before the final replay on a delayed world update.
        TempSummon* clone = player->SummonCreature(NPC_SHADOW_REFLECTION, target->GetPosition(),
            TEMPSUMMON_MANUAL_DESPAWN, 0);
        auto* cloneAI = clone && clone->IsAIEnabled ? dynamic_cast<npc_capstone_shadow_reflection*>(clone->AI()) : nullptr;
        if (!cloneAI)
        {
            LOG_ERROR("spells", "Shadow Reflection cannot summon creature {} with its required AI.", uint32(NPC_SHADOW_REFLECTION));
            if (clone)
                clone->DespawnOrUnsummon();
            player->RemoveAurasDueToSpell(SPELL_SHADOW_REFLECTION);
            return;
        }
        // Ownership makes player-controlled diminishing returns apply while
        // retaining the reflection as the actual aura caster.
        clone->SetOwnerGUID(player->GetGUID());
        clone->SetFaction(player->GetFaction());
        player->CastSpell(clone, SPELL_MIRROR_IMAGE_CLONE, true);
        if (!clone->HasAura(SPELL_MIRROR_IMAGE_CLONE))
        {
            LOG_ERROR("spells", "Shadow Reflection failed to copy the owner's appearance.");
            clone->DespawnOrUnsummon();
            player->RemoveAurasDueToSpell(SPELL_SHADOW_REFLECTION);
            return;
        }
        clone->SetVirtualItem(0, player->GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + EQUIPMENT_SLOT_MAINHAND * 2));
        clone->SetVirtualItem(1, player->GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + EQUIPMENT_SLOT_OFFHAND * 2));
        cloneAI->SetGUID(target->GetGUID(), SHADOW_REFLECTION_TARGET);
        script->AttachClone(clone->GetGUID());
    }

    void Register() override
    {
        BeforeHit += BeforeSpellHitFn(spell_capstone_shadow_reflection_summon::CheckTargetHit);
        OnEffectHitTarget += SpellEffectFn(spell_capstone_shadow_reflection_summon::MarkAppliedEffect, EFFECT_ALL, SPELL_EFFECT_ANY);
        OnHit += SpellHitFn(spell_capstone_shadow_reflection_summon::RememberTargetHit);
        AfterCast += SpellCastFn(spell_capstone_shadow_reflection_summon::SummonReflection);
    }
};

class spell_capstone_shadow_reflection_capture : public SpellScript
{
    PrepareSpellScript(spell_capstone_shadow_reflection_capture);

    uint8 _comboPoints = 0;
    bool _eligible = false;
    bool _recorded = false;
    bool _currentTargetHit = false;
    bool _initialTargetHit = false;

    void SnapshotComboPoints()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || GetSpell()->IsTriggered() || !player->HasAura(SPELL_SHADOW_REFLECTION))
            return;

        _comboPoints = player->GetComboPoints(GetExplTargetUnit());
        _eligible = true;
    }

    void CheckTargetHit(SpellMissInfo missInfo)
    {
        _currentTargetHit = false;
        _initialTargetHit = missInfo == SPELL_MISS_NONE;
    }

    void MarkAppliedEffect(SpellEffIndex)
    {
        _currentTargetHit = _initialTargetHit;
    }

    void RecordSuccessfulHit()
    {
        if (!_eligible || _recorded || !_currentTargetHit || !GetHitUnit())
            return;

        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;
        if (Aura* aura = player->GetAura(SPELL_SHADOW_REFLECTION))
        {
            if (auto* script = aura->GetScript<spell_capstone_shadow_reflection_aura>("spell_capstone_shadow_reflection_aura"))
                _recorded = script->RecordAction(GetSpellInfo()->Id, _comboPoints);
            else
                LOG_ERROR("spells", "Shadow Reflection aura is missing its recording script.");
        }
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_capstone_shadow_reflection_capture::SnapshotComboPoints);
        BeforeHit += BeforeSpellHitFn(spell_capstone_shadow_reflection_capture::CheckTargetHit);
        OnEffectHitTarget += SpellEffectFn(spell_capstone_shadow_reflection_capture::MarkAppliedEffect, EFFECT_ALL, SPELL_EFFECT_ANY);
        OnHit += SpellHitFn(spell_capstone_shadow_reflection_capture::RecordSuccessfulHit);
    }
};

class spell_capstone_shadow_blades_combo : public SpellScript
{
    PrepareSpellScript(spell_capstone_shadow_blades_combo);

    bool _granted = false;

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_SHADOW_BLADES});
    }

    void GrantComboPoint()
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetHitUnit();
        Spell* spell = GetSpell();
        // Native effects determine whether this attack earned points. This
        // excludes misses and separate Mutilate hand strikes, and gives one
        // bonus for the parent ability rather than one per damage component.
        if (_granted || !caster || !target || !caster->HasAura(SPELL_SHADOW_BLADES)
            || spell->m_comboTarget != target || spell->m_comboPointGain <= 0)
            return;

        spell->AddComboPointGain(target, 1);
        _granted = true;
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_shadow_blades_combo::GrantComboPoint);
    }
};

class spell_capstone_vigil_heal : public SpellScript
{
    PrepareSpellScript(spell_capstone_vigil_heal);

    void SelectAlly(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster()->ToUnit();
        targets.remove_if([caster](WorldObject* object)
        {
            Unit* unit = object->ToUnit();
            return !caster || !unit || !unit->IsAlive() || !caster->IsValidAssistTarget(unit);
        });
        targets.sort(Acore::HealthPctOrderPred());
        if (targets.size() > 1)
            targets.resize(1);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_vigil_heal::SelectAlly,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

class spell_capstone_vigil_damage : public SpellScript
{
    PrepareSpellScript(spell_capstone_vigil_damage);

    void SelectEnemy(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster()->ToUnit();
        targets.remove_if([caster](WorldObject* object)
        {
            Unit* unit = object->ToUnit();
            return !caster || !unit || !unit->IsAlive() || !caster->IsValidAttackTarget(unit)
                || !unit->IsInCombat() || unit->HasStealthAura() || unit->HasInvisibilityAura()
                || unit->HasBreakableByDamageCrowdControlAura() || !caster->CanSeeOrDetect(unit);
        });
        Acore::Containers::RandomResize(targets, 1);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_capstone_vigil_damage::SelectEnemy,
            EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

class spell_capstone_runic_corruption : public AuraScript
{
    PrepareAuraScript(spell_capstone_runic_corruption);

    bool Load() override
    {
        return GetUnitOwner()->IsPlayer()
            && GetUnitOwner()->IsClass(CLASS_DEATH_KNIGHT, CLASS_CONTEXT_ABILITY);
    }

    bool Validate(SpellInfo const* info) override
    {
        return ValidateSpellInfo({SPELL_RUNIC_CORRUPTION_REGEN}) && info->ProcChance == 45;
    }

    bool CheckProc(ProcEventInfo& event)
    {
        SpellInfo const* info = event.GetSpellInfo();
        Unit* actor = event.GetActor();
        if (!info || actor != GetTarget() || !actor->IsPlayer() || !event.GetDamageInfo())
            return false;

        if (!(event.GetHitMask() & (PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB | PROC_HIT_BLOCK)))
            return false;

        uint32 firstRank = sSpellMgr->GetFirstSpellInChain(info->Id);
        // Off-hand Threat of Thassarian copies have different first-rank IDs;
        // they must not produce a second proc for the same resource expenditure.
        return info->Id == SPELL_DEATH_COIL_DAMAGE || firstRank == SPELL_FROST_STRIKE_FIRST_RANK
            || firstRank == SPELL_RUNE_STRIKE;
    }

    void Proc(AuraEffect const*, ProcEventInfo&)
    {
        PreventDefaultAction();
        Player* player = GetTarget()->ToPlayer();
        Aura* active = player->GetAura(SPELL_RUNIC_CORRUPTION_REGEN);
        uint32 baseCooldown = 0;
        for (uint8 i = 0; i < MAX_RUNES; ++i)
            baseCooldown += player->GetRuneBaseCooldown(i, true);

        baseCooldown /= MAX_RUNES;
        if (active)
            baseCooldown = ScaleRuneCooldown(baseCooldown, 100, false);

        uint32 duration = RuneCorruptionDuration(baseCooldown);
        if (active)
        {
            int32 total = active->GetDuration() + duration;
            active->SetMaxDuration(std::max(active->GetMaxDuration(), total));
            active->SetDuration(total);
        }
        else
        {
            player->CastSpell(player, SPELL_RUNIC_CORRUPTION_REGEN, true);
            if (Aura* added = player->GetAura(SPELL_RUNIC_CORRUPTION_REGEN))
            {
                added->SetMaxDuration(duration);
                added->SetDuration(duration);
            }
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_capstone_runic_corruption::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_capstone_runic_corruption::Proc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

class spell_capstone_runic_regeneration : public AuraScript
{
    PrepareAuraScript(spell_capstone_runic_regeneration);

    bool Validate(SpellInfo const* info) override
    {
        uint32 runeTypes = 0;
        for (SpellEffectInfo const& effect : info->Effects)
        {
            if (effect.ApplyAuraName != SPELL_AURA_MOD_POWER_REGEN_PERCENT || effect.MiscValue != POWER_RUNE
                || effect.MiscValueB < 0 || effect.MiscValueB >= 3 || effect.CalcValue() != 50)
                return false;

            runeTypes |= 1U << effect.MiscValueB;
        }
        return runeTypes == 7;
    }

    bool Load() override
    {
        return GetUnitOwner()->IsPlayer()
            && GetUnitOwner()->IsClass(CLASS_DEATH_KNIGHT, CLASS_CONTEXT_ABILITY);
    }

    void Rescale(AuraEffect const* effect, bool applying)
    {
        Player* player = GetTarget()->ToPlayer();
        for (uint8 i = 0; i < MAX_RUNES; ++i)
            if (player->GetBaseRune(i) == effect->GetMiscValueB())
                player->SetRuneCooldown(i, ScaleRuneCooldown(player->GetRuneCooldown(i), 100, applying));

        player->ResyncRunes(MAX_RUNES);
    }

    void Apply(AuraEffect const* effect, AuraEffectHandleModes)
    {
        Rescale(effect, true);
    }

    void Remove(AuraEffect const* effect, AuraEffectHandleModes)
    {
        Rescale(effect, false);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_capstone_runic_regeneration::Apply,
            EFFECT_ALL, SPELL_AURA_MOD_POWER_REGEN_PERCENT, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_runic_regeneration::Remove,
            EFFECT_ALL, SPELL_AURA_MOD_POWER_REGEN_PERCENT, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_death_prevention : public AuraScript
{
    PrepareAuraScript(spell_capstone_death_prevention);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_PURGATORY_SHROUD, SPELL_PURGATORY_COOLDOWN,
            SPELL_CAUTERIZE_BURN, SPELL_CAUTERIZE_COOLDOWN});
    }

    bool Load() override
    {
        return GetUnitOwner()->IsPlayer();
    }

    void UnlimitedAbsorb(AuraEffect const*, int32& amount, bool& canRecalculate)
    {
        amount = -1;
        canRecalculate = false;
    }

    void Absorb(AuraEffect*, DamageInfo& damage, uint32& amount)
    {
        amount = 0;
        Player* player = GetTarget()->ToPlayer();
        if (damage.GetDamage() < player->GetHealth())
            return;

        if (GetSpellInfo()->Id == SPELL_CAUTERIZE)
        {
            if (player->HasSpellCooldown(SPELL_CAUTERIZE_COOLDOWN))
                return;

            player->CastSpell(player, SPELL_CAUTERIZE_BURN, true);
            if (!player->HasAura(SPELL_CAUTERIZE_BURN))
            {
                LOG_ERROR("spells", "Cauterize could not apply its burn to player {}.", player->GetGUID().ToString());
                return;
            }
            player->AddSpellCooldown(SPELL_CAUTERIZE_COOLDOWN, 0, 2 * MINUTE * IN_MILLISECONDS);
            player->CastSpell(player, SPELL_CAUTERIZE_COOLDOWN, true);
            uint32 percent = GetSpellInfo()->Effects[EFFECT_1].CalcValue(player);
            player->SetHealth(std::max<uint32>(1, player->CountPctFromMaxHealth(percent)));
            amount = damage.GetDamage();
            return;
        }

        AuraEffect* debt = player->GetAuraEffect(SPELL_PURGATORY_SHROUD, EFFECT_0);
        if (!debt && player->HasSpellCooldown(SPELL_PURGATORY_COOLDOWN))
            return;

        uint32 prevented = FatalDamagePrevented(damage.GetDamage(), player->GetHealth());
        if (debt)
            debt->ChangeAmount(AddHealingDebt(debt->GetAmount(), prevented));
        else
        {
            int32 initialDebt = AddHealingDebt(0, prevented);
            player->CastCustomSpell(player, SPELL_PURGATORY_SHROUD, &initialDebt, nullptr, nullptr, true);
            if (!player->GetAuraEffect(SPELL_PURGATORY_SHROUD, EFFECT_0))
            {
                LOG_ERROR("spells", "Purgatory could not apply its healing debt to player {}.", player->GetGUID().ToString());
                return;
            }
            player->AddSpellCooldown(SPELL_PURGATORY_COOLDOWN, 0, 3 * MINUTE * IN_MILLISECONDS);
            player->CastSpell(player, SPELL_PURGATORY_COOLDOWN, true);
        }
        amount = prevented;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_capstone_death_prevention::UnlimitedAbsorb,
            EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_capstone_death_prevention::Absorb, EFFECT_0);
    }
};

class spell_capstone_purgatory_shroud : public AuraScript
{
    PrepareAuraScript(spell_capstone_purgatory_shroud);

    void Expire(AuraEffect const* effect, AuraEffectHandleModes)
    {
        Unit* target = GetTarget();
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE || effect->GetAmount() <= 0
            || !target->IsAlive())
            return;

        target->KillSelf(true, BASE_ATTACK, GetSpellInfo());
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_purgatory_shroud::Expire,
            EFFECT_0, SPELL_AURA_SCHOOL_HEAL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_marked_for_death : public AuraScript
{
    PrepareAuraScript(spell_capstone_marked_for_death);

    void OnRemove(AuraEffect const*, AuraEffectHandleModes)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
            return;

        if (Unit* caster = GetCaster())
            if (Player* player = caster->ToPlayer())
                player->RemoveSpellCooldown(SPELL_MARKED_FOR_DEATH, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_capstone_marked_for_death::OnRemove,
            EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_capstone_avatar : public SpellScript
{
    PrepareSpellScript(spell_capstone_avatar);

    void BreakMovementImpairment()
    {
        if (Unit* caster = GetCaster()->ToUnit())
            caster->RemoveMovementImpairingAuras(true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_capstone_avatar::BreakMovementImpairment);
    }
};

class spell_capstone_storm_bolt : public SpellScript
{
    PrepareSpellScript(spell_capstone_storm_bolt);

    void HandleDamage()
    {
        // Temporary player immunities do not satisfy the original permanent-
        // immunity condition. Read the creature template's mechanic immunity.
        if (Creature* target = GetHitCreature())
            if (target->HasMechanicTemplateImmunity(1ULL << MECHANIC_STUN))
                SetHitDamage(StormBoltDamage(GetHitDamage(), true));
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_storm_bolt::HandleDamage);
    }
};

class spell_capstone_unholy_blight_pulse : public SpellScript
{
    PrepareSpellScript(spell_capstone_unholy_blight_pulse);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_BLOOD_PLAGUE, SPELL_FROST_FEVER});
    }

    void Infect(SpellEffIndex)
    {
        Unit* caster = GetCaster()->ToUnit();
        Unit* target = GetHitUnit();
        if (!caster || !target || !caster->IsValidAttackTarget(target))
            return;

        caster->CastSpell(target, SPELL_BLOOD_PLAGUE, true);
        caster->CastSpell(target, SPELL_FROST_FEVER, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_capstone_unholy_blight_pulse::Infect,
            EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_capstone_dragon_roar : public SpellScript
{
    PrepareSpellScript(spell_capstone_dragon_roar);

    bool Validate(SpellInfo const*) override
    {
        return ValidateSpellInfo({SPELL_DRAGON_ROAR_CONTROL});
    }

    void ApplyControl()
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_DRAGON_ROAR_CONTROL, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_capstone_dragon_roar::ApplyControl);
    }
};

class capstone_combat_metadata : public AllSpellScript
{
public:
    capstone_combat_metadata() : AllSpellScript("capstone_combat_metadata",
        {ALLSPELLHOOK_ON_METADATA_CORRECTION, ALLSPELLHOOK_ON_CALCULATED_TARGET}) { }

    void OnSpellInfoCorrection(SpellInfo* info) override
    {
        if (info->Id == SPELL_DRAGON_ROAR)
            info->AttributesCu |= SPELL_ATTR0_CU_IGNORE_ARMOR;

        // Runic Corruption (991006): the DBC leaves effect 2 (Frost runes, MiscValueB 1) on power 0,
        // so it buffs mana regen and fails spell_capstone_runic_regeneration's validation.
        if (info->Id == 991006)
            for (SpellEffectInfo& effect : info->Effects)
                if (effect.ApplyAuraName == SPELL_AURA_MOD_POWER_REGEN_PERCENT)
                    effect.MiscValue = POWER_RUNE;
    }

    void OnSpellCalculatedTarget(Spell* spell, Unit*, TargetInfo& hit) override
    {
        if (spell->GetSpellInfo()->Id == SPELL_DRAGON_ROAR && hit.missCondition == SPELL_MISS_NONE)
            hit.crit = true;
    }
};
}

void Addmod_capstone_talentsScripts()
{
    using namespace CapstoneTalents;
    RegisterCreatureAI(npc_capstone_supremacy_abyssal);
    RegisterCreatureAI(npc_capstone_supremacy_terrorguard);
    RegisterCreatureAI(npc_capstone_shadow_reflection);
    RegisterSpellScript(spell_capstone_supremacy_summon);
    RegisterSpellScript(spell_capstone_supremacy_doom_bolt);
    RegisterSpellScript(spell_capstone_supremacy_suffering);
    RegisterSpellScript(spell_capstone_supremacy_shadow_bulwark);
    RegisterSpellScript(spell_capstone_supremacy_bulwark_owner);
    RegisterSpellScript(spell_capstone_supremacy_void_shield);
    RegisterSpellScript(spell_capstone_supremacy_void_shield_aura);
    RegisterSpellScript(spell_capstone_supremacy_cauterize_master);
    RegisterSpellScript(spell_capstone_supremacy_mortal_cleave);
    RegisterSpellScript(spell_capstone_supremacy_clone_magic);
    RegisterSpellScript(spell_capstone_supremacy_clone_heal);
    RegisterSpellScript(spell_capstone_supremacy_sear_magic);
    new capstone_supremacy_player();
    RegisterSpellScript(spell_capstone_elemental_blast);
    RegisterSpellScript(spell_capstone_elemental_blast_stat);
    RegisterSpellScript(spell_capstone_primal_summon);
    RegisterSpellScript(spell_capstone_primal_elementalist);
    RegisterSpellScript(spell_capstone_primal_immolate);
    RegisterSpellScript(spell_capstone_mindbender);
    RegisterSpellScript(spell_capstone_mindbender_mana);
    new capstone_mindbender_player();
    RegisterSpellScript(spell_capstone_marked_for_death);
    RegisterSpellScript(spell_capstone_avatar);
    RegisterSpellScript(spell_capstone_storm_bolt);
    RegisterSpellScript(spell_capstone_unholy_blight_pulse);
    RegisterSpellScript(spell_capstone_dragon_roar);
    RegisterSpellScript(spell_capstone_death_prevention);
    RegisterSpellScript(spell_capstone_purgatory_shroud);
    RegisterSpellScript(spell_capstone_runic_corruption);
    RegisterSpellScript(spell_capstone_runic_regeneration);
    RegisterSpellScript(spell_capstone_natures_vigil);
    RegisterSpellScript(spell_capstone_spirit_shell_heal);
    RegisterSpellScript(spell_capstone_shadow_blades);
    RegisterSpellScript(spell_capstone_shadow_blades_combo);
    RegisterSpellScript(spell_capstone_shadow_reflection_aura);
    RegisterSpellScript(spell_capstone_shadow_reflection_capture);
    RegisterSpellScript(spell_capstone_shadow_reflection_summon);
    RegisterSpellScript(spell_capstone_chosen_of_elune);
    RegisterSpellScript(spell_capstone_restoration_ascendance);
    RegisterSpellScript(spell_capstone_restorative_mists);
    RegisterSpellScript(spell_capstone_frost_bomb_cast);
    RegisterSpellScript(spell_capstone_frost_bomb_aura);
    RegisterSpellScript(spell_capstone_frost_bomb_damage);
    RegisterSpellScript(spell_capstone_frost_bomb_splash);
    RegisterSpellScript(spell_capstone_holy_prism);
    RegisterSpellScript(spell_capstone_holy_prism_allies);
    RegisterSpellScript(spell_capstone_holy_prism_enemies);
    RegisterSpellScript(spell_capstone_lynx_rush_cast);
    RegisterSpellScript(spell_capstone_lynx_rush_aura);
    RegisterSpellScript(spell_capstone_lynx_rush_bleed);
    RegisterSpellScript(spell_capstone_binding_shot_cast);
    RegisterSpellScript(spell_capstone_binding_shot_zone);
    RegisterSpellScript(spell_capstone_binding_shot_targets);
    RegisterSpellScript(spell_capstone_binding_shot_tether);
    RegisterSpellScript(spell_capstone_halo);
    RegisterSpellScript(spell_capstone_halo_impact);
    RegisterSpellScript(spell_capstone_rune_of_power);
    RegisterSpellScript(spell_capstone_rune_of_power_ground);
    RegisterSpellScript(spell_capstone_rune_of_power_buff);
    RegisterSpellScript(spell_capstone_execution_sentence);
    RegisterSpellScript(spell_capstone_execution_sentence_damage_aura);
    RegisterSpellScript(spell_capstone_execution_sentence_heal_aura);
    RegisterSpellScript(spell_capstone_king_of_the_jungle);
    RegisterSpellScript(spell_capstone_king_of_the_jungle_prowl);
    RegisterSpellScript(spell_capstone_vigil_heal);
    RegisterSpellScript(spell_capstone_vigil_damage);
    new capstone_spell_mod_rules();
    new capstone_eclipse_damage();
    new capstone_combat_metadata();
    new capstone_rune_player();
    new capstone_rune_pet_damage();
}
