"""Port the reviewed class-combat contracts into the pinned supported core.

This is an authoring utility, not a runtime fallback. Every edit is preflighted.
The upstream snippets retain their original license in their destination files.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
CORE = ROOT / "PlayerbotCore/src/server/game"
SOURCE = ROOT / ".advai-deps/azerothcore-wotlk-coa/src/server/game"
edits = {}


def replace(path, old, new, count=1):
    target = CORE / path
    text = edits.get(target, target.read_text(encoding="utf-8"))
    if text.count(old) != count:
        raise RuntimeError(f"{path}: expected {count} occurrences of {old[:90]!r}")
    edits[target] = text.replace(old, new)


def section(path, begin, end):
    text = (SOURCE / path).read_text(encoding="utf-8")
    return text[text.index(begin):text.index(end, text.index(begin))]


replace("Spells/SpellDefines.h", "SPELLMOD_SPELL_COST_REFUND_ON_FAIL = 30",
        "SPELLMOD_SPELL_COST_REFUND_ON_FAIL = 30,\n    SPELLMOD_MAX_AURA_STACKS = 31")
replace("Spells/SpellInfo.h", "    uint32 StackAmount;",
        "    uint32 StackAmount;\n    uint8 CalcMaxAuraStacks(Unit* caster = nullptr) const;")
stack = section("Spells/SpellInfo.cpp", "uint8 SpellInfo::CalcMaxAuraStacks", "namespace\n{")
replace("Spells/SpellInfo.cpp", "bool SpellInfo::IsPassive() const", stack + "bool SpellInfo::IsPassive() const")
replace("Spells/Auras/SpellAuras.cpp",
        "    if ((num > 0) && (stackAmount > int32(m_spellInfo->StackAmount)))",
        "    uint8 maximum = m_spellInfo->CalcMaxAuraStacks(GetCaster());\n"
        "    if ((num > 0) && (stackAmount > int32(maximum)))")
replace("Spells/Auras/SpellAuras.cpp", "            stackAmount = m_spellInfo->StackAmount;",
        "            stackAmount = maximum;")
enums = section("Spells/Auras/SpellAuraDefines.h", "enum AscensionConditionalCombatScript", "enum AuraObjectType")
replace("Spells/Auras/SpellAuraDefines.h", "enum AuraObjectType", enums + "enum AuraObjectType")
replace("Entities/Unit/Unit.h", "    float GetUnitCriticalChance(",
        "    bool CanReceivePowerFromSpell(Powers power) const;\n"
        "    bool IsImmuneToForcedMovement() const;\n"
        "    bool HasAscensionConditionalCombatState(int32 state) const;\n"
        "    int32 GetAscensionConditionalCombatModifier(Unit const* victim, SpellInfo const* spellInfo,\n"
        "        AscensionConditionalCombatModifier modifier) const;\n\n    float GetUnitCriticalChance(")
combat = section("Entities/Unit/Unit.cpp", "bool Unit::HasAscensionConditionalCombatState", "float Unit::GetUnitCriticalChance")
combat = combat.replace("AURA_STATE_ASCENSION_POISONED", "AURA_STATE_DEADLY_POISON")
combat = combat.replace("[this, victim, spellInfo, modifier](AuraEffect const* effect)",
                        "[this, victim, spellInfo, modifier](AuraEffect const* effect)")
combat = combat.replace("        if (effect->GetSpellInfo()->SpellFamilyName != uint32(getClass()) + 6)",
    "        uint32 family = ToPlayer()->HasSpell(804360) ? 29 : ToPlayer()->HasSpell(500720) ? 31 : uint32(getClass()) + 6;\n"
    "        if (effect->GetSpellInfo()->SpellFamilyName != family)")
combat = combat.replace("getClass() == CLASS_NECROMANCER", "ToPlayer()->HasSpell(804360)")
combat = combat.replace("(ToPlayer()->HasSpell(804360) || getClass() == CLASS_SON_OF_ARUGAL)", "ToPlayer()->HasSpell(804360)")
combat = combat.replace("aura->GetSpellInfo()->SpellFamilyName == uint32(getClass()) + 6", "aura->GetSpellInfo()->SpellFamilyName == family")
helpers = """bool Unit::CanReceivePowerFromSpell(Powers power) const
{
    return getPowerType() == power && GetMaxPower(power) > 0;
}

bool Unit::IsImmuneToForcedMovement() const
{
    Creature const* creature = ToCreature();
    return creature && (creature->isWorldBoss() || creature->IsDungeonBoss() || creature->IsImmuneToKnockback());
}

"""
replace("Entities/Unit/Unit.cpp", "float Unit::GetUnitCriticalChance(", helpers + combat + "float Unit::GetUnitCriticalChance(")
replace("Entities/Unit/Unit.cpp", "\n    // reduce crit chance from Rating for players",
        "\n    crit += GetAscensionConditionalCombatModifier(victim, nullptr, ASCENSION_CONDITIONAL_CRIT_CHANCE);\n\n"
        "    // reduce crit chance from Rating for players")
replace("Entities/Unit/Unit.cpp", "        crit_chance += GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);\n\n    // xinef: can be negative!\n    return crit_chance;",
        "        crit_chance += GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);\n\n    if (caster)\n    {\n"
        "        crit_chance += caster->GetAscensionConditionalCombatModifier(this, spellProto, ASCENSION_CONDITIONAL_CRIT_CHANCE);\n"
        "        if (caster->GetAscensionConditionalCombatModifier(this, spellProto, ASCENSION_CONDITIONAL_GUARANTEED_CRIT) > 0)\n"
        "            return 100.0f;\n    }\n\n"
        "    // xinef: can be negative!\n    return crit_chance;")
replace("Entities/Unit/Unit.cpp",
        "        crit_mod += caster->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, spellProto->GetSchoolMask());",
        "        crit_mod += caster->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, spellProto->GetSchoolMask());\n"
        "        crit_mod += caster->GetAscensionConditionalCombatModifier(victim, spellProto, ASCENSION_CONDITIONAL_CRIT_DAMAGE);")
replace("Entities/Unit/Unit.cpp",
        "                    crit_mod += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, spellInfo->GetSchoolMask());",
        "                    crit_mod += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_CRIT_DAMAGE_BONUS, spellInfo->GetSchoolMask());\n"
        "                    crit_mod += GetAscensionConditionalCombatModifier(victim, spellInfo, ASCENSION_CONDITIONAL_CRIT_DAMAGE);")
replace("Entities/Unit/Unit.cpp", "        // Apply Player CR_ARMOR_PENETRATION rating and buffs from stances\\specializations etc.",
        "        int32 conditionalIgnore = attacker->GetAscensionConditionalCombatModifier(victim, spellInfo, ASCENSION_CONDITIONAL_IGNORE_ARMOR);\n"
        "        armor = std::floor(CalculatePct(armor, 100 - std::clamp(conditionalIgnore, 0, 100)));\n\n"
        "        // Apply Player CR_ARMOR_PENETRATION rating and buffs from stances\\specializations etc.")
replace("Entities/Player/Player.h", "    PlayerSpellMap m_spells;",
        "    std::unordered_map<uint32, uint32> m_temporarySpellReplacements;\n"
        "    std::unordered_map<uint32, uint32> m_temporarySpellReplacementOrigins;\n"
        "    std::unordered_map<uint32, uint8> m_temporarySpellReplacementKeepsCast;\n    PlayerSpellMap m_spells;")
replace("Entities/Player/Player.h", "    void SendLearnPacket(uint32 spellId, bool learn);",
        "    void SetTemporarySpellReplacement(uint32 original, uint32 replacement, bool redirect = true);\n"
        "    uint32 GetTemporarySpellReplacement(uint32 original) const;\n"
        "    uint32 GetCastReplacement(uint32 original) const;\n"
        "    bool IsTemporarySpellReplacementStandIn(uint32 spellId) const;\n"
        "    uint32 GetSavedActionButtonSpell(uint32 action);\n"
        "    void SendLearnPacket(uint32 spellId, bool learn);")
replacement = section("Entities/Player/Player.cpp", "void Player::SetTemporarySpellReplacement", "bool Player::CanUseTwoHandWithShield")
replacement = "\n".join(line for line in replacement.split("\n") if
    "sScriptMgr->OnPlayerTemporarySpell" not in line and "sScriptMgr->OnPlayerNormalizeActionButtonSpell" not in line)
replacement = replacement.replace("SendLearnPacket(replacement, false, true)", "SendLearnPacket(replacement, false)")
replace("Entities/Player/Player.cpp", "void Player::SendLearnPacket(", replacement + "void Player::SendLearnPacket(")
replace("Entities/Player/PlayerStorage.cpp", "        switch (itr->second.uState)\n        {\n            case ACTIONBUTTON_NEW:",
        "        uint32 action = itr->second.GetAction();\n"
        "        if (itr->second.GetType() == ACTION_BUTTON_SPELL)\n"
        "            action = GetSavedActionButtonSpell(action);\n"
        "        switch (itr->second.uState)\n        {\n            case ACTIONBUTTON_NEW:")
replace("Entities/Player/PlayerStorage.cpp", "stmt->SetData(3, itr->second.GetAction());", "stmt->SetData(3, action);")
replace("Entities/Player/PlayerStorage.cpp", "stmt->SetData(0, itr->second.GetAction());", "stmt->SetData(0, action);")
replace("Handlers/SpellHandler.cpp", "    uint32 oldSpellId = spellId;",
        "    uint32 oldSpellId = spellId;\n"
        "    if (_player->m_mover == _player)\n        spellId = _player->GetCastReplacement(spellId);")
for path, text in edits.items():
    path.write_text(text, encoding="utf-8", newline="\n")
print(f"Installed combat contracts into {len(edits)} core files")
