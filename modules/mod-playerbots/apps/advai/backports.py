"""Build the matching server/client DBC bundle for the local Dracthyr backport."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct


DONORS = {
    (12, 1): 1, (12, 3): 3, (12, 4): 1, (12, 5): 1, (12, 6): 1, (12, 8): 1, (12, 9): 1,
    (13, 1): 2, (13, 3): 2, (13, 4): 2, (13, 5): 8, (13, 6): 2, (13, 8): 10, (13, 9): 2,
    (12, 12): 1, (12, 13): 1, (13, 12): 5, (13, 13): 5,
}
DONOR_CLASSES = {12: 5, 13: 9}
SOURCE_TABLES = ("CharSections", "ChrRaces", "CreatureDisplayInfo", "CreatureModelData", "Spell")
RACIALS = (
    (363404, 130, "Glide", "Slow your fall. Requires falling; consumes no reagent.", 15000),
    (363405, 20549, "Tail Swipe", "Stun nearby opponents for a brief moment.", 90000),
    (363406, 61391, "Wing Buffet", "Push nearby opponents away from you.", 90000),
)
MODELS = ((16981, 2299), (16980, 2298), (17402, 2353), (17403, 2354), (17404, 2355), (17405, 2356))


class DBC:
    def __init__(self, path: Path):
        data = path.read_bytes()
        if len(data) < 20:
            raise ValueError(f"Truncated DBC: {path}")
        magic, count, self.fields, self.size, strings = struct.unpack_from("<4s4I", data)
        if magic != b"WDBC" or len(data) != 20 + count * self.size + strings:
            raise ValueError(f"Invalid 3.3.5 DBC: {path}")
        self.records = [bytearray(data[20 + i * self.size:20 + (i + 1) * self.size]) for i in range(count)]
        self.strings = bytearray(data[20 + count * self.size:])

    def words(self, record: bytearray) -> list[int]:
        if self.size != self.fields * 4:
            raise ValueError("This DBC uses packed byte fields")
        return list(struct.unpack(f"<{self.fields}I", record))

    def row(self, identity: int) -> bytearray:
        found = [record for record in self.records if struct.unpack_from("<I", record)[0] == identity]
        if len(found) != 1:
            raise ValueError(f"Required DBC row {identity}: found {len(found)}")
        return found[0]

    def string(self, value: str) -> int:
        offset = len(self.strings)
        self.strings.extend(value.encode("utf-8") + b"\0")
        return offset

    def text(self, offset: int) -> str:
        if offset >= len(self.strings):
            raise ValueError("Invalid DBC string offset")
        end = self.strings.find(0, offset)
        if end < 0:
            raise ValueError("Unterminated DBC string")
        return self.strings[offset:end].decode("utf-8")

    def save(self, path: Path) -> None:
        payload = struct.pack("<4s4I", b"WDBC", len(self.records), self.fields, self.size, len(self.strings))
        payload += b"".join(self.records) + self.strings
        temporary = path.with_suffix(".tmp")
        temporary.write_bytes(payload)
        temporary.replace(path)


def racial_spells(table: DBC) -> None:
    if table.fields != 234 or table.size != 936:
        raise ValueError("Spell.dbc does not match the supported core's 234-field layout")
    for identity, source, name, description, cooldown in RACIALS:
        if any(struct.unpack_from("<I", row)[0] == identity for row in table.records):
            raise ValueError(f"Racial spell ID {identity} is already allocated in the source data")
        row = table.words(table.row(source))
        row[0] = identity
        row[1] = 0
        row[3] = 0
        row[4] &= ~(0x40 | 0x80)
        row[29] = cooldown
        row[30] = 0
        row[37:40] = [0, 1, 1]
        row[41:44] = [0, 0, 0]
        row[52:68] = [0] * 16
        row[204] = 0
        row[208:212] = [0] * 4
        row[226] = 0
        row[136:152] = [table.string(name)] + [0] * 15
        row[153:169] = [0] * 16
        row[170:186] = [table.string(description)] + [0] * 15
        if identity == 363406:
            # Wing Buffet is a dracthyr knockback, not a frost stun.
            row[133] = 2837
            if row[71] != 98:
                raise ValueError("Wing Buffet source does not contain the expected native knockback effect")
            # The racial displaces opponents; the druid trigger's damage and snare are separate effects.
            for start, width in ((71, 3), (74, 3), (77, 3), (80, 3), (83, 3), (86, 3), (89, 3),
                                 (92, 3), (95, 3), (98, 3), (101, 3), (104, 3), (107, 3),
                                 (110, 3), (113, 3), (116, 3), (119, 3), (216, 3), (229, 3)):
                for effect in range(1, width):
                    row[start + effect] = 0
        table.records.append(bytearray(struct.pack("<234I", *row)))
    for identity, display in ((363402, 50004), (363403, 50005)):
        original = table.row(identity)
        row = table.words(original)
        row[3] = 0
        row[32] = 0x00000002 | 0x00001000 | 0x00002000 | 0x00800000
        row[37:40] = [0, 1, 1]
        row[41:44] = [0, 0, 0]
        row[52:68] = [0] * 16
        row[204] = 0
        row[110] = display
        row[133] = 497
        # Visage changes appearance. The source polymorph's movement/regen effects do not belong to it.
        for start in (71, 74, 77, 80, 83, 86, 89, 92, 95, 98, 101, 104, 107, 110, 113, 116, 119, 216, 229):
            row[start + 1:start + 3] = [0, 0]
        original[:] = struct.pack("<234I", *row)
    table.records.sort(key=lambda record: struct.unpack_from("<I", record)[0])


def outfits(table: DBC) -> None:
    if table.size != 296 or table.fields != 77:
        raise ValueError("Unexpected packed CharStartOutfit layout")
    table.records = [record for record in table.records if record[4] not in (12, 13)]
    identity = max(struct.unpack_from("<I", record)[0] for record in table.records)
    for (race, class_id), donor in DONORS.items():
        donor_class = DONOR_CLASSES.get(class_id, class_id)
        for gender in (0, 1):
            source = [record for record in table.records if tuple(record[4:7]) == (donor, donor_class, gender)]
            if len(source) != 1:
                raise ValueError(f"Missing or ambiguous donor outfit: {donor}/{donor_class}/{gender}")
            identity += 1
            record = bytearray(source[0])
            struct.pack_into("<I", record, 0, identity)
            record[4], record[5] = race, class_id
            table.records.append(record)


def class_spells(table: DBC, durations: DBC) -> None:
    specs = json.loads(Path(__file__).with_name("backport_spells.json").read_text(encoding="utf-8"))
    for spec in specs:
        identity = spec["id"]
        if any(struct.unpack_from("<I", row)[0] == identity for row in table.records):
            raise ValueError(f"Custom class spell ID {identity} is already allocated")
        row = table.words(table.row(spec["source"]))
        row[0] = identity
        row[1] = 0
        row[29], row[30] = spec["cooldown"], 0
        row[37:40] = [80, spec["level"], spec["level"]]
        row[41:44] = [0, spec.get("mana_cost", 0), spec.get("mana_per_level", 0)]
        row[204] = spec["mana_percent"]
        row[226] = 0
        row[208:212] = [0] * 4
        row[52:68] = [0] * 16
        if "reagent" in spec:
            row[52], row[60] = spec["reagent"], 1
        if "charges" in spec:
            row[36] = spec["charges"]
        if "damage" in spec:
            row[80] = spec["damage"] - 1
            row[77] = struct.unpack("<I", struct.pack("<f", spec["damage_per_level"]))[0]
        if "school" in spec:
            row[225] = spec["school"]
        if "summon" in spec:
            summons = [effect for effect in range(3) if row[71 + effect] in (28, 56)]
            if len(summons) != 1:
                raise ValueError(f"Custom summon source has no unambiguous native summon effect: {identity}")
            row[110 + summons[0]] = spec["summon"]
        if "duration" in spec:
            matches = [durations.words(record)[0] for record in durations.records
                       if struct.unpack_from("<i", record, 4)[0] == spec["duration"]
                       and struct.unpack_from("<i", record, 8)[0] == 0]
            if not matches:
                raise ValueError(f"No native duration matching {spec['duration']}ms")
            row[40] = min(matches)
        row[136:152] = [table.string(spec["name"])] + [0] * 15
        row[153:169] = [0] * 16
        description = {
            363410: "Hurl a bolt of frost that damages and slows an enemy.",
            363411: "Raise an undead companion to fight for you.",
            363412: "Surround yourself with bones that reduce incoming damage for 20 seconds.",
            363413: "Detonate a suitable nearby corpse, damaging enemies around it.",
            363414: "Damage an enemy or restore health to a friendly undead target.",
            363415: "Strike an enemy with cold and apply Frost Fever.",
            363416: "Raise an undead spellcaster to fight for you.",
            363417: "Raise an undead archer to fight for you.",
            363418: "Raise an undead construct to fight for you. Consumes a Soul Shard.",
            363419: "Fire a shadow projectile at an enemy.",
            363420: "Summon a temporary void servant to attack your target.",
            363421: "Summon a temporary faceless servant to attack your target.",
            363422: "Shoot an enemy with a skeletal bow.",
        }[identity]
        row[170:186] = [table.string(description)] + [0] * 15
        row[187:203] = [0] * 16
        table.records.append(bytearray(struct.pack("<234I", *row)))
    table.records.sort(key=lambda record: struct.unpack_from("<I", record)[0])


def facial_hair(table: DBC) -> None:
    if table.fields != 8 or table.size != 32:
        raise ValueError("Unexpected CharacterFacialHairStyles layout")
    source = [record for record in table.records if table.words(record)[0] == 11]
    if not source:
        raise ValueError("No Draenei facial-hair definitions to match the backport's CharSections")
    table.records = [record for record in table.records if table.words(record)[0] not in (12, 13)]
    for race in (12, 13):
        for record in source:
            clone = bytearray(record)
            struct.pack_into("<I", clone, 0, race)
            table.records.append(clone)


def char_sections(table: DBC) -> None:
    if table.fields != 10 or table.size != 40:
        raise ValueError("Unexpected CharSections layout")
    # The prototype appends copies with reused IDs, overwriting vanilla races in the
    # server's indexed store. Author both races from the same Draenei customization contract.
    table.records = [record for record in table.records if table.words(record)[1] not in (12, 13)]
    ids = [table.words(record)[0] for record in table.records]
    if len(ids) != len(set(ids)):
        raise ValueError("Vanilla CharSections records contain duplicate IDs")
    donor = [record for record in table.records if table.words(record)[1] == 11]
    if not donor:
        raise ValueError("Missing the declared Draenei appearance donor")
    identity = max(ids)
    for race in (12, 13):
        for record in donor:
            clone = bytearray(record)
            identity += 1
            struct.pack_into("<I", clone, 0, identity)
            struct.pack_into("<I", clone, 4, race)
            table.records.append(clone)
    table.records.sort(key=lambda record: struct.unpack_from("<I", record)[0])


def race_skills(table: DBC) -> None:
    if (table.fields, table.size) != (8, 32):
        raise ValueError("Unexpected SkillRaceClassInfo layout")
    original = [table.words(record) for record in table.records]
    identity = max(row[0] for row in original)
    for (race, class_id), donor in DONORS.items():
        donor_class = DONOR_CLASSES.get(class_id, class_id)
        seen = set()
        eligible = {row[1] for row in original
                    if (not row[2] or row[2] & (1 << (race - 1)))
                    and (not row[3] or row[3] & (1 << (class_id - 1)))}
        for row in original:
            if not row[2] or not row[2] & (1 << (donor - 1)):
                continue
            if row[3] and not row[3] & (1 << (donor_class - 1)):
                continue
            if row[1] in seen:
                continue  # GetSkillRaceClassInfo uses the first matching donor row in DBC order.
            seen.add(row[1])
            if row[1] in eligible:
                continue
            identity += 1
            clone = list(row)
            clone[0], clone[2], clone[3] = identity, 1 << (race - 1), 1 << (class_id - 1)
            table.records.append(bytearray(struct.pack("<8I", *clone)))
        language = 98 if race == 12 else 109
        if language not in seen:
            raise ValueError(f"The declared donor does not support the faction language: {donor}/{class_id}")


def isolated_models(source: Path, base: Path, output: Path) -> None:
    source_models = DBC(source / "CreatureModelData.dbc")
    source_displays = DBC(source / "CreatureDisplayInfo.dbc")
    models = DBC(base / "CreatureModelData.dbc")
    displays = DBC(base / "CreatureDisplayInfo.dbc")
    if (models.fields, models.size, displays.fields, displays.size) != (28, 112, 16, 64):
        raise ValueError("Unsupported creature model/display layout")
    for index, (display, model) in enumerate(MODELS):
        model_id, display_id = 10000 + index, 50000 + index
        if any(models.words(row)[0] == model_id for row in models.records):
            raise ValueError(f"Reserved Dracthyr model ID {model_id} is already occupied in the base")
        if any(displays.words(row)[0] == display_id for row in displays.records):
            raise ValueError(f"Reserved Dracthyr display ID {display_id} is already occupied in the base")
        candidates = [source_models.words(row) for row in source_models.records
                      if source_models.words(row)[0] == model
                      and source_models.text(source_models.words(row)[2]).lower().startswith("character\\dracthyr\\")]
        if len(candidates) != 1:
            raise ValueError(f"Missing or ambiguous Dracthyr source model {model}")
        record = candidates[0]
        record[0], record[2] = model_id, models.string(source_models.text(record[2]))
        models.records.append(bytearray(struct.pack("<28I", *record)))
        candidates = [source_displays.words(row) for row in source_displays.records
                      if source_displays.words(row)[0] == display and source_displays.words(row)[1] == model]
        if len(candidates) != 1:
            raise ValueError(f"Missing or ambiguous Dracthyr source display {display}")
        record = candidates[0]
        record[0], record[1] = display_id, model_id
        for field in (6, 7, 8):
            record[field] = displays.string(source_displays.text(record[field])) if record[field] else 0
        displays.records.append(bytearray(struct.pack("<16I", *record)))
    models.save(output / "CreatureModelData.dbc")
    displays.save(output / "CreatureDisplayInfo.dbc")
    races = DBC(output / "ChrRaces.dbc")
    if races.fields != 69 or races.size != 276:
        raise ValueError("ChrRaces.dbc does not match the supported 3.3.5 layout")
    for race in (12, 13):
        row = races.words(races.row(race))
        row[4:6] = [50000 + (race - 12) * 2, 50001 + (race - 12) * 2]
        # The UI and native race lookups use these two fields. The prototype
        # leaves both empty, which breaks race selection and its preview model.
        row[6] = races.string("Dh")
        row[11] = races.string("Dracthyr")
        if races.text(row[6]) != "Dh" or races.text(row[11]) != "Dracthyr":
            raise ValueError(f"Dracthyr race {race} has invalid client identifiers")
        if races.text(row[14]) != "Dracthyr":
            raise ValueError(f"Dracthyr race {race} has no enUS display name")
        races.row(race)[:] = struct.pack("<69I", *row)
    races.save(output / "ChrRaces.dbc")


def build(source: Path, base: Path, output: Path) -> None:
    source, base, output = source.resolve(strict=True), base.resolve(strict=True), output.resolve()
    if output == source or output == base or output.is_relative_to(source) or output.is_relative_to(base):
        raise ValueError("Generate a separate bundle; source DBCs cannot be overwritten")
    output.mkdir(parents=True, exist_ok=True)
    for name in SOURCE_TABLES:
        DBC(source / f"{name}.dbc")
        shutil.copyfile(source / f"{name}.dbc", output / f"{name}.dbc")
    isolated_models(source, base, output)
    sections = DBC(output / "CharSections.dbc")
    char_sections(sections)
    sections.save(output / "CharSections.dbc")
    spell = DBC(output / "Spell.dbc")
    racial_spells(spell)
    class_spells(spell, DBC(base / "SpellDuration.dbc"))
    spell.save(output / "Spell.dbc")
    outfit = DBC(base / "CharStartOutfit.dbc")
    outfits(outfit)
    outfit.save(output / "CharStartOutfit.dbc")
    hair = DBC(base / "CharacterFacialHairStyles.dbc")
    facial_hair(hair)
    hair.save(output / "CharacterFacialHairStyles.dbc")
    skills = DBC(base / "SkillRaceClassInfo.dbc")
    race_skills(skills)
    skills.save(output / "SkillRaceClassInfo.dbc")
    # The supplied prototype omits CharBaseInfo's required string block. Author the complete
    # combinations table from the supported client's base and the declared backport contracts.
    combinations = DBC(base / "CharBaseInfo.dbc")
    if combinations.size != 2:
        raise ValueError("Unexpected packed CharBaseInfo layout")
    forbidden = {(2, 12), (3, 12), (3, 13), (10, 12), (10, 13), (11, 12), (11, 13)}
    combinations.records = [record for record in combinations.records
                            if record[0] not in (12, 13) and (record[0], record[1]) not in forbidden]
    combinations.records.extend(bytearray((race, class_id)) for race, class_id in DONORS)
    final_pairs = {(record[0], record[1]) for record in combinations.records}
    remaining_forbidden = forbidden & final_pairs
    if remaining_forbidden:
        raise ValueError(f"Forbidden Cultist/Necromancer race combinations remain: {sorted(remaining_forbidden)}")
    missing_custom = set(DONORS) - final_pairs
    if missing_custom:
        raise ValueError(f"Custom race/class combinations are missing: {sorted(missing_custom)}")
    combinations.save(output / "CharBaseInfo.dbc")
    for name in (*SOURCE_TABLES, "CharStartOutfit", "SkillRaceClassInfo"):
        table = DBC(output / f"{name}.dbc")
        identities = [struct.unpack_from("<I", record)[0] for record in table.records]
        if len(identities) != len(set(identities)):
            raise ValueError(f"{name} contains duplicate indexed IDs")
    manifest = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(output.glob("*.dbc"))}
    (output / "manifest.json").write_text(json.dumps({"format": 1, "sha256": manifest}, indent=2) + "\n",
                                        encoding="utf-8")
    print(f"Authored Dracthyr server/client DBC bundle: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Local dracthyr_dbcs/DBFilesClient directory")
    parser.add_argument("--base", required=True, type=Path, help="Supported core's complete base DBC directory")
    parser.add_argument("--output", required=True, type=Path, help="Separate generated DBFilesClient directory")
    args = parser.parse_args()
    try:
        build(args.source, args.base, args.output)
    except (ValueError, OSError) as error:
        parser.exit(1, f"ADVAI BACKPORT CONTENT FAILURE: {error}\n")
