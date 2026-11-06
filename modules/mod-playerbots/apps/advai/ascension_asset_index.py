"""Index client and Ascension MPQs for the pinned class asset references."""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import re


class Storm:
    def __init__(self, library: Path):
        self.api = ctypes.WinDLL(str(library.resolve(strict=True)), use_last_error=True)
        self.open = self.api.SFileOpenArchive
        self.open.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                              ctypes.POINTER(wintypes.HANDLE)]
        self.open.restype = ctypes.c_bool
        self.close = self.api.SFileCloseArchive
        self.close.argtypes = [wintypes.HANDLE]
        self.close.restype = ctypes.c_bool
        self.has = self.api.SFileHasFile
        self.has.argtypes = [wintypes.HANDLE, ctypes.c_char_p]
        self.has.restype = ctypes.c_bool
        self.open_patch = self.api.SFileOpenPatchArchive
        self.open_patch.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, ctypes.c_char_p, wintypes.DWORD]
        self.open_patch.restype = ctypes.c_bool

    @staticmethod
    def require(success: bool, operation: str) -> None:
        if not success:
            raise OSError(ctypes.get_last_error(), f"StormLib {operation} failed")

    def open_archives(self, root: Path, include_client_overlay: bool = False):
        paths = sorted(root.rglob("*.MPQ"), key=lambda path: str(path).casefold())
        if not paths:
            raise ValueError(f"No MPQ archives found below {root}")
        overlay = None
        if include_client_overlay:
            patches = {path.name.casefold(): path for path in paths}
            base, overlay = patches.get("patch-w.mpq"), patches.get("patch-wa.mpq")
            if base is None or overlay is None:
                raise ValueError("The supplied client must include both patch-W.MPQ and its patch-WA.MPQ overlay")
        handles = []
        try:
            for path in paths:
                if overlay and path == overlay:
                    continue
                handle = wintypes.HANDLE()
                self.require(self.open(str(path), 0, 0x00000100, ctypes.byref(handle)),
                             f"open read-only archive {path}")
                handles.append((path, handle))
            if overlay:
                base_handle = next((handle for path, handle in handles if path == base), None)
                if base_handle is None:
                    raise ValueError(f"Could not open base archive for overlay {overlay}")
                self.require(self.open_patch(base_handle, str(overlay), None, 0),
                             f"attach patch archive {overlay} to {base}")
            return handles, overlay
        except BaseException:
            for _, handle in handles:
                self.close(handle)
            raise

    def contains(self, handles, name: str) -> list[str]:
        encoded = name.replace("/", "\\").encode("ascii")
        for path, handle in handles:
            if self.has(handle, encoded):
                return [str(path)]
        return []


def alternate_name(name: str):
    lowered = name.casefold()
    if lowered.endswith(".mdx"):
        return name[:-4] + ".m2"
    return None


def best_source_order(paths):
    def rank(item):
        path = item[0] if isinstance(item, tuple) else item
        lowered = str(path).replace("/", "\\").casefold()
        if "area-52\\patch-d.mpq" in lowered:
            return (1000, lowered)
        match = re.search(r"patch-([a-z]+)\.mpq$", path.name.casefold())
        if match:
            return (ord(match.group(1)[0]), lowered)
        return (0, lowered)
    return sorted(paths, key=rank, reverse=True)


def scan(handles, references: list[str]):
    found = {}
    for index, reference in enumerate(references, 1):
        if not Path(reference.replace("\\", "/")).name:
            continue
        hits = handles["contains"](reference)
        if hits:
            found[reference] = hits
        if index % 500 == 0:
            print(f"Indexed {index}/{len(references)} referenced files", flush=True)
    return found


def compile_index(references_file: Path, source_manifest_path: Path, world_manifest_path: Path,
                  client_data: Path, source_data: Path, output: Path, alternates_output: Path,
                  library: Path):
    if output.exists() or alternates_output.exists():
        raise ValueError("Refusing to overwrite an existing MPQ inventory")
    references = json.loads(references_file.read_text(encoding="utf-8"))
    source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
    world_manifest = json.loads(world_manifest_path.read_text(encoding="utf-8"))
    if references["format"] != 1 or references["source_manifest_sha256"] != hashlib.sha256(
            source_manifest_path.read_bytes()).hexdigest():
        raise ValueError("Asset references do not match the installed Ascension DBC snapshot")
    if references["world_snapshot_sha256"] != world_manifest["snapshot_sha256"]:
        raise ValueError("Asset references do not match the class world snapshot")
    names = sorted(set(references["references"]), key=str.casefold)
    alternatives = {name: alternate_name(name) for name in names if alternate_name(name)}
    storm = Storm(library)
    target_handles, target_overlay = storm.open_archives(client_data, include_client_overlay=True)
    source_handles, _ = storm.open_archives(source_data)
    source_handles = best_source_order(source_handles)
    try:
        target = scan({"contains": lambda name: storm.contains(target_handles, name)}, names)
        source = scan({"contains": lambda name: storm.contains(source_handles, name)}, names)
        target_alternates = {}
        source_alternates = {}
        alternate_names = sorted(set(alternatives.values()), key=str.casefold)
        for name in alternate_names:
            if not any(name.casefold() == reference.casefold() for reference in names):
                hits = storm.contains(target_handles, name)
                if hits:
                    target_alternates[name] = hits
                hits = storm.contains(source_handles, name)
                if hits:
                    source_alternates[name] = hits
        target_paths = {name.casefold() for name in target}
        source_paths = {name.casefold() for name in source}
        target_alt_keys = {name.casefold() for name in target_alternates}
        source_alt_keys = {name.casefold() for name in source_alternates}
        missing = []
        for name in names:
            key = name.casefold()
            alternate = alternatives.get(name)
            alternate_key = alternate.casefold() if alternate else None
            if key not in target_paths | source_paths and alternate_key not in target_alt_keys | source_alt_keys:
                missing.append(name)
        inventory = {
            "format": 1,
            "reference_catalog_sha256": hashlib.sha256(references_file.read_bytes()).hexdigest(),
            "target": {"archivesOpened": len(target_handles) + 1, "archivesFailed": [],
                       "patchOverlays": {str(target_overlay): "patch-W.MPQ"}, "found": target},
            "source": {"archivesOpened": len(source_handles), "archivesFailed": [], "found": source},
            "unavailable_unmapped_inputs": missing,
        }
        alternate_index = {"format": 1, "alternates": alternatives,
                           "target": target_alternates, "source": source_alternates}
        output.parent.mkdir(parents=True, exist_ok=True)
        alternates_output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
        alternates_output.write_text(json.dumps(alternate_index, indent=2) + "\n", encoding="utf-8")
        return {"references": len(names), "target_opened": len(target_handles) + 1,
                "source_opened": len(source_handles), "target_found": len(target),
                "source_found": len(source), "target_alternates": len(target_alternates),
                "source_alternates": len(source_alternates), "unavailable_unmapped_inputs": len(missing)}
    finally:
        for _, handle in target_handles + source_handles:
            storm.close(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--references", required=True, type=Path)
    parser.add_argument("--source-manifest", required=True, type=Path)
    parser.add_argument("--world-manifest", required=True, type=Path)
    parser.add_argument("--client-data", required=True, type=Path)
    parser.add_argument("--source-data", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--alternates-output", required=True, type=Path)
    parser.add_argument("--stormlib", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(compile_index(args.references, args.source_manifest, args.world_manifest,
                                       args.client_data, args.source_data, args.output,
                                       args.alternates_output, args.stormlib)))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"ADVAI ASSET INDEX FAILURE: {error}\n")
