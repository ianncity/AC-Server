"""Extract only class-owned world data from the pinned Ascension SQL snapshot.

Never execute the snapshot against an existing realm: it contains whole tables.
This compiler preserves SQL literals, selects dependency rows, and records hashes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
from zipfile import ZipFile

from backports import DBC


def split_values(text: str, tuples: bool = False) -> list[str]:
    result, start, depth, quote, escape = [], 0, 0, False, False
    for index, char in enumerate(text):
        if quote:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == "'":
                quote = False
            continue
        if char == "'":
            quote = True
        elif char == "(":
            if tuples and depth == 0:
                start = index + 1
            depth += 1
        elif char == ")":
            depth -= 1
            if tuples and depth == 0:
                result.append(text[start:index])
        elif char == "," and not tuples and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
        if depth < 0:
            raise ValueError("Unbalanced SQL tuple")
    if quote or escape or depth:
        raise ValueError("Truncated SQL literal or tuple")
    if not tuples:
        result.append(text[start:].strip())
    return result


def remap_display_ids(sql: str, display_map: dict[int, int], fallback_ids: set[int]) -> str:
    """Rewrite class world display references to the client DBC IDs we actually ship."""
    definitions = (
        ("creature_template_model", "CreatureDisplayID", False),
        ("creature_model_info", "DisplayID", True),
    )
    for table_name, column_name, omit_fallback_rows in definitions:
        pattern = re.compile(
            rf"(INSERT INTO `{table_name}` \(([^)]*)\) VALUES\n)(.*?)(\nON DUPLICATE KEY UPDATE )",
            re.DOTALL,
        )
        match = pattern.search(sql)
        if not match:
            continue
        columns = re.findall(r"`([^`]+)`", match.group(2))
        if column_name not in columns:
            raise ValueError(f"Missing {column_name} column in {table_name} SQL")
        column = columns.index(column_name)
        rows = []
        for record in split_values(match.group(3), tuples=True):
            values = split_values(record)
            if len(values) != len(columns):
                raise ValueError(f"Invalid {table_name} SQL row width")
            source_id = int(values[column].strip("'"))
            if omit_fallback_rows and source_id in fallback_ids:
                continue
            target_id = display_map.get(source_id, source_id)
            values[column] = str(target_id)
            rows.append("(" + ",".join(values) + ")")
        if not rows:
            raise ValueError(f"Display remap removed every {table_name} row")
        replacement = match.group(1) + ",\n".join(rows) + match.group(4)
        sql = sql[:match.start()] + replacement + sql[match.end():]
    return sql


def ensure_preceding_deletes(sql: str) -> str:
    """Make the selected class rows safely re-applicable before each insert."""
    keys = {
        "creature_template": ("entry",),
        "creature_template_model": ("CreatureID", "Idx"),
        "spell_script_names": ("spell_id", "ScriptName"),
        "spell_bonus_data": ("entry",),
        "spell_proc": ("SpellId",),
        "spell_ranks": ("first_spell_id", "spell_id", "rank"),
        "creature_model_info": ("DisplayID",),
    }
    pattern = re.compile(
        r"INSERT INTO `([^`]+)` \(([^)]*)\) VALUES\n(.*?)\nON DUPLICATE KEY UPDATE .*?;",
        re.DOTALL,
    )

    def replace(match: re.Match) -> str:
        table_name = match.group(1)
        if table_name not in keys:
            raise ValueError(f"No class SQL key is defined for {table_name}")
        columns = re.findall(r"`([^`]+)`", match.group(2))
        key_columns = keys[table_name]
        if any(column not in columns for column in key_columns):
            raise ValueError(f"Missing class SQL key columns for {table_name}")
        indices = [columns.index(column) for column in key_columns]
        rows = [split_values(record) for record in split_values(match.group(3), tuples=True)]
        if not rows or any(len(row) != len(columns) for row in rows):
            raise ValueError(f"Invalid class SQL row width for {table_name}")
        unique_keys = list(dict.fromkeys(tuple(row[index] for index in indices) for row in rows))
        rendered_columns = ",".join(f"`{column}`" for column in key_columns)
        rendered_values = ",".join("(" + ",".join(row) + ")" for row in unique_keys)
        if len(key_columns) == 1:
            delete = f"DELETE FROM `{table_name}` WHERE {rendered_columns} IN ({','.join(row[0] for row in unique_keys)});"
        else:
            delete = f"DELETE FROM `{table_name}` WHERE ({rendered_columns}) IN ({rendered_values});"
        if sql[:match.start()].rstrip().endswith(delete):
            return match.group(0)
        return delete + "\n" + match.group(0)

    return pattern.sub(replace, sql)


def table(archive: ZipFile, name: str) -> tuple[list[str], list[list[str]]]:
    text = archive.read(f"{name}.sql").decode("utf-8")
    definition = text.split("INSERT INTO", 1)[0]
    columns = re.findall(r"^\s+`([^`]+)`\s", definition, re.MULTILINE)
    if not columns:
        raise ValueError(f"Missing SQL schema for {name}")
    rows = []
    for match in re.finditer(r"INSERT INTO `" + re.escape(name) + r"` VALUES (.*);\s*$", text, re.MULTILINE):
        rows.extend(split_values(record) for record in split_values(match[1], tuples=True))
    if not rows or any(len(row) != len(columns) for row in rows):
        raise ValueError(f"Invalid SQL snapshot row width for {name}")
    return columns, rows


def compile_world(source: Path, snapshot: Path, output: Path, code: Path) -> dict:
    if output.exists():
        raise ValueError(f"Refusing to replace an existing class world bundle: {output}")
    spell_table = DBC(source / "Spell.dbc")
    spell_rows = [spell_table.words(row) for row in spell_table.records]
    spells = {row[0] for row in spell_rows}
    creatures = set()
    for row in spell_rows:
        for index in range(3):
            if row[71 + index] in (28, 56) and row[110 + index]:
                creatures.add(row[110 + index])
    raw_dbc_creatures = creatures
    creatures = set()
    scripts = "\n".join(path.read_text(encoding="utf-8") for path in sorted(code.glob("Ascension*")))
    # Summon contracts replace several original DBC effects. Include their authored entries.
    summon_header = (code / "AscensionNecromancerData.h").read_text(encoding="utf-8")
    block = summon_header.split("NecromancerSummons[] =", 1)[1].split("};", 1)[0]
    creatures.update(int(value) for value in re.findall(r"\{\s*\d+,\s*\d+,\s*(\d+),", block))
    cultist_summons = (code / "AscensionCultistSummons.cpp").read_text(encoding="utf-8")
    creatures.update(int(value) for value in re.findall(r"\bSummon\(player,\s*(\d+)", cultist_summons))
    sql_tables = {}
    with ZipFile(snapshot) as archive:
        cols, rows = table(archive, "creature_template")
        entry_col, script_col = cols.index("entry"), cols.index("ScriptName")
        for row in rows:
            if row[script_col].startswith(("'npc_ascension_cultist", "'npc_ascension_necromancer")):
                creatures.add(int(row[entry_col]))
        selected = [row for row in rows if int(row[entry_col]) in creatures]
        missing = creatures - {int(row[entry_col]) for row in selected}
        if missing:
            raise ValueError(f"Missing required class creature templates: {sorted(missing)}")
        sql_tables["creature_template"] = (cols, selected)
        for name, column, identities in (
            ("creature_template_model", "CreatureID", creatures),
            ("pet_levelstats", "creature_entry", creatures),
            ("spell_script_names", "spell_id", spells),
            ("spell_bonus_data", "entry", spells),
            ("spell_proc", "SpellId", spells),
            ("spell_ranks", "spell_id", spells),
            ("npc_vendor", "entry", creatures),
        ):
            cols, rows = table(archive, name)
            index = cols.index(column)
            selected = [row for row in rows if abs(int(row[index])) in identities]
            if name == "spell_script_names":
                # Every selected binding must exist in the integrated class source.
                # The SQL snapshot predates the client and also contains reused IDs.
                # Bind only the two integrated classes, never another class's old hook.
                selected = [row for row in selected if any(token in row[cols.index("ScriptName")]
                            for token in ("ascension_cultist", "ascension_necromancer"))]
                unknown = sorted({row[cols.index("ScriptName")].strip("'") for row in selected
                                  if row[cols.index("ScriptName")].strip("'") not in scripts})
                if unknown:
                    raise ValueError(f"Class spell bindings require unported scripts: {unknown}")
            sql_tables[name] = (cols, selected)
        model_ids = {int(row[sql_tables["creature_template_model"][0].index("CreatureDisplayID")])
                     for row in sql_tables["creature_template_model"][1]}
        cols, rows = table(archive, "creature_model_info")
        sql_tables["creature_model_info"] = (cols, [row for row in rows if int(row[cols.index("DisplayID")]) in model_ids])
        cols, rows = table(archive, "playercreateinfo_spell_custom")
        starter = {}
        for identity in (23, 25):
            mask = 1 << (identity - 1)
            starter[str(identity)] = sorted({int(row[cols.index("Spell")]) for row in rows
                if int(row[cols.index("classmask")]) == mask and int(row[cols.index("racemask")]) == 0})
            if not starter[str(identity)]:
                raise ValueError(f"Missing class {identity} initial spells")
    output.mkdir(parents=True)
    creature_columns, creature_rows = sql_tables["creature_template"]
    entry_index, name_index = creature_columns.index("entry"), creature_columns.index("name")
    collisions = " OR ".join(f"(`entry`={row[entry_index]} AND `name`<>{row[name_index]})"
                             for row in creature_rows)
    generated = ["-- Generated class dependency subset; full snapshot tables must never be imported.",
                 "-- Source: jealous-sound/azerothcore-wotlk-coa @ 0487265f863b9c50e7757e1f113169b80505ba8b.",
                 "DELIMITER $$", "DROP PROCEDURE IF EXISTS `advai_assert_class_ids`$$",
                 "CREATE PROCEDURE `advai_assert_class_ids`() BEGIN",
                 "IF EXISTS (SELECT 1 FROM `creature_template` WHERE " + collisions + ") THEN",
                 "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='ADVAI Ascension creature ID conflicts with existing world content';",
                 "END IF; END$$", "CALL `advai_assert_class_ids`()$$",
                 "DROP PROCEDURE `advai_assert_class_ids`$$", "DELIMITER ;", "START TRANSACTION;"]
    for name, (columns, rows) in sql_tables.items():
        if not rows:
            continue
        names = ",".join(f"`{column}`" for column in columns)
        # Native SQL errors (including incompatible schemas) remain explicit.
        generated.append(f"INSERT INTO `{name}` ({names}) VALUES")
        generated.append(",\n".join("(" + ",".join(row) + ")" for row in rows))
        generated.append("ON DUPLICATE KEY UPDATE " + ",".join(
            f"`{column}`=VALUES(`{column}`)" for column in columns[1:]) + ";")
    generated += ["COMMIT;", ""]
    (output / "class-world.sql").write_text("\n".join(generated), encoding="utf-8", newline="\n")
    manifest = {"snapshot_sha256": hashlib.sha256(snapshot.read_bytes()).hexdigest(),
                "tables": {name: len(rows) for name, (_, rows) in sql_tables.items()},
                "creatures": sorted(creatures), "displays": sorted(model_ids), "initial_spells": starter,
                "raw_dbc_summons_requiring_contract_review": sorted(raw_dbc_creatures - creatures)}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--snapshot", required=True, type=Path)
    parser.add_argument("--code", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(compile_world(args.source, args.snapshot, args.output, args.code)))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"ADVAI CLASS WORLD FAILURE: {error}\n")
