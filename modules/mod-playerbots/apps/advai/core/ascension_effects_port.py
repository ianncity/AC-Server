"""Port the pinned CoA extended-effect contracts into the supported core.

Preflight every edit before writing. Preserve existing core behavior and reject
source drift. The class-dependent spell corrections are installed separately.
"""

import argparse
from pathlib import Path


def function(text: str, signature: str) -> str:
    start = text.index(signature)
    position = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[position] == "{") - (text[position] == "}")
        position += 1
    return text[start:position] + "\n"


def port(core: Path, upstream: Path) -> None:
    changed = {}

    def read(path):
        return changed.get(path, (core / path).read_text(encoding="utf-8"))

    def replace(path, old, new):
        text = read(path)
        if text.count(old) != 1:
            raise ValueError(f"Missing or ambiguous supported-core anchor in {path}: {old[:70]}")
        changed[path] = text.replace(old, new)

    source = (upstream / "src/server/game/Spells/SpellEffects.cpp").read_text(encoding="utf-8")
    path = "src/server/shared/SharedDefines.h"
    shared = (upstream / path).read_text(encoding="utf-8")
    start = shared.index("    SPELL_EFFECT_ASCENSION_FIRST")
    end = shared.index("    TOTAL_SPELL_EFFECTS", start)
    replace(path, "    TOTAL_SPELL_EFFECTS                             = 165",
            shared[start:end] + "    TOTAL_SPELL_EFFECTS                             = 199")
    handlers = {165: "EffectAscensionModifyCooldown", 166: "EffectAscensionRestoreBaseManaPct",
                168: "EffectAscensionPlaySpellVisualKit", 173: "EffectAscensionRefreshAura",
                174: "EffectAscensionPctDamage", 175: "EffectAscensionModifyAuraStacks",
                176: "EffectAscensionModifyAuraStacksBySpell", 177: "EffectAscensionModifyAuraDuration",
                181: "EffectAscensionRestoreBaseHealthPct", 183: "EffectAscensionTriggerSpellDelayed",
                187: "EffectAscensionRestoreSpellCharges", 190: "EffectApplyAreaAura",
                193: "EffectDelayedTriggerSpell", 195: "EffectAscensionResetCooldown"}
    path = "src/server/game/Spells/SpellEffects.cpp"
    anchor = "    &Spell::EffectRemoveAura,                               //164 SPELL_EFFECT_REMOVE_AURA"
    entries = "\n".join(f"    &Spell::{handlers.get(index, 'EffectAdvaiUnsupported')}, // {index} Ascension"
                        for index in range(165, 199))
    replace(path, anchor, anchor + "\n" + entries)
    methods = sorted(set(handlers.values()) - {"EffectApplyAreaAura", "EffectAscensionTriggerSpellDelayed",
                                              "EffectDelayedTriggerSpell"})
    methods.append("EffectAscensionHealthPctDamage")
    declarations = methods + ["EffectAdvaiUnsupported", "EffectAscensionTriggerSpellDelayed", "EffectDelayedTriggerSpell"]
    replace("src/server/game/Spells/Spell.h", "    void EffectNULL(SpellEffIndex effIndex);",
            "".join(f"    void {name}(SpellEffIndex effIndex);\n" for name in declarations)
            + "    void ScheduleAdvaiTrigger(SpellEffIndex index, uint32 delay);\n"
            + "    void EffectNULL(SpellEffIndex effIndex);")
    helpers = function(source, "void ResetAscensionCooldown(") + function(source, "void ModifyAscensionCooldown(")
    helpers += """
void ModifyAscensionAuraStacks(Unit* caster, Unit* target, uint32 spell, int32 delta)
{
    if (!caster || !target || !spell || !delta)
        return;
    if (!sSpellMgr->GetSpellInfo(spell))
        throw std::runtime_error("Missing required resource aura " + std::to_string(spell));
    if (Aura* aura = target->GetAura(spell))
        aura->ModStackAmount(delta);
    else if (delta > 0)
        if (Aura* aura = caster->AddAura(spell, target))
            aura->SetStackAmount(uint8(std::min(delta, 255)));
}
"""
    extra = ("\n// Adapted from jealous-sound/azerothcore-wotlk-coa at 0487265f; GNU AGPL v3.\n"
             + "namespace\n{\n" + helpers + "}\n")
    for name in methods:
        body = function(source, "void Spell::" + name + "(")
        if "m_caster" in body:
            body = body.replace("m_caster", "classCaster")
            opening = body.index("{") + 1
            body = (body[:opening] + "\n    Unit* classCaster = m_caster->ToUnit();\n"
                    "    if (!classCaster)\n        return;\n" + body[opening:])
        body = body.replace("!unitTarget->HasActivePowerType(POWER_MANA)", "!unitTarget->GetMaxPower(POWER_MANA)")
        extra += "\n" + body
    extra += Path(__file__).with_name("ascension_delayed_effects.inc").read_text(encoding="utf-8")
    replace(path, '#include "Spell.h"', '#include "Spell.h"\n#include "MapMgr.h"\n#include <stdexcept>')
    changed[path] = read(path) + extra
    path = "src/server/game/Spells/SpellInfo.cpp"
    text = (upstream / path).read_text(encoding="utf-8")
    start = text.index("    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 165")
    end = text.index("\n} };", start)
    anchor = "    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 164 SPELL_EFFECT_REMOVE_AURA"
    replace(path, anchor, anchor + "\n" + text[start:end])
    for path, text in changed.items():
        (core / path).write_text(text, encoding="utf-8", newline="\n")
    print(f"Ported extended effect contracts into {len(changed)} supported-core files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", required=True, type=Path)
    parser.add_argument("--upstream", required=True, type=Path)
    args = parser.parse_args()
    try:
        port(args.core.resolve(strict=True), args.upstream.resolve(strict=True))
    except (OSError, ValueError) as error:
        parser.exit(1, f"ADVAI CORE EFFECT PORT FAILURE: {error}\n")
