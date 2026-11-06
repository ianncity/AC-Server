"""Materialize pinned class SQL in a separate reference database for extraction.

The destination schema is deliberately fixed and CREATE DATABASE must succeed;
this tool cannot import into, replace, or reset a gameplay database.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from zipfile import ZipFile, ZIP_DEFLATED

SCHEMA = "advai_ascension_reference_v2"
MIGRATIONS = (
    "rev_20260903_01_live_class_baseline.sql",
    "rev_20260909_01_necromancer_completion.sql",
    "rev_20260909_02_necromancer_runtime.sql",
    "rev_20260909_03_necromancer_summon_sizes.sql",
    "rev_20260909_12_cultist_completion.sql",
    "rev_20260919_30_necromancer_skeletal_smith_vendor.sql",
    "rev_20260920_01_cultist_sanity_tap_missing_mana.sql",
    "rev_20260922_03_cultist_void_enchanted_armor.sql",
    "rev_20260922_03_cultist_worrysome_idol_missing_mana.sql",
    "rev_20260922_11_cultist_shadow_training_spell_script.sql",
    "rev_20260927_62_necromancer_transfer_life.sql",
    "rev_20261004_05_necromancer_transfer_life_upkeep.sql",
    "rev_20261007_11_skeletal_warrior_strike.sql",
    "rev_20261013_02_necromancer_skeleton_weapons.sql",
)


def prepare(reference: Path, mysql: Path, port: int, output: Path):
    if output.exists():
        raise ValueError(f"Refusing to replace {output}")
    snapshot = reference / "data/coa-world/coa-world-20260912.zip"
    pending = reference / "data/sql/updates/pending_db_world"
    migrations = [(name, (pending / name).read_bytes()) for name in MIGRATIONS]
    tables = {"creature_template", "creature_template_model", "creature_model_info", "pet_levelstats",
              "spell_script_names", "spell_bonus_data", "spell_proc", "spell_ranks", "npc_vendor",
              "playercreateinfo_spell_custom", "item_template", "gameobject_template", "spell_group"}
    temporary_tables = set()
    for name, data in migrations:
        sql = data.decode("utf-8")
        if re.search(r"^\s*(?:USE\s+|CREATE\s+DATABASE\s+|DROP\s+DATABASE\s+)", sql, re.I | re.M):
            raise ValueError(f"Migration changes database scope: {name}")
        tables.update(re.findall(r"(?:FROM|INTO|UPDATE|TABLE|JOIN)\s+`([^`]+)`", sql, re.I))
        tables.update(re.findall(r"\bLIKE\s+`([^`]+)`", sql, re.I))
        temporary_tables.update(re.findall(r"CREATE\s+TEMPORARY\s+TABLE\s+`([^`]+)`", sql, re.I))
    tables -= temporary_tables
    with ZipFile(snapshot) as archive:
        for table in tables:
            archive.getinfo(table + ".sql")
    connection = ["--no-defaults", "--host=127.0.0.1", f"--port={port}", "--user=root",
                  "--default-character-set=utf8mb4"]
    def execute(data, database=False):
        result = subprocess.run([str(mysql), *connection, *([SCHEMA] if database else [])],
                                input=data, capture_output=True)
        if result.returncode:
            raise ValueError(result.stderr.decode("utf-8", errors="replace")[:4000])
    # A previous reference is never implicitly overwritten or reused.
    execute(f"CREATE DATABASE `{SCHEMA}` CHARACTER SET utf8mb4;".encode())
    with ZipFile(snapshot) as archive:
        payload = [b"SET FOREIGN_KEY_CHECKS=0; SET autocommit=0;"]
        for table in sorted(tables):
            payload.append(archive.read(table + ".sql"))
        execute(b"\n".join(payload) + b"\nCOMMIT;", True)
    for name, sql in migrations:
        execute(sql, True)
    dump = mysql.with_name("mysqldump.exe")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".tmp")
    with ZipFile(temporary, "w", ZIP_DEFLATED) as archive:
        for table in sorted(tables):
            result = subprocess.run([str(dump), *connection, "--skip-comments", "--skip-add-drop-table",
                                     "--skip-lock-tables", "--skip-add-locks", "--skip-disable-keys",
                                     "--no-tablespaces", SCHEMA, table], capture_output=True)
            if result.returncode:
                raise ValueError(result.stderr.decode("utf-8", errors="replace")[:4000])
            archive.writestr(table + ".sql", result.stdout)
    temporary.replace(output)
    manifest = {"schema": SCHEMA, "revision": "0487265f863b9c50e7757e1f113169b80505ba8b",
                "base_sha256": hashlib.sha256(snapshot.read_bytes()).hexdigest(),
                "migrations": {name: hashlib.sha256(sql).hexdigest() for name, sql in migrations},
                "tables": sorted(tables), "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
    output.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--mysql", type=Path, required=True)
    parser.add_argument("--port", type=int, default=13316)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.reference, args.mysql, args.port, args.output)))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"ADVAI CLASS REFERENCE FAILURE: {error}\n")
