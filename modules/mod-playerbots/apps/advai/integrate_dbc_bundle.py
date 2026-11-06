"""Merge the authored Dracthyr DBC rows into the active class-enabled server DBC set."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

from necromancer_spell_visuals import integrate_necromancer_spell_visuals


RACES = (12, 13)
RACE_CLASSES = (1, 3, 4, 5, 6, 8, 9, 12, 13)
FORBIDDEN_COMBINATIONS = {
    (2, 12),  # Orc Cultists are not playable.
    (3, 12), (3, 13),  # Dwarves are not eligible for either class.
    (10, 12), (10, 13),  # Blood Elves are not eligible for either class.
    (11, 12), (11, 13),  # Draenei are not eligible for either class.
}
SPELL_IDS = (363402, 363403, 363404, 363405, 363406)
MODEL_IDS = tuple(range(10000, 10006))
DISPLAY_IDS = tuple(range(50000, 50006))


class DBC:
    def __init__(self, path: Path):
        data = path.read_bytes()
        if len(data) < 20:
            raise ValueError(f"Truncated DBC: {path}")
        magic, count, self.fields, self.size, string_size = struct.unpack_from("<4s4I", data)
        if magic != b"WDBC" or len(data) != 20 + count * self.size + string_size:
            raise ValueError(f"Invalid WDBC structure: {path}")
        self.records = [bytearray(data[20 + i * self.size:20 + (i + 1) * self.size]) for i in range(count)]
        self.strings = bytearray(data[20 + count * self.size:])

    def words(self, record: bytearray) -> list[int]:
        if self.size != self.fields * 4:
            raise ValueError("Expected an unpacked 32-bit DBC row")
        return list(struct.unpack(f"<{self.fields}I", record))

    def text(self, offset: int) -> str:
        if offset >= len(self.strings):
            raise ValueError(f"DBC string offset {offset} is out of range")
        end = self.strings.find(0, offset)
        if end < 0:
            raise ValueError(f"DBC string at offset {offset} is unterminated")
        return self.strings[offset:end].decode("utf-8")

    def save(self, path: Path) -> None:
        payload = struct.pack("<4s4I", b"WDBC", len(self.records), self.fields, self.size, len(self.strings))
        payload += b"".join(self.records) + self.strings
        path.write_bytes(payload)


def require_shape(name: str, target: DBC, source: DBC) -> None:
    if (target.fields, target.size) != (source.fields, source.size):
        raise ValueError(f"{name} DBC layout differs between server and Dracthyr bundle")


def copy_rows_with_strings(name: str, target: DBC, source: DBC, rows: list[bytearray], fields: tuple[int, ...]) -> None:
    if not rows:
        raise ValueError(f"No Dracthyr rows selected from {name}.dbc")
    base = len(target.strings)
    for row in rows:
        for field in fields:
            offset = struct.unpack_from("<I", row, field * 4)[0]
            if offset:
                source.text(offset)
                struct.pack_into("<I", row, field * 4, base + offset)
    target.strings.extend(source.strings)
    target.records.extend(rows)


def copy_spell_rows(target: DBC, source: DBC, rows: list[bytearray]) -> None:
    """Copy spell strings individually; appending a second full Spell string table bloats the bundle."""
    string_fields = tuple(range(136, 152)) + tuple(range(153, 169)) + \
                    tuple(range(170, 186)) + tuple(range(187, 203))
    offsets: dict[str, int] = {}
    for row in rows:
        for field in string_fields:
            offset = struct.unpack_from("<I", row, field * 4)[0]
            if not offset:
                continue
            value = source.text(offset)
            if value not in offsets:
                offsets[value] = len(target.strings)
                target.strings.extend(value.encode("utf-8") + b"\0")
            struct.pack_into("<I", row, field * 4, offsets[value])
    target.records.extend(rows)


def indexed_rows(table: DBC, ids: set[int], *, name: str) -> list[bytearray]:
    if table.size != table.fields * 4:
        raise ValueError(f"{name}.dbc has an unexpected packed layout")
    return [row for row in table.records if table.words(row)[0] in ids]


def unique_ids(table: DBC, name: str) -> set[int]:
    if table.size != table.fields * 4:
        raise ValueError(f"{name}.dbc has an unexpected packed layout")
    ids = [table.words(row)[0] for row in table.records]
    if len(ids) != len(set(ids)):
        raise ValueError(f"{name}.dbc contains duplicate row IDs")
    return set(ids)


def merge(server_dir: Path, race_dir: Path, class_spell_source: Path, output_dir: Path,
          class_visual_source: Path) -> dict:
    server_dir = server_dir.resolve(strict=True)
    race_dir = race_dir.resolve(strict=True)
    class_spell_source = class_spell_source.resolve(strict=True)
    class_visual_source = class_visual_source.resolve(strict=True)
    output_dir = output_dir.resolve()
    if output_dir.exists():
        raise ValueError(f"Output already exists; inspect or remove it before rebuilding: {output_dir}")
    if output_dir == server_dir or output_dir == race_dir or output_dir.is_relative_to(server_dir) or output_dir.is_relative_to(race_dir):
        raise ValueError("Output must be separate from both DBC sources")

    output_dir.mkdir(parents=True)
    for path in server_dir.glob("*.dbc"):
        shutil.copyfile(path, output_dir / path.name)
    if not list(output_dir.glob("*.dbc")):
        raise ValueError(f"No server DBC files found in {server_dir}")

    changed: dict[str, int] = {}

    # Replace the two unused race slots with the faction-specific Dracthyr records.
    name = "ChrRaces"
    target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
    require_shape(name, target, source)
    selected = indexed_rows(source, set(RACES), name=name)
    if {target.words(row)[0] for row in selected} != set(RACES) or len(selected) != len(RACES):
        raise ValueError("Dracthyr ChrRaces IDs 12 and 13 must each occur exactly once")
    if (source.fields, source.size) != (69, 276):
        raise ValueError("ChrRaces.dbc does not match the supported 3.3.5 layout")
    for row in selected:
        values = source.words(row)
        race = values[0]
        if source.text(values[6]) != "Dh" or source.text(values[11]) != "Dracthyr":
            raise ValueError(f"Dracthyr race {race} has missing or invalid client identifiers")
        if source.text(values[14]) != "Dracthyr":
            raise ValueError(f"Dracthyr race {race} has no enUS display name")
    target.records = [row for row in target.records if target.words(row)[0] not in RACES]
    copy_rows_with_strings(name, target, source, selected, (6, 11, *range(14, 30)))
    target.records.sort(key=lambda row: target.words(row)[0])
    target.save(output_dir / f"{name}.dbc")
    installed = DBC(output_dir / f"{name}.dbc")
    for race in RACES:
        installed_rows = [row for row in installed.records if installed.words(row)[0] == race]
        if len(installed_rows) != 1:
            raise ValueError(f"Integrated Dracthyr race {race} occurs {len(installed_rows)} times")
        values = installed.words(installed_rows[0])
        if installed.text(values[6]) != "Dh" or installed.text(values[11]) != "Dracthyr":
            raise ValueError(f"Integrated Dracthyr race {race} lost its client identifiers")
    changed[name] = len(selected)

    # Append Dracthyr appearance sections and replace any stale placeholder sections.
    name = "CharSections"
    target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
    require_shape(name, target, source)
    selected = [row for row in source.records if target.words(row)[1] in RACES]
    target.records = [row for row in target.records if target.words(row)[1] not in RACES]
    ids = unique_ids(target, name)
    next_id = max(ids, default=0)
    for row in selected:
        next_id += 1
        struct.pack_into("<I", row, 0, next_id)
    copy_rows_with_strings(name, target, source, selected, (4, 5, 6))
    target.records.sort(key=lambda row: target.words(row)[0])
    target.save(output_dir / f"{name}.dbc")
    changed[name] = len(selected)

    # Facial-hair rows use race ID as their first word and do not have string references.
    name = "CharacterFacialHairStyles"
    source = DBC(race_dir / f"{name}.dbc")
    target_path = output_dir / f"{name}.dbc"
    selected = [row for row in source.records if source.words(row)[0] in RACES]
    if target_path.exists():
        target = DBC(target_path)
        require_shape(name, target, source)
        target.records = [row for row in target.records if target.words(row)[0] not in RACES]
        target.records.extend(selected)
        target.save(target_path)
    else:
        # The server does not load this client-only table, so the class core has no copy.
        shutil.copyfile(race_dir / f"{name}.dbc", target_path)
    changed[name] = len(selected)

    # Dracthyr start outfits have no external row references; assign IDs above custom classes.
    name = "CharStartOutfit"
    target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
    require_shape(name, target, source)
    if target.size != 296 or target.fields != 77:
        raise ValueError("CharStartOutfit.dbc layout is not the supported 3.3.5 format")
    selected = [bytearray(row) for row in source.records if row[4] in RACES]
    target.records = [row for row in target.records
                      if row[4] not in RACES and (row[4], row[5]) not in FORBIDDEN_COMBINATIONS]
    next_id = max(struct.unpack_from("<I", row)[0] for row in target.records)
    keys = {(row[4], row[5], row[6]) for row in target.records}
    for row in selected:
        key = (row[4], row[5], row[6])
        if key in keys:
            raise ValueError(f"Duplicate Dracthyr start outfit {key}")
        keys.add(key)
        next_id += 1
        struct.pack_into("<I", row, 0, next_id)
    if len(selected) != len(RACES) * len(RACE_CLASSES) * 2:
        raise ValueError(f"Expected {len(RACES) * len(RACE_CLASSES) * 2} Dracthyr start outfits, found {len(selected)}")
    target.records.extend(selected)
    target.records.sort(key=lambda row: struct.unpack_from("<I", row)[0])
    target.save(output_dir / f"{name}.dbc")
    changed[name] = len(selected)

    # CharBaseInfo is the compact race/class availability table used by character creation.
    name = "CharBaseInfo"
    target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
    require_shape(name, target, source)
    if target.size != 2:
        raise ValueError("CharBaseInfo.dbc layout is not the supported compact format")
    selected = [bytearray(row) for row in source.records if row[0] in RACES]
    expected = {(race, class_id) for race in RACES for class_id in RACE_CLASSES}
    selected_keys = {(row[0], row[1]) for row in selected}
    if selected_keys != expected or len(selected) != len(expected):
        raise ValueError("Dracthyr CharBaseInfo rows do not match the supported race/class combinations")
    target.records = [row for row in target.records
                      if row[0] not in RACES and (row[0], row[1]) not in FORBIDDEN_COMBINATIONS]
    target.records.extend(selected)
    target.records.sort(key=lambda row: (row[0], row[1]))
    target.save(output_dir / f"{name}.dbc")
    changed[name] = len(selected)

    # Racial spells depend on existing WotLK support rows; rebase their localized strings.
    name = "Spell"
    target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
    require_shape(name, target, source)
    selected = indexed_rows(source, set(SPELL_IDS), name=name)
    if {target.words(row)[0] for row in selected} != set(SPELL_IDS):
        raise ValueError("The Dracthyr racial spell set is incomplete")
    collisions = set(SPELL_IDS) & unique_ids(target, name)
    if collisions:
        raise ValueError(f"Dracthyr racial spell IDs already exist in the class DBC: {sorted(collisions)}")
    copy_rows_with_strings(name, target, source, selected,
                           tuple(range(136, 152)) + tuple(range(153, 169)) +
                           tuple(range(170, 186)) + tuple(range(187, 203)))
    target.records.sort(key=lambda row: target.words(row)[0])
    target.save(output_dir / f"{name}.dbc")
    racial_spell_count = len(selected)

    # Necromancer/Cultist runtime spells are authored from WotLK donor effects.
    # Merge them into the same client/server Spell.dbc instead of leaving their
    # runtime script registrations pointed at nonexistent IDs.
    class_source = DBC(class_spell_source)
    require_shape(name, target, class_source)
    class_spell_ids = set(range(363410, 363423))
    class_spells = indexed_rows(class_source, class_spell_ids, name=name)
    if {class_source.words(row)[0] for row in class_spells} != class_spell_ids or len(class_spells) != len(class_spell_ids):
        raise ValueError("The integrated class spell source must contain every spell ID 363410-363422 exactly once")
    collisions = class_spell_ids & unique_ids(target, name)
    if collisions:
        raise ValueError(f"Custom class spell IDs already exist in the server DBC: {sorted(collisions)}")
    copy_spell_rows(target, class_source, class_spells)
    target.records.sort(key=lambda row: target.words(row)[0])
    target.save(output_dir / f"{name}.dbc")
    changed[name] = racial_spell_count + len(class_spells)

    necromancer_visuals = integrate_necromancer_spell_visuals(output_dir, class_visual_source)
    changed["Spell"] += len(necromancer_visuals["spell_ids"])
    changed["SpellVisual"] = len(necromancer_visuals["visual_ids"])
    changed["SpellVisualKit"] = len(necromancer_visuals["source_kit_to_cloned_kit_ids"])

    # Add the reserved Dracthyr model/display rows after checking the active class tables.
    for name, ids, string_fields in (
        ("CreatureModelData", set(MODEL_IDS), (2,)),
        ("CreatureDisplayInfo", set(DISPLAY_IDS), (6, 7, 8, 9)),
    ):
        target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
        require_shape(name, target, source)
        selected = indexed_rows(source, ids, name=name)
        if {target.words(row)[0] for row in selected} != ids:
            raise ValueError(f"The Dracthyr {name} ID set is incomplete")
        collisions = ids & unique_ids(target, name)
        if collisions:
            raise ValueError(f"Reserved Dracthyr IDs already exist in {name}.dbc: {sorted(collisions)}")
        copy_rows_with_strings(name, target, source, selected, string_fields)
        target.records.sort(key=lambda row: target.words(row)[0])
        target.save(output_dir / f"{name}.dbc")
        changed[name] = len(selected)

    # SkillRaceClassInfo rows have no foreign key to their row ID, so remap only those IDs.
    name = "SkillRaceClassInfo"
    target, source = DBC(output_dir / f"{name}.dbc"), DBC(race_dir / f"{name}.dbc")
    require_shape(name, target, source)
    if target.fields != 8 or target.size != 32:
        raise ValueError("SkillRaceClassInfo.dbc layout is not the supported 3.3.5 format")
    combinations = {(race, class_id) for race in RACES for class_id in RACE_CLASSES}
    selected = [bytearray(row) for row in source.records
                if (next((race for race in RACES if target.words(row)[2] == 1 << (race - 1)), None),
                    next((class_id for class_id in RACE_CLASSES
                          if target.words(row)[3] == 1 << (class_id - 1)), None)) in combinations]
    for race, class_id in combinations:
        if not any(target.words(row)[2] == 1 << (race - 1) and target.words(row)[3] == 1 << (class_id - 1)
                   for row in selected):
            raise ValueError(f"SkillRaceClassInfo lacks Dracthyr skills for race/class {race}/{class_id}")
    target_ids = unique_ids(target, name)
    next_id = max(target_ids, default=0)
    for row in selected:
        next_id += 1
        struct.pack_into("<I", row, 0, next_id)
    target.records.extend(selected)
    target.records.sort(key=lambda row: target.words(row)[0])
    target.save(output_dir / f"{name}.dbc")
    changed[name] = len(selected)

    # Validate race/class creation, names, outfit coverage, racial spell names, and model links.
    races = DBC(output_dir / "ChrRaces.dbc")
    for race in RACES:
        words = races.words(next(row for row in races.records if races.words(row)[0] == race))
        if races.text(words[14]) != "Dracthyr":
            raise ValueError(f"ChrRaces row {race} does not expose the Dracthyr name")
        if words[4] not in DISPLAY_IDS or words[5] not in DISPLAY_IDS:
            raise ValueError(f"Dracthyr race {race} points at a missing display")
    displays = DBC(output_dir / "CreatureDisplayInfo.dbc")
    models = unique_ids(DBC(output_dir / "CreatureModelData.dbc"), "CreatureModelData")
    for row in indexed_rows(displays, set(DISPLAY_IDS), name="CreatureDisplayInfo"):
        if displays.words(row)[1] not in models:
            raise ValueError("A Dracthyr display points at a missing creature model")
    base_info = DBC(output_dir / "CharBaseInfo.dbc")
    final_combos = {(row[0], row[1]) for row in base_info.records}
    if not expected.issubset(final_combos):
        raise ValueError("Integrated CharBaseInfo is missing Dracthyr creation combinations")
    for race in RACES:
        available = {class_id for combo_race, class_id in final_combos if combo_race == race}
        if available != set(RACE_CLASSES):
            raise ValueError(f"Dracthyr race {race} has unexpected class combinations: {sorted(available)}")
        if (race, 11) in final_combos:
            raise ValueError(f"Dracthyr race {race} must not have the Druid class")
    if FORBIDDEN_COMBINATIONS & final_combos:
        raise ValueError(f"Forbidden Cultist/Necromancer race combinations remain: {sorted(FORBIDDEN_COMBINATIONS & final_combos)}")
    outfits = DBC(output_dir / "CharStartOutfit.dbc")
    outfit_combos = {(row[4], row[5], row[6]) for row in outfits.records}
    expected_outfits = {(race, class_id, gender) for race in RACES
                        for class_id in RACE_CLASSES for gender in (0, 1)}
    if not expected_outfits.issubset(outfit_combos):
        raise ValueError("Integrated CharStartOutfit is missing Dracthyr character creation outfits")
    class_table = DBC(output_dir / "ChrClasses.dbc")
    class_names = {class_table.text(class_table.words(row)[4]) for row in class_table.records
                   if class_table.words(row)[0] in (12, 13)}
    if not {"Cultist", "Necromancer"}.issubset(class_names):
        raise ValueError("Integrated ChrClasses is missing the custom class names")

    hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(output_dir.glob("*.dbc"))}
    manifest = {"format": 1, "status": "validated_integrated_server_client_dbc_set",
                "server_dbc_source": str(server_dir), "dracthyr_dbc_source": str(race_dir),
                "class_spell_source": str(class_spell_source),
                "necromancer_spell_visual_import": necromancer_visuals,
                "added_rows": changed, "sha256": hashes, "assets": [], "asset_sources": {}}
    (output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-dbc", required=True, type=Path)
    parser.add_argument("--race-dbc", required=True, type=Path)
    parser.add_argument("--class-spell-source", required=True, type=Path,
                        help="Spell.dbc containing custom Cultist/Necromancer rows 363410-363422")
    parser.add_argument("--class-visual-source", type=Path,
                        default=Path(__file__).resolve().parents[4] / "var" / "advai-ascension-canonical-source",
                        help="Canonical official Spell.dbc, SpellVisual.dbc, and SpellVisualKit.dbc source directory")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        manifest = merge(args.server_dbc, args.race_dbc, args.class_spell_source, args.output,
                         args.class_visual_source)
    except (ValueError, OSError) as error:
        parser.exit(1, f"INTEGRATED DBC BUILD FAILURE: {error}\n")
    print(f"Integrated {len(manifest['sha256'])} DBC tables; added rows: {manifest['added_rows']}")


if __name__ == "__main__":
    main()
