"""Compile the installed client's Cultist/Necromancer advancement and budgets.

Record positions follow AscensionCoATalentData.cpp in
jealous-sound/azerothcore-wotlk-coa at 0487265f863b9c50e7757e1f113169b80505ba8b.
The client DBCs are authoritative: older addon/Content JSON entry IDs are reused
by newer trees and cannot safely identify a talent or its spell.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from backports import DBC


CLASS_IDS = {23: "Necromancer", 25: "Cultist"}
ROLES = {"DEATH": "caster", "ANIMATION": "summoner", "RIME": "caster",
         "INFLUENCE": "healer", "CORRUPTION": "caster", "GODBLADE": "melee", "BULWARK": "tank"}


def records(table: DBC, minimum: int) -> list[tuple[int, ...]]:
    if table.size < minimum * 4 or table.size % 4:
        raise ValueError(f"Incompatible packed DBC record size: {table.size}, needs {minimum} DWORDs")
    # The custom header's field count includes packed fields. Used columns precede those fields.
    return [struct.unpack(f"<{table.size // 4}I", record) for record in table.records]


def literal(value) -> str:
    if isinstance(value, str):
        return "'" + value.replace("\\", "\\\\").replace("'", "''") + "'"
    return str(value)


def compile_catalog(source: Path, output: Path, json_output: Path | None = None,
                    cpp_output: Path | None = None) -> dict:
    source = source.resolve(strict=True)
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    for name, expected in manifest["sha256"].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Class source changed after extraction: {name}")
    tables = {name: DBC(source / f"{name}.dbc") for name in
              ("ChrClasses", "ChrSpecs", "CharacterAdvancement", "CharacterAdvancementClassTypes",
               "CharacterAdvancementTabTypes", "CharacterAdvancementEssence", "Spell")}
    classes = tables["ChrClasses"]
    tokens = {row[0]: classes.text(row[55]).upper() for row in records(classes, 56)}
    for identity, name in CLASS_IDS.items():
        if tokens.get(identity) != name.upper():
            raise ValueError(f"The client class identity {identity} does not identify {name}")
    class_types = {row[0]: row for row in records(tables["CharacterAdvancementClassTypes"], 5)}
    tab_table = tables["CharacterAdvancementTabTypes"]
    tabs = {row[0]: tab_table.text(row[1]).upper() for row in records(tab_table, 2)}
    specs = []
    spec_table = tables["ChrSpecs"]
    for row in records(spec_table, 29):
        class_token = spec_table.text(row[1]).upper()
        matching = [identity for identity in CLASS_IDS if tokens[identity] == class_token]
        if not matching:
            continue
        tab = spec_table.text(row[2]).upper()
        if tab not in ROLES:
            raise ValueError(f"Unmapped class specialization role: {class_token}/{tab}")
        specs.append({"id": row[0], "class": matching[0], "tab": tab, "role": ROLES[tab],
                      "identity": row[28], "signature_spell": row[24],
                      "primary_stats": [spec_table.text(offset) for offset in row[8:11]]})
    by_tab = {(tokens[spec["class"]], spec["tab"]): spec["id"] for spec in specs}
    spell_table = tables["Spell"]
    spells = {row[0]: row for row in records(spell_table, 234)}
    nodes = []
    for row in records(tables["CharacterAdvancement"], 74):
        class_type = class_types.get(row[32])
        if not class_type or not class_type[4] or class_type[2] not in CLASS_IDS:
            continue
        class_id = class_type[2]
        tab = tabs.get(row[33])
        if not tab:
            raise ValueError(f"Unknown tab {row[33]} for class node {row[0]}")
        spec = 0 if row[33] == 87 else by_tab.get((tokens[class_id], tab))
        if spec is None:
            raise ValueError(f"Class node {row[0]} has no matching specialization")
        ranks = [spell for spell in row[5:10] if spell]
        if not ranks or len(ranks) != len(set(ranks)):
            raise ValueError(f"Class node {row[0]} has an empty or duplicate rank definition")
        missing = set(ranks) - spells.keys()
        if missing:
            raise ValueError(f"Class node {row[0]} lacks spells in the source snapshot: {sorted(missing)}")
        node = {"id": row[0], "class": class_id, "spec": spec, "tab": tab,
                "name": spell_table.text(spells[ranks[-1]][136]), "spells": ranks,
                "required": [identity for identity in row[2:5] if identity],
                "ae": row[14], "te": row[15], "level": row[26], "group": row[29], "flags": row[73]}
        if not node["name"] or max(node["ae"], node["te"]) > 255 or node["level"] > 80:
            raise ValueError(f"Invalid class node cost/level/name: {row[0]}")
        nodes.append(node)
    ids = {node["id"] for node in nodes}
    if len(ids) != len(nodes):
        raise ValueError("Class advancement contains duplicate node identities")
    for node in nodes:
        if any(identity not in ids for identity in node["required"]):
            raise ValueError(f"Class node {node['id']} refers to an unavailable prerequisite")
    for spec in specs:
        if spec["identity"] not in ids:
            raise ValueError(f"Specialization {spec['id']} lacks its identity node")
        if not any(spec["signature_spell"] in node["spells"] for node in nodes if node["class"] == spec["class"]):
            raise ValueError(f"Specialization {spec['id']} lacks its signature ability")
    budgets = []
    for row in records(tables["CharacterAdvancementEssence"], 9):
        if row[2] in CLASS_IDS and 1 <= row[1] <= 80 and not any(row[3:7]):
            budgets.append({"class": row[2], "level": row[1], "ae": row[7], "te": row[8]})
    budget_keys = {(row["class"], row["level"]) for row in budgets}
    if len(budget_keys) != len(budgets) or len(budget_keys) != 160:
        raise ValueError("Class advancement must supply exactly one real budget per class at every level 1-80")
    nodes.sort(key=lambda value: value["id"])
    specs.sort(key=lambda value: value["id"])
    budgets.sort(key=lambda value: (value["class"], value["level"]))
    catalog = {"format": 1, "nodes": nodes, "specs": specs, "budgets": budgets}
    signature = hashlib.sha256(json.dumps(catalog, sort_keys=True).encode("utf-8")).hexdigest()
    catalog["sha256"] = signature
    sql = ["-- ADVAI class advancement extracted from the installed Ascension client's DBCs.",
           f"-- Catalog SHA-256: {signature}", "START TRANSACTION;",
           "CREATE TABLE IF NOT EXISTS `playerbots_advai_class_catalog` (",
           "  `id` TINYINT UNSIGNED NOT NULL PRIMARY KEY, `sha256` CHAR(64) NOT NULL,",
           "  `nodes` INT UNSIGNED NOT NULL, `specs` INT UNSIGNED NOT NULL, `budgets` INT UNSIGNED NOT NULL",
           ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
           "CREATE TABLE IF NOT EXISTS `playerbots_advai_class_node` (",
           "  `entry` INT UNSIGNED NOT NULL PRIMARY KEY, `class_id` TINYINT UNSIGNED NOT NULL,",
           "  `spec_id` SMALLINT UNSIGNED NOT NULL, `name` VARCHAR(128) NOT NULL,",
           "  `level` TINYINT UNSIGNED NOT NULL, `ae` TINYINT UNSIGNED NOT NULL, `te` TINYINT UNSIGNED NOT NULL,",
           "  `choice_group` INT UNSIGNED NOT NULL, `flags` INT UNSIGNED NOT NULL,",
           "  `required_ids` VARCHAR(64) NOT NULL, `spells` VARCHAR(96) NOT NULL,",
           "  KEY `class_spec` (`class_id`,`spec_id`)", ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
           "CREATE TABLE IF NOT EXISTS `playerbots_advai_class_spec` (",
           "  `spec_id` SMALLINT UNSIGNED NOT NULL PRIMARY KEY, `class_id` TINYINT UNSIGNED NOT NULL,",
           "  `name` VARCHAR(32) NOT NULL, `role` VARCHAR(16) NOT NULL,",
           "  `identity_entry` INT UNSIGNED NOT NULL, `signature_spell` INT UNSIGNED NOT NULL,",
           "  `primary_stats` VARCHAR(64) NOT NULL", ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
           "CREATE TABLE IF NOT EXISTS `playerbots_advai_class_budget` (",
           "  `class_id` TINYINT UNSIGNED NOT NULL, `level` TINYINT UNSIGNED NOT NULL,",
           "  `ae` SMALLINT UNSIGNED NOT NULL, `te` SMALLINT UNSIGNED NOT NULL,",
           "  PRIMARY KEY (`class_id`,`level`)", ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",
           "DELETE FROM `playerbots_advai_class_catalog` WHERE `id`=1;",
           "INSERT INTO `playerbots_advai_class_catalog` VALUES "
           f"(1,'{signature}',{len(nodes)},{len(specs)},{len(budgets)});",
           "DELETE FROM `playerbots_advai_class_node`;",
           "INSERT INTO `playerbots_advai_class_node` VALUES"]
    sql.append(",\n".join("(" + ",".join(literal(value) for value in
               (node["id"], node["class"], node["spec"], node["name"], node["level"], node["ae"], node["te"],
                node["group"], node["flags"], " ".join(map(str, node["required"])),
                " ".join(map(str, node["spells"])))) + ")" for node in nodes) + ";")
    sql += ["DELETE FROM `playerbots_advai_class_spec`;", "INSERT INTO `playerbots_advai_class_spec` VALUES"]
    sql.append(",\n".join("(" + ",".join(literal(value) for value in
               (spec["id"], spec["class"], spec["tab"], spec["role"], spec["identity"], spec["signature_spell"],
                " ".join(spec["primary_stats"]))) + ")" for spec in specs) + ";")
    sql += ["DELETE FROM `playerbots_advai_class_budget`;", "INSERT INTO `playerbots_advai_class_budget` VALUES"]
    sql.append(",\n".join(f"({row['class']},{row['level']},{row['ae']},{row['te']})" for row in budgets) + ";")
    sql += ["COMMIT;", ""]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(sql), encoding="utf-8", newline="\n")
    if json_output:
        json_output.parent.mkdir(parents=True, exist_ok=True)
        json_output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if cpp_output:
        categories = DBC(source / "SpellChargesCategory.dbc")
        category_by_id = {row[0]: row for row in records(categories, 3)}
        links = DBC(source / "SpellCharges.dbc")
        rank_data = json.loads((source / "ranks.json").read_text(encoding="utf-8"))
        roots = {rank["spellId"]: rank["firstSpellId"] for rank in rank_data}
        charges = []
        for row in records(links, 2):
            if row[0] not in spells:
                continue
            category = category_by_id.get(row[1])
            if not category or not 1 <= category[1] <= 20 or not 1 <= category[2] <= 86400000:
                raise ValueError(f"Invalid charge category for class spell {row[0]}")
            root = roots.get(row[0], row[0])
            if root not in spells:
                raise ValueError(f"Missing charge rank-family root {root}")
            charges.append((row[0], category[1], category[2], root, row[1]))
        charges.sort()
        header = ["// Generated from the installed Ascension class snapshot; do not hand edit.",
                  "#ifndef PLAYERBOTS_ADVAI_ASCENSION_DATA_H", "#define PLAYERBOTS_ADVAI_ASCENSION_DATA_H",
                  "#include <array>", "#include <cstdint>", "namespace Advai::Ascension::Data", "{",
                  f'inline constexpr char CatalogHash[] = "{signature}";',
                  f"inline constexpr std::uint32_t NodeCount = {len(nodes)};",
                  "struct Charge { std::uint32_t Spell, Maximum, Recovery, Root, Category; };",
                  f"inline constexpr std::array<Charge, {len(charges)}> Charges = {{{{"]
        header += ["    {" + ", ".join(map(str, row)) + "}," for row in charges]
        header += ["}};", f"inline constexpr std::array<std::uint32_t, {len(spells)}> Spells = {{{{"]
        ordered = sorted(spells)
        header += ["    " + ", ".join(map(str, ordered[start:start + 12])) + ","
                   for start in range(0, len(ordered), 12)]
        header += ["}};",
                   "struct Node { std::uint32_t Id, Class, Spec, Level, AE, TE, Group;",
                   "    std::array<std::uint32_t, 3> Required; std::array<std::uint32_t, 5> Ranks; };",
                   f"inline constexpr std::array<Node, {len(nodes)}> Nodes = {{{{"]
        for node in nodes:
            values = [node[key] for key in ("id", "class", "spec", "level", "ae", "te", "group")]
            required = node["required"] + [0] * (3 - len(node["required"]))
            header.append("    {" + ", ".join(map(str, values)) + ", {{" +
                          ", ".join(map(str, required)) + "}}, {{" +
                          ", ".join(map(str, node["spells"] + [0] * (5 - len(node["spells"])))) + "}}},")
        header += ["}};", "struct Spec { std::uint32_t Id, Class, Identity, Signature; };",
                   f"inline constexpr std::array<Spec, {len(specs)}> Specs = {{{{"]
        header += ["    {" + ", ".join(map(str, [spec[key] for key in
                   ("id", "class", "identity", "signature_spell")])) + "}," for spec in specs]
        header += ["}};", "struct Budget { std::uint32_t Class, Level, AE, TE; };",
                   f"inline constexpr std::array<Budget, {len(budgets)}> Budgets = {{{{"]
        header += ["    {" + ", ".join(map(str, [budget[key] for key in ("class", "level", "ae", "te")])) + "},"
                   for budget in budgets]
        header += ["}};", "}", "#endif", ""]
        cpp_output.parent.mkdir(parents=True, exist_ok=True)
        cpp_output.write_text("\n".join(header), encoding="utf-8", newline="\n")
    return {"nodes": dict(Counter(CLASS_IDS[node["class"]] for node in nodes)),
            "specs": len(specs), "budgets": len(budgets), "sha256": signature}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--cpp-output", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(compile_catalog(args.source, args.output, args.json_output, args.cpp_output)))
    except (ValueError, OSError, KeyError, struct.error) as error:
        parser.exit(1, f"ADVAI CLASS CATALOG FAILURE: {error}\n")
