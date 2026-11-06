"""Import the production Necromancer WotLK spell and visual rows losslessly."""

from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path


SPELL_IDS = (500365, 805252, 899900, 899901)
VISUAL_IDS = (21988, 2939)
SOURCE_KITS = (217, 218, 2350, 9303, 10868, 10869, 4787)
KIT_REFERENCE_FIELDS = (1, 2, 3, 4, 5, 6, 23, 24, 25)
SPELL_STRING_FIELDS = tuple(range(136, 152)) + tuple(range(153, 169)) + tuple(range(170, 186)) + tuple(range(187, 203))
SPELL_MUTABLE_DESCRIPTION_FIELDS = frozenset(range(170, 203))


class DBC:
    def __init__(self, path: Path):
        self.path = path
        data = path.read_bytes()
        if len(data) < 20:
            raise ValueError(f"Truncated DBC: {path}")
        magic, count, self.fields, self.row_size, string_size = struct.unpack_from("<4s4I", data)
        if magic != b"WDBC" or self.row_size != self.fields * 4 or len(data) != 20 + count * self.row_size + string_size:
            raise ValueError(f"Invalid unpacked WDBC layout: {path}")
        self.rows = [bytearray(data[20 + i * self.row_size:20 + (i + 1) * self.row_size]) for i in range(count)]
        self.strings = bytearray(data[20 + count * self.row_size:])
        self.by_id = {self.word(row, 0): row for row in self.rows}
        if len(self.by_id) != len(self.rows):
            raise ValueError(f"Duplicate row IDs in {path}")

    @staticmethod
    def word(row: bytes, field: int) -> int:
        return struct.unpack_from("<I", row, field * 4)[0]

    def text(self, offset: int) -> str:
        if offset >= len(self.strings):
            raise ValueError(f"String offset {offset} is outside {self.path}")
        end = self.strings.find(0, offset)
        if end < 0:
            raise ValueError(f"Unterminated string in {self.path}")
        return self.strings[offset:end].decode("utf-8")

    def save(self) -> None:
        self.rows.sort(key=lambda row: self.word(row, 0))
        payload = struct.pack("<4s4I", b"WDBC", len(self.rows), self.fields, self.row_size, len(self.strings))
        payload += b"".join(self.rows) + self.strings
        self.path.write_bytes(payload)
        self.by_id = {self.word(row, 0): row for row in self.rows}


def _schema(name: str, target: DBC, source: DBC, fields: int) -> None:
    if target.fields != fields or (target.fields, target.row_size) != (source.fields, source.row_size):
        raise ValueError(f"Unexpected WotLK {name}.dbc schema")


def _spell_rows_match_core(left: DBC, left_row: bytes, right: DBC, right_row: bytes) -> bool:
    """Compare canonical spell data while allowing the deliberate tooltip rewrite."""
    for field in range(left.fields):
        if field in SPELL_MUTABLE_DESCRIPTION_FIELDS:
            continue
        if field in SPELL_STRING_FIELDS:
            left_value = left.text(DBC.word(left_row, field))
            right_value = right.text(DBC.word(right_row, field))
        else:
            left_value = DBC.word(left_row, field)
            right_value = DBC.word(right_row, field)
        if left_value != right_value:
            return False
    return True


def _copy_spells(target: DBC, source: DBC, spell_rows: DBC) -> None:
    missing = set(SPELL_IDS) - spell_rows.by_id.keys()
    if missing:
        raise ValueError(f"Official Necromancer SpellRows.dbc lacks production IDs: {sorted(missing)}")
    string_map: dict[str, int] = {}
    for spell_id in SPELL_IDS:
        official_row = spell_rows.by_id[spell_id]
        canonical_row = source.by_id.get(spell_id)
        if canonical_row is not None and not _spell_rows_match_core(source, canonical_row, spell_rows, official_row):
            raise ValueError(f"Canonical and official Necromancer spell data disagree for {spell_id}")
        row_source = source if canonical_row is not None else spell_rows
        row = bytearray(canonical_row if canonical_row is not None else official_row)
        target_row = target.by_id.get(spell_id)
        if target_row is not None:
            if not _spell_rows_match_core(target, target_row, row_source, row):
                raise ValueError(f"Staged Necromancer spell row {spell_id} differs from its official core data")
            continue
        for field in SPELL_STRING_FIELDS:
            offset = DBC.word(row, field)
            if not offset:
                continue
            value = row_source.text(offset)
            if value not in string_map:
                string_map[value] = len(target.strings)
                target.strings.extend(value.encode("utf-8") + b"\0")
            struct.pack_into("<I", row, field * 4, string_map[value])
        target.rows.append(row)
        target.by_id[spell_id] = row


def integrate_necromancer_spell_visuals(dbc_dir: Path, source_dir: Path, spell_rows_path: Path) -> dict:
    """Import official summon spell rows, two visuals, and cloned kits into staged DBCs."""
    dbc_dir = dbc_dir.resolve(strict=True)
    source_dir = source_dir.resolve(strict=True)
    spell_rows_path = spell_rows_path.resolve(strict=True)
    tables = {}
    for name, fields in (("Spell", 234), ("SpellVisual", 32), ("SpellVisualKit", 38)):
        target, source = DBC(dbc_dir / f"{name}.dbc"), DBC(source_dir / f"{name}.dbc")
        _schema(name, target, source, fields)
        tables[name] = (target, source)

    spell, source_spell = tables["Spell"]
    visual, source_visual = tables["SpellVisual"]
    kit, source_kit = tables["SpellVisualKit"]
    official_spell_rows = DBC(spell_rows_path)
    _schema("Necromancer SpellRows", official_spell_rows, official_spell_rows, 234)
    if set(VISUAL_IDS) - source_visual.by_id.keys():
        raise ValueError("Canonical SpellVisual.dbc lacks required visual rows 21988/2939")
    if set(SOURCE_KITS) - source_kit.by_id.keys():
        raise ValueError(f"Canonical SpellVisualKit.dbc lacks source kit rows: {sorted(set(SOURCE_KITS) - source_kit.by_id.keys())}")
    _copy_spells(spell, source_spell, official_spell_rows)

    visual_collisions = set(VISUAL_IDS) & visual.by_id.keys()
    if visual_collisions:
        if visual_collisions != set(VISUAL_IDS):
            raise ValueError(f"Only part of the Necromancer visual set exists in staged SpellVisual.dbc: {sorted(visual_collisions)}")

        # Rebuilds start from the already-installed server DBCs. Verify that the
        # existing rows are the exact canonical visuals with their cloned kit
        # references, then reuse them rather than treating them as collisions.
        kit_remap: dict[int, int] = {}
        for visual_id in VISUAL_IDS:
            expected_row = source_visual.by_id[visual_id]
            actual_row = visual.by_id[visual_id]
            for field in range(visual.fields):
                if field not in KIT_REFERENCE_FIELDS:
                    if actual_row[field * 4:field * 4 + 4] != expected_row[field * 4:field * 4 + 4]:
                        raise ValueError(f"Staged Necromancer SpellVisual row {visual_id} differs from its canonical source")
                    continue

                source_kit_id = DBC.word(expected_row, field)
                target_kit_id = DBC.word(actual_row, field)
                if source_kit_id not in SOURCE_KITS:
                    if target_kit_id != source_kit_id:
                        raise ValueError(f"Staged Necromancer visual {visual_id} has an unexpected kit reference in field {field}")
                    continue
                previous_target = kit_remap.setdefault(source_kit_id, target_kit_id)
                if previous_target != target_kit_id:
                    raise ValueError(f"Staged Necromancer visual set maps source kit {source_kit_id} inconsistently")

        if set(kit_remap) != set(SOURCE_KITS):
            raise ValueError(f"Staged Necromancer visuals do not reference the full source kit set: {sorted(set(SOURCE_KITS) - set(kit_remap))}")
        for source_id, target_id in kit_remap.items():
            target_row = kit.by_id.get(target_id)
            if target_row is None:
                raise ValueError(f"Staged Necromancer visual references missing cloned kit {target_id}")
            expected_kit_row = bytearray(source_kit.by_id[source_id])
            struct.pack_into("<I", expected_kit_row, 0, target_id)
            if target_row != expected_kit_row:
                raise ValueError(f"Staged Necromancer kit {target_id} differs from canonical source kit {source_id}")
    else:
        # Allocate above the highest ID in either table, so IDs stay deterministic
        # and canonical kit data never overwrites shared WotLK kit rows.
        next_kit_id = max(max(kit.by_id), max(source_kit.by_id)) + 1
        kit_remap = {source_id: next_kit_id + index for index, source_id in enumerate(SOURCE_KITS)}
        collisions = set(kit_remap.values()) & kit.by_id.keys()
        if collisions:
            raise ValueError(f"Allocated Necromancer kit IDs collide with staged SpellVisualKit.dbc: {sorted(collisions)}")

        for visual_id in VISUAL_IDS:
            row = bytearray(source_visual.by_id[visual_id])
            for field in KIT_REFERENCE_FIELDS:
                source_kit_id = DBC.word(row, field)
                if source_kit_id in kit_remap:
                    struct.pack_into("<I", row, field * 4, kit_remap[source_kit_id])
            visual.rows.append(row)
            visual.by_id[visual_id] = row
        for source_id in SOURCE_KITS:
            row = bytearray(source_kit.by_id[source_id])
            struct.pack_into("<I", row, 0, kit_remap[source_id])
            kit.rows.append(row)
            kit.by_id[kit_remap[source_id]] = row

    spell.save()
    visual.save()
    kit.save()

    # Validate imported references and ensure shared source rows were not replaced.
    for spell_id in SPELL_IDS:
        row = spell.by_id[spell_id]
        visual_id = DBC.word(row, 131)
        if visual_id and visual_id not in visual.by_id:
            raise ValueError(f"Spell {spell_id} references missing SpellVisual {visual_id}")
    for visual_id in VISUAL_IDS:
        row = visual.by_id[visual_id]
        for field in KIT_REFERENCE_FIELDS:
            kit_id = DBC.word(row, field)
            if kit_id and kit_id not in kit.by_id:
                raise ValueError(f"SpellVisual {visual_id} references missing SpellVisualKit {kit_id}")
    effect_path = dbc_dir / "SpellVisualEffectName.dbc"
    if effect_path.exists():
        effect_ids = DBC(effect_path).by_id
        for kit_id in kit_remap.values():
            row = kit.by_id[kit_id]
            for field in range(3, 15):
                effect_id = DBC.word(row, field)
                if effect_id and effect_id not in effect_ids:
                    raise ValueError(f"Cloned SpellVisualKit {kit_id} references missing effect {effect_id}")

    return {
        "source_dir": str(source_dir),
        "source_spell_dbc": str(source_dir / "Spell.dbc"),
        "official_spell_rows_dbc": str(spell_rows_path),
        "source_spellvisual_dbc": str(source_dir / "SpellVisual.dbc"),
        "source_spellvisualkit_dbc": str(source_dir / "SpellVisualKit.dbc"),
        "spell_ids": list(SPELL_IDS), "visual_ids": list(VISUAL_IDS),
        "source_kit_to_cloned_kit_ids": {str(k): v for k, v in kit_remap.items()},
        "excluded_test_spell_ids": [600992],
        "dbc_sha256": {name: hashlib.sha256((dbc_dir / f"{name}.dbc").read_bytes()).hexdigest()
                        for name in ("Spell", "SpellVisual", "SpellVisualKit")},
    }
