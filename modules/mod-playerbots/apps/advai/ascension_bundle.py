"""Merge class DBC dependencies without replacing the target client's records.

Support records receive independent IDs; references are rewritten together.
The output is staged source until the required client assets have been packaged.
Field positions follow WoWDBDefs' 3.3.5.12340 definitions and the supported core.
"""
import argparse
import hashlib
import json
import ntpath
from pathlib import Path
import shutil
import struct

from backports import DBC
from ascension_world import ensure_preceding_deletes, remap_display_ids

STRINGS = {
    "Spell": tuple(range(136, 152)) + tuple(range(153, 169)) +
             tuple(range(170, 186)) + tuple(range(187, 203)),
    "SpellRange": tuple(range(6, 22)) + tuple(range(23, 39)),
    "SpellIcon": (1,), "SpellVisualEffectName": (1, 2),
    "CreatureModelData": (2,), "CreatureDisplayInfo": (6, 7, 8, 9),
    "SoundEntries": (2, *range(3, 13), 23),
}
REFERENCES = {
    "Spell": {28: "SpellCastTimes", 40: "SpellDuration", 46: "SpellRange",
              **{i: "SpellRadius" for i in range(92, 95)},
              131: "SpellVisual", 132: "SpellVisual", 133: "SpellIcon", 134: "SpellIcon",
              226: "SpellRuneCost"},
    "SpellVisual": {**{i: "SpellVisualKit" for i in (*range(1, 7), 14, 15, 22, 23, 24, 25)},
                    8: "SpellVisualEffectName", 11: "SoundEntries", 12: "SoundEntries"},
    "SpellVisualKit": {**{i: "SpellVisualEffectName" for i in range(3, 15)},
                       15: "SoundEntries", 16: "SpellEffectCameraShakes"},
    "SpellVisualKitModelAttach": {1: "SpellVisualKit", 2: "SpellVisualEffectName"},
    "SpellEffectCameraShakes": {1: "CameraShakes", 2: "CameraShakes", 3: "CameraShakes"},
    "SoundEntries": {29: "SoundEntriesAdvanced"},
    "SoundEntriesAdvanced": {1: "SoundEntries"},
    "CreatureDisplayInfo": {1: "CreatureModelData"},
}
SUPPORT = ("SpellCastTimes", "SpellDuration", "SpellRange", "SpellRadius", "SpellIcon", "SpellVisual",
           "SpellVisualKit", "SpellVisualEffectName", "SpellRuneCost", "SummonProperties",
           "CreatureDisplayInfo", "CreatureModelData", "SoundEntries", "SoundEntriesAdvanced",
           "SpellEffectCameraShakes", "CameraShakes", "SpellVisualKitModelAttach")


class AssetIndex:
    def __init__(self, inventory_path: Path, alternates_path: Path):
        inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
        alternates = json.loads(alternates_path.read_text(encoding="utf-8"))
        self.target, self.source = {}, {}
        for kind, values in (("target", self.target), ("source", self.source)):
            for name, archives in inventory[kind]["found"].items():
                values[self.key(name)] = list(archives)
            for name, archives in alternates[kind].items():
                values[self.key(name)] = list(archives)
        self.package_files = {}
        self.references = {}
        self.plan = {}

    @staticmethod
    def key(name: str) -> str:
        return name.replace("/", "\\").strip("\\").casefold()

    @staticmethod
    def display_path(name: str) -> str:
        return name.replace("/", "\\").strip("\\")

    def candidates(self, name: str) -> list[str]:
        normalized = self.display_path(name)
        result = [normalized]
        suffix = ntpath.splitext(normalized)[1].casefold()
        if suffix == ".mdx":
            result.append(normalized[:-4] + ".m2")
        return result

    @staticmethod
    def best_archive(archives: list[str]) -> str:
        def rank(path: str) -> tuple[int, str]:
            lowered = path.replace("/", "\\").casefold()
            if "area-52\\patch-d.mpq" in lowered:
                return (1000, lowered)
            if "patch-" in lowered:
                return (ord(lowered.rsplit("patch-", 1)[1][:1]) if lowered.rsplit("patch-", 1)[1][:1].isalpha() else 0,
                        lowered)
            return (0, lowered)
        return max(archives, key=rank)

    def locate(self, name: str):
        for candidate in self.candidates(name):
            key = self.key(candidate)
            if key in self.target:
                return {"path": candidate, "source": None}
            if key in self.source:
                return {"path": candidate, "source": self.best_archive(self.source[key])}
        return None

    def require(self, name: str) -> dict:
        name = self.display_path(name)
        if not name or not ntpath.basename(name):
            raise ValueError(f"Invalid empty client asset path: {name!r}")
        result = self.locate(name)
        if result is None:
            raise ValueError(f"Client asset is absent from both installed and Ascension MPQs: {name}")
        key = self.key(name)
        self.references[key] = name
        self.plan[key] = {"reference": name, **result,
                          "availability": "ascension_source" if result["source"] else "installed_client"}
        if result["source"]:
            physical_key = self.key(result["path"])
            self.package_files[physical_key] = {"path": result["path"], "source": result["source"]}
        return result


class Merger:
    def __init__(self, source: Path, base: Path, asset_index: AssetIndex,
                 compatibility: dict, source_manifest: dict):
        self.source = {name: DBC(source / f"{name}.dbc") for name in ("Spell", *SUPPORT)}
        self.target = {name: DBC(base / f"{name}.dbc") for name in ("Spell", *SUPPORT)}
        self.asset_index = asset_index
        self.rows, self.used, self.mapped, self.added = {}, {}, {}, {}
        for name, table in self.source.items():
            other = self.target[name]
            if (table.fields, table.size) != (other.fields, other.size):
                raise ValueError(f"Incompatible class/client DBC layout: {name}")
            self.rows[name] = {table.words(row)[0]: table.words(row) for row in table.records}
            self.used[name] = {other.words(row)[0] for row in other.records}
            self.mapped[name], self.added[name] = {0: 0}, []
        self.assets = set()
        self.visual_repairs = []
        self.sound_repairs = {"wav_substitutions": 0, "muted_files": 0}
        self.compatibility = compatibility
        expected_hashes = compatibility["source_sha256"]
        for name, expected in expected_hashes.items():
            if source_manifest["sha256"].get(name) != expected:
                raise ValueError(f"Vanilla compatibility definitions do not match source {name}")
        repair_file = Path(__file__).parent / "catalog/ascension_visual_repairs.json"
        repairs = json.loads(repair_file.read_text(encoding="utf-8"))
        visual_hash = hashlib.sha256((source / "SpellVisual.dbc").read_bytes()).hexdigest()
        if repairs["format"] != 1 or repairs["source_sha256"] != visual_hash:
            raise ValueError("Authored class visual repairs do not match the installed source revision")
        for definition in repairs["visuals"]:
            identity, template = definition["id"], definition["template"]
            if identity in self.rows["SpellVisual"] or template not in self.rows["SpellVisual"]:
                raise ValueError(f"Invalid authored visual definition {identity}")
            row = list(self.rows["SpellVisual"][template])
            row[0] = identity
            self.rows["SpellVisual"][identity] = row
        if repairs["icon_source_sha256"] != hashlib.sha256((source / "SpellIcon.dbc").read_bytes()).hexdigest():
            raise ValueError("Authored class icon repairs do not match the installed source revision")
        for definition in repairs["icons"]:
            identity, template = definition["id"], definition["template"]
            if identity in self.rows["SpellIcon"] or template not in self.rows["SpellIcon"]:
                raise ValueError(f"Invalid authored icon definition {identity}")
            row = list(self.rows["SpellIcon"][template])
            row[0] = identity
            self.rows["SpellIcon"][identity] = row
        self.repairs = repairs
        if repairs["kit_source_sha256"] != hashlib.sha256((source / "SpellVisualKit.dbc").read_bytes()).hexdigest():
            raise ValueError("Authored class kit repairs do not match the installed source revision")
        for definition in repairs["kits"]:
            identity, template = definition["id"], definition["template"]
            if identity in self.rows["SpellVisualKit"] or template not in self.rows["SpellVisualKit"]:
                raise ValueError(f"Invalid authored kit definition {identity}")
            row = list(self.rows["SpellVisualKit"][template])
            row[0] = identity
            self.rows["SpellVisualKit"][identity] = row
        if repairs["effect_source_sha256"] != hashlib.sha256((source / "SpellVisualEffectName.dbc").read_bytes()).hexdigest():
            raise ValueError("Authored class effect repairs do not match the installed source revision")
        for definition in repairs["effects"]:
            identity, template = definition["id"], definition["template"]
            if identity in self.rows["SpellVisualEffectName"] or template not in self.rows["SpellVisualEffectName"]:
                raise ValueError(f"Invalid authored effect definition {identity}")
            row = list(self.rows["SpellVisualEffectName"][template])
            row[0] = identity
            self.rows["SpellVisualEffectName"][identity] = row

    def _record_sound_assets(self, row: list[int]) -> None:
        target = self.target["SoundEntries"]
        directory = target.text(row[23]).rstrip("\\/") if row[23] else ""
        for column in range(3, 13):
            if not row[column]:
                continue
            filename = target.text(row[column]).strip()
            if not filename:
                row[column] = 0
                continue
            path = (directory + "\\" + filename).lstrip("\\") if directory else filename
            self.asset_index.require(path)

    def copy(self, name: str, identity: int) -> int:
        if identity in (0, 0xFFFFFFFF):
            return identity
        if identity in self.mapped[name]:
            return self.mapped[name][identity]
        source, target = self.source[name], self.target[name]
        if identity not in self.rows[name]:
            raise ValueError(f"Missing {name} dependency {identity}")
        row = list(self.rows[name][identity])
        if name == "Spell" and identity in self.used[name]:
            if row[208] in (29, 31):
                raise ValueError(f"Ascension class spell ID collides with existing client spell {identity}")
            # A vanilla reference is satisfied by the existing vanilla record.
            self.mapped[name][identity] = identity
            return identity
        # Summon properties referenced by native class contracts must be identical.
        if name == "SummonProperties" and identity in (61, 63, 64):
            if target.words(target.row(identity)) != row:
                raise ValueError(f"Native class summon property {identity} differs between clients")
            self.mapped[name][identity] = identity
            return identity
        relocated = identity if identity not in self.used[name] else max(self.used[name]) + 1
        if relocated > 0x7FFFFFFF:
            raise ValueError(f"DBC ID space exhausted: {name}")
        self.used[name].add(relocated)
        self.mapped[name][identity] = relocated  # publish before traversing cyclic sound links
        row[0] = relocated
        for column, dependency in REFERENCES.get(name, {}).items():
            row[column] = self.copy(dependency, row[column])
        if name == "Spell":
            for index in range(3):
                effect = row[71 + index]
                if effect in (28, 56):
                    row[113 + index] = self.copy("SummonProperties", row[113 + index])
                elif effect == 168:
                    row[110 + index] = self.copy("SpellVisualKit", row[110 + index])
        for column in STRINGS.get(name, ()):
            row[column] = target.string(source.text(row[column])) if row[column] else 0
        if name == "SpellIcon" and row[1]:
            self.asset_index.require(target.text(row[1]) + ".blp")
        if name == "SpellVisualEffectName" and row[2]:
            path = target.text(row[2]).strip()
            if path:
                resolved = self.asset_index.require(path)
                if resolved["path"].casefold() != path.casefold():
                    row[2] = target.string(resolved["path"])
        if name == "CreatureModelData" and row[2]:
            path = target.text(row[2]).strip()
            if path:
                resolved = self.asset_index.require(path)
                if resolved["path"].casefold() != path.casefold():
                    row[2] = target.string(resolved["path"])
        if name == "SoundEntries":
            self._record_sound_assets(row)
        target.records.append(bytearray(struct.pack(f"<{target.fields}I", *row)))
        self.added[name].append(identity)
        return relocated


def compile_bundle(source: Path, base: Path, world: Path, output: Path,
                   asset_inventory: Path, asset_alternates: Path):
    if output.exists():
        raise ValueError(f"Refusing to replace staged class bundle {output}")
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    for name, digest in manifest["sha256"].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"Class source changed: {name}")
    world_manifest = json.loads((world / "manifest.json").read_text(encoding="utf-8"))
    compatibility = json.loads((Path(__file__).parent / "catalog/ascension_client_compat.json").read_text(encoding="utf-8"))
    if compatibility["world_snapshot_sha256"] != world_manifest["snapshot_sha256"]:
        raise ValueError("Client compatibility metadata does not match the pinned class world snapshot")
    asset_index = AssetIndex(asset_inventory, asset_alternates)
    merger = Merger(source, base, asset_index, compatibility, manifest)
    for identity in sorted(merger.rows["Spell"]):
        merger.copy("Spell", identity)
    for display in world_manifest["displays"]:
        merger.copy("CreatureDisplayInfo", display)
    # Attachments are indexed by their parent kit rather than referenced from it.
    for identity, row in merger.rows["SpellVisualKitModelAttach"].items():
        if row[1] in merger.mapped["SpellVisualKit"] and row[1]:
            merger.copy("SpellVisualKitModelAttach", identity)
    # Display texture variations are relative to the model's directory.
    display_table, model_table = merger.source["CreatureDisplayInfo"], merger.source["CreatureModelData"]
    for identity in world_manifest["displays"]:
        row = merger.rows["CreatureDisplayInfo"][identity]
        model = merger.rows["CreatureModelData"][row[1]]
        filename = model_table.text(model[2]).replace("/", "\\")
        directory = filename.rsplit("\\", 1)[0]
        for offset in row[6:9]:
            if offset:
                texture = display_table.text(offset).strip()
                if texture:
                    path = (texture if "\\" in texture else directory + "\\" + texture) + ".blp"
                    asset_index.require(path)
    dbc = output / "DBFilesClient"
    dbc.mkdir(parents=True)
    # Keep the already-authored Dracthyr tables and racial spells in this integrated patch.
    for path in base.glob("*.dbc"):
        if path.stem in ("ChrRaces", "CharSections", "CharBaseInfo", "CharStartOutfit", "SkillRaceClassInfo"):
            shutil.copyfile(path, dbc / path.name)
    for name, table in merger.target.items():
        if merger.added[name] or name == "Spell":
            table.records.sort(key=lambda record: struct.unpack_from("<I", record)[0])
            table.save(dbc / f"{name}.dbc")
    mapping = {name: {str(original): target for original, target in values.items() if original and target != original}
               for name, values in merger.mapped.items()}
    remapped_world = remap_display_ids(
        (world / "class-world.sql").read_text(encoding="utf-8"),
        {int(source_id): target_id for source_id, target_id in mapping["CreatureDisplayInfo"].items()},
        set(),
    )
    remapped_world = ensure_preceding_deletes(remapped_world)
    (output / "class-world.sql").write_text(remapped_world, encoding="utf-8", newline="\n")
    result = {"format": 1, "status": "ready_to_package_and_install",
              "source": str(source.resolve()), "world": str(world.resolve()),
              "added": {name: len(values) for name, values in merger.added.items()},
              "authored_visuals": merger.repairs,
              "id_map": mapping,
              "assets": sorted((item["path"] for item in asset_index.package_files.values()), key=str.casefold),
              "asset_sources": {item["path"]: item["source"] for item in asset_index.package_files.values()},
              "asset_references": sorted(asset_index.references.values(), key=str.casefold),
              "asset_plan": sorted(asset_index.plan.values(), key=lambda item: item["reference"].casefold()),
              "asset_repairs": {"visual_effects": merger.visual_repairs,
                                "sound_files": merger.sound_repairs,
                                "world_displays": {}},
              "class_world_sql_sha256": hashlib.sha256(remapped_world.encode("utf-8")).hexdigest(),
              "sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(dbc.glob("*.dbc"))}}
    (output / "manifest.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    (dbc / "manifest.json").write_text(json.dumps({"format": 1, "sha256": result["sha256"]}, indent=2) + "\n",
                                       encoding="utf-8")
    return {"added": result["added"], "assets_to_package": len(result["assets"]),
            "asset_references": len(result["asset_references"]),
            "asset_repairs": {key: len(value) if isinstance(value, list) else value for key, value in result["asset_repairs"].items()},
            "status": result["status"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "base", "world", "output", "asset_inventory", "asset_alternates"):
        parser.add_argument("--" + name.replace("_", "-"), dest=name, type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(compile_bundle(args.source, args.base, args.world, args.output,
                                        args.asset_inventory, args.asset_alternates)))
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, f"ADVAI CLASS BUNDLE FAILURE: {error}\n")
