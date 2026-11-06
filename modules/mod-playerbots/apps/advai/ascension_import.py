"""Import the installed Ascension class definitions without changing either client.

The extracted snapshot is source material, not an installable server patch. The
mechanics report identifies effects which still need native server implementation.
"""

import argparse
from collections import Counter
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import re
import struct

from backport_mpq import Storm
from backports import DBC


CLASSES = ("Cultist", "Necromancer")
STRINGS = (136, 153, 170, 187)
SUPPORT_TABLES = ("SpellCastTimes", "SpellDuration", "SpellRange", "SpellRadius", "SpellIcon",
                  "SpellVisual", "SpellVisualKit", "SpellVisualEffectName", "SpellRuneCost",
                  "SummonProperties", "CreatureDisplayInfo", "CreatureModelData", "ChrClasses", "ChrSpecs",
                  "CharacterAdvancement", "CharacterAdvancementClassTypes", "CharacterAdvancementTabTypes",
                  "CharacterAdvancementEssence", "SpellChargesCategory", "SpellCharges",
                  "SoundEntries", "SoundEntriesAdvanced", "SpellEffectCameraShakes", "CameraShakes",
                  "SpellVisualKitModelAttach", "SpellVisualKitAreaModel", "SpellVisualPrecastTransitions")


def addon_catalog(path: Path) -> list[dict]:
    entries = []
    section = None
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        heading = re.search(r'\["([^"]+)"\]\s*=\s*\{', line)
        if heading:
            section = heading[1]
        if "{ id =" not in line:
            continue
        match = re.search(r'id = (\d+), name = "([^"]+)",.*?type = "([^"]+)", '
                          r'level = (\d+), spells = \{ ([\d, ]+) \}', line)
        if not section or not match:
            raise ValueError(f"Unrecognized addon entry at {path}:{number}")
        entries.append({"ID": int(match[1]), "Name": match[2], "Type": match[3],
                        "RequiredLevel": int(match[4]), "Tab": section,
                        "Spells": [int(value) for value in match[5].split(",")]})
    if not entries:
        raise ValueError(f"Empty class addon catalog: {path}")
    return entries


class SpellSource:
    def __init__(self, data: bytes):
        magic, self.count, fields, size, strings = struct.unpack_from("<4s4I", data)
        if (magic, fields, size) != (b"WDBC", 234, 936) or len(data) != 20 + self.count * size + strings:
            raise ValueError("Ascension Spell.dbc is not the supported 234-field WotLK layout")
        self.data = data
        self.strings = memoryview(data)[20 + self.count * size:]
        self.index = {}
        for offset in range(20, 20 + self.count * size, size):
            identity = struct.unpack_from("<I", data, offset)[0]
            if identity in self.index:
                raise ValueError(f"Duplicate Ascension spell ID {identity}")
            self.index[identity] = offset

    def row(self, identity: int) -> list[int]:
        if identity not in self.index:
            raise ValueError(f"Required Ascension spell {identity} is absent from this snapshot")
        return list(struct.unpack_from("<234I", self.data, self.index[identity]))

    def text(self, offset: int) -> str:
        if offset >= len(self.strings):
            raise ValueError(f"Invalid Ascension spell string offset {offset}")
        end = self.data.find(b"\0", 20 + self.count * 936 + offset)
        if end < 0:
            raise ValueError("Unterminated Ascension spell string")
        return self.data[20 + self.count * 936 + offset:end].decode("utf-8")

    def references(self, row: list[int]) -> set[int]:
        result = set(row[24:28] + row[116:119])
        for effect in range(3):
            if row[71 + effect] == 36:  # SPELL_EFFECT_LEARN_SPELL
                result.add(row[116 + effect])
        for column in STRINGS:
            result.update(int(match) for match in re.findall(r'\$(?:\?\w)?(\d+)[a-zA-Z]',
                                                             self.text(row[column])))
        return result - {0}


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def import_classes(client: Path, addons: Path, archive_name: str, support_archives: list[str],
                   output: Path, library: Path) -> None:
    client = client.resolve(strict=True)
    archive_path = (client / "Data" / archive_name).resolve(strict=True)
    if not archive_path.is_relative_to(client / "Data"):
        raise ValueError("The source archive must be inside the selected client Data directory")
    if output.exists():
        raise ValueError(f"Refusing to replace an existing class snapshot: {output}")
    content = client / "Data/Content"
    advancement = json.loads((content / "CharacterAdvancementData.json").read_text(encoding="utf-8-sig"))
    ranks = json.loads((content / "SpellRankData.json").read_text(encoding="utf-8-sig"))
    entries = [entry for entry in advancement if entry.get("Class") in CLASSES]
    identities = {entry["ID"]: entry for entry in advancement}
    if len(identities) != len(advancement):
        raise ValueError("Ascension advancement IDs are not unique")
    roots = {spell for entry in entries for spell in entry.get("Spells", [])}
    addon_entries = {}
    for name in CLASSES:
        rows = addon_catalog(addons / f"{name}UI/{name}Data.lua")
        addon_entries[name] = rows
        roots.update(spell for entry in rows for spell in entry["Spells"])
        if not any(entry.get("Class") == name for entry in entries):
            raise ValueError(f"The installed advancement catalog has no {name} definitions")
    class_ranks = [entry for entry in ranks if entry["firstSpellId"] in roots]
    roots.update(entry["spellId"] for entry in class_ranks)
    prerequisites = []
    pending = list({identity for entry in entries for identity in entry.get("RequiredIDs", [])})
    seen = {entry["ID"] for entry in entries}
    while pending:
        identity = pending.pop()
        if identity in seen:
            continue
        if identity not in identities:
            raise ValueError(f"Missing advancement prerequisite {identity}")
        seen.add(identity)
        entry = identities[identity]
        prerequisites.append(entry)
        roots.update(entry.get("Spells", []))
        pending.extend(entry.get("RequiredIDs", []))
    storm = Storm(library)
    archive = wintypes.HANDLE()
    storm.require(storm.api.SFileOpenArchive(str(archive_path), 0, 0, ctypes.byref(archive)), "open Ascension snapshot")
    source_hashes = {}
    try:
        payload = storm.read(archive, "DBFilesClient\\Spell.dbc", max_bytes=512 * 1024 * 1024)
        source_hashes["Spell.dbc"] = hashlib.sha256(payload).hexdigest()
        spells = SpellSource(payload)
        output.mkdir(parents=True)
        support_sources = {}
        handles = [archive]
        try:
            for relative in support_archives:
                path = (client / "Data" / relative).resolve(strict=True)
                if not path.is_relative_to(client / "Data"):
                    raise ValueError("Support archives must be inside the selected client Data directory")
                handle = wintypes.HANDLE()
                storm.require(storm.api.SFileOpenArchive(str(path), 0, 0, ctypes.byref(handle)), f"open {path.name}")
                handles.append(handle)
            for name in SUPPORT_TABLES:
                member = f"DBFilesClient\\{name}.dbc"
                candidates = []
                for index, handle in enumerate(handles):
                    probe = wintypes.HANDLE()
                    if storm.api.SFileOpenFileEx(handle, member.encode("ascii"), 0, ctypes.byref(probe)):
                        storm.require(storm.api.SFileCloseFile(probe), f"close {member}")
                        candidates.append(index)
                if not candidates:
                    raise ValueError(f"Required support table {name} is absent from the declared archive layers")
                index = candidates[0]
                data = storm.read(handles[index], member)
                (output / f"{name}.dbc").write_bytes(data)
                source_hashes[f"{name}.dbc"] = hashlib.sha256(data).hexdigest()
                support_sources[name] = archive_name if index == 0 else support_archives[index - 1]
                DBC(output / f"{name}.dbc")
        finally:
            for handle in reversed(handles[1:]):
                storm.require(storm.api.SFileCloseArchive(handle), "close support archive")
        # Older addon/Content JSON reuses IDs now owned by other classes. Keep it
        # as reference inventory only; installed DBC nodes and families own the import.
        roots = set()
        class_types = DBC(output / "CharacterAdvancementClassTypes.dbc")
        types = {struct.unpack_from("<I", record)[0]: struct.unpack_from("<I", record, 8)[0]
                 for record in class_types.records if struct.unpack_from("<I", record, 16)[0]}
        advancement_table = DBC(output / "CharacterAdvancement.dbc")
        for record in advancement_table.records:
            if types.get(struct.unpack_from("<I", record, 32 * 4)[0]) in (23, 25):
                roots.update(spell for spell in struct.unpack_from("<5I", record, 5 * 4) if spell)
        def verified_rank(entry):
            root, identity = entry["firstSpellId"], entry["spellId"]
            if root not in roots or identity not in spells.index or root not in spells.index:
                return False
            first, ranked = spells.row(root), spells.row(identity)
            return first[208] == ranked[208] and spells.text(first[136]) == spells.text(ranked[136])

        class_ranks = [entry for entry in ranks if verified_rank(entry)]
        roots.update(entry["spellId"] for entry in class_ranks)
        missing = sorted(roots - spells.index.keys())
        if missing:
            raise ValueError(f"The selected archive is missing {len(missing)} required class/rank spells: {missing}")
        families = {identity for identity, offset in spells.index.items()
                    if struct.unpack_from("<I", spells.data, offset + 208 * 4)[0] in (29, 31)}
        selected = roots | families
        pending = list(selected)
        unresolved = {}
        while pending:
            identity = pending.pop()
            for reference in spells.references(spells.row(identity)):
                if reference not in spells.index:
                    unresolved.setdefault(identity, []).append(reference)
                elif reference not in selected:
                    selected.add(reference)
                    pending.append(reference)
        # Relocate strings into a compact, English-only snapshot; keep numeric effects unchanged.
        strings = bytearray(b"\0")
        offsets = {"": 0}
        records = []
        descriptions = {}
        effects, auras, powers = Counter(), Counter(), Counter()
        mechanics = []
        for identity in sorted(selected):
            row = spells.row(identity)
            texts = [spells.text(row[column]) for column in STRINGS]
            descriptions[identity] = dict(zip(("name", "rank", "description", "aura"), texts))
            for column, text in zip(STRINGS, texts):
                if text not in offsets:
                    offsets[text] = len(strings)
                    strings.extend(text.encode("utf-8") + b"\0")
                row[column:column + 16] = [offsets[text]] + [0] * 15
            records.append(struct.pack("<234I", *row))
            if identity in roots:
                effects.update(effect for effect in row[71:74] if effect)
                auras.update(aura for aura in row[95:98] if aura)
                powers[row[41]] += 1
                scripted = any(effect in (3, 77) or effect >= 165 for effect in row[71:74])
                scripted |= any(aura in (4, 226) or aura >= 317 for aura in row[95:98])
                if scripted or row[41] not in (0, 1, 2, 3, 4, 5, 6, 0xFFFFFFFE):
                    mechanics.append({"id": identity, "name": texts[0], "effects": row[71:74],
                                      "auras": row[95:98], "power": row[41]})
        (output / "Spell.dbc").write_bytes(struct.pack("<4s4I", b"WDBC", len(records), 234, 936, len(strings))
                                          + b"".join(records) + strings)
        write_json(output / "advancement.json", entries)
        write_json(output / "prerequisites.json", prerequisites)
        write_json(output / "ranks.json", class_ranks)
        write_json(output / "addon-catalog.json", addon_entries)
        write_json(output / "spell-text.json", descriptions)
        write_json(output / "mechanics-required.json", {"spells": mechanics, "unresolved_references": unresolved})
        manifest = {"format": 1, "client": str(client), "archive": str(archive_path),
                    "class_entries": dict(Counter(entry["Class"] for entry in entries)),
                    "root_spells": len(roots), "dependency_spells": len(selected - roots),
                    "class_family_spells": len(families),
                    "requires_server_mechanics": len(mechanics), "effects": dict(effects),
                    "auras": dict(auras), "powers": dict(powers), "source_sha256": source_hashes,
                    "support_archives": support_sources,
                    "sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in sorted(output.iterdir()) if path.is_file()}}
        write_json(output / "manifest.json", manifest)
        print(json.dumps({key: manifest[key] for key in ("class_entries", "root_spells", "dependency_spells",
                                                        "requires_server_mechanics", "powers")}))
        print(f"Imported class source snapshot: {output.resolve()}")
    finally:
        storm.require(storm.api.SFileCloseArchive(archive), "close Ascension snapshot")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--addons", required=True, type=Path)
    parser.add_argument("--archive", default="area-52/patch-D.MPQ")
    parser.add_argument("--support-archive", action="append", dest="support_archives")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--stormlib", required=True, type=Path)
    args = parser.parse_args()
    try:
        import_classes(args.client, args.addons, args.archive,
                       args.support_archives if args.support_archives is not None else ["patch-S.MPQ", "patch-M.MPQ"],
                       args.output, args.stormlib)
    except (ValueError, OSError, KeyError, struct.error) as error:
        parser.exit(1, f"ADVAI ASCENSION IMPORT FAILURE: {error}\n")
