"""Package and read back the authored client DBCs using the official StormLib library."""

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
from pathlib import PureWindowsPath
import tempfile


class Storm:
    def __init__(self, library: Path):
        self.api = ctypes.WinDLL(str(library.resolve(strict=True)), use_last_error=True)
        signatures = {
            "SFileCreateArchive": ([wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                    ctypes.POINTER(wintypes.HANDLE)], ctypes.c_bool),
            "SFileAddFileEx": ([wintypes.HANDLE, wintypes.LPCWSTR, ctypes.c_char_p,
                                wintypes.DWORD, wintypes.DWORD, wintypes.DWORD], ctypes.c_bool),
            "SFileCloseArchive": ([wintypes.HANDLE], ctypes.c_bool),
            "SFileOpenArchive": ([wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  ctypes.POINTER(wintypes.HANDLE)], ctypes.c_bool),
            "SFileOpenFileEx": ([wintypes.HANDLE, ctypes.c_char_p, wintypes.DWORD,
                                 ctypes.POINTER(wintypes.HANDLE)], ctypes.c_bool),
            "SFileReadFile": ([wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                               ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p], ctypes.c_bool),
            "SFileGetFileSize": ([wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)], wintypes.DWORD),
            "SFileCloseFile": ([wintypes.HANDLE], ctypes.c_bool),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.api, name)
            function.argtypes, function.restype = arguments, result

    def require(self, success: bool, operation: str) -> None:
        if not success:
            raise OSError(ctypes.get_last_error(), f"StormLib {operation} failed")

    def read(self, archive: wintypes.HANDLE, name: str, max_bytes: int = 1024 * 1024 * 1024) -> bytes:
        handle = wintypes.HANDLE()
        self.require(self.api.SFileOpenFileEx(archive, name.encode("ascii"), 0, ctypes.byref(handle)), f"open {name}")
        try:
            high = wintypes.DWORD()
            size = self.api.SFileGetFileSize(handle, ctypes.byref(high))
            if high.value or size > max_bytes:
                raise ValueError(f"Unexpected DBC file size: {name}")
            data = ctypes.create_string_buffer(size)
            received = wintypes.DWORD()
            self.require(self.api.SFileReadFile(handle, data, size, ctypes.byref(received), None), "read file")
            if received.value != size:
                raise ValueError(f"Truncated archive member: {name}")
            return data.raw
        finally:
            self.require(self.api.SFileCloseFile(handle), "close file")


def package(bundle: Path, output: Path, library: Path) -> None:
    bundle = bundle.resolve(strict=True)
    manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
    if not manifest.get("asset_sources") and (bundle.parent / "manifest.json").exists():
        parent_manifest = json.loads((bundle.parent / "manifest.json").read_text(encoding="utf-8"))
        if parent_manifest.get("asset_sources"):
            bundle = bundle.parent
            manifest = parent_manifest
    dbc_directory = bundle / "DBFilesClient" if (bundle / "DBFilesClient").is_dir() else bundle
    if manifest["format"] != 1 or not manifest["sha256"]:
        raise ValueError("Unsupported or empty backport manifest")
    for name, digest in manifest["sha256"].items():
        if Path(name).name != name or not name.endswith(".dbc"):
            raise ValueError("Invalid manifest member name")
        if hashlib.sha256((dbc_directory / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"Bundle changed after generation: {name}")
    assets = manifest.get("assets", [])
    asset_sources = manifest.get("asset_sources", {})
    if set(assets) != set(asset_sources):
        raise ValueError("Every packaged client asset must have exactly one pinned Ascension source archive")
    if len({name.casefold() for name in assets}) != len(assets):
        raise ValueError("Class asset list contains duplicate case-insensitive MPQ paths")
    for name in assets:
        relative = PureWindowsPath(name)
        if relative.is_absolute() or ":" in name or ".." in relative.parts or not relative.parts:
            raise ValueError(f"Invalid client asset path: {name}")
    output = output.resolve()
    if output.exists():
        raise ValueError(f"Output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".building")
    if temporary.exists():
        raise ValueError(f"An unfinished archive needs investigation: {temporary}")
    report_path = output.with_suffix(output.suffix + ".manifest.json")
    if report_path.exists():
        raise ValueError(f"An existing archive report needs investigation: {report_path}")
    storm = Storm(library)
    member_hashes = {f"DBFilesClient\\{name}": digest for name, digest in manifest["sha256"].items()}
    source_archives = {}
    with tempfile.TemporaryDirectory(prefix="advai-ascension-assets-", dir=output.parent) as stage_name:
        stage = Path(stage_name)
        staged_assets = {}
        try:
            for name in assets:
                source_archive = asset_sources[name]
                if source_archive not in source_archives:
                    handle = wintypes.HANDLE()
                    storm.require(storm.api.SFileOpenArchive(source_archive, 0, 0x00000100, ctypes.byref(handle)),
                                  f"open Ascension asset archive {source_archive}")
                    source_archives[source_archive] = handle
                payload = storm.read(source_archives[source_archive], name)
                relative = PureWindowsPath(name)
                staged = stage.joinpath(*relative.parts)
                staged.parent.mkdir(parents=True, exist_ok=True)
                staged.write_bytes(payload)
                member_hashes[name.replace("/", "\\")] = hashlib.sha256(payload).hexdigest()
                staged_assets[name] = staged
        finally:
            for handle in source_archives.values():
                storm.require(storm.api.SFileCloseArchive(handle), "close Ascension asset archive")

        archive = wintypes.HANDLE()
        members = len(manifest["sha256"]) + len(staged_assets)
        max_files = 1 << max(6, (members * 2 - 1).bit_length())
        storm.require(storm.api.SFileCreateArchive(str(temporary), 0x00100000, max_files, ctypes.byref(archive)),
                      "create archive")
        try:
            for name in sorted(manifest["sha256"]):
                storm.require(storm.api.SFileAddFileEx(archive, str((dbc_directory / name).resolve()),
                              f"DBFilesClient\\{name}".encode("ascii"), 0x00000200, 0x02, 0x02), f"add {name}")
            for name in sorted(staged_assets, key=str.casefold):
                storm.require(storm.api.SFileAddFileEx(archive, str(staged_assets[name].resolve()),
                              name.replace("/", "\\").encode("ascii"), 0x00000200, 0x02, 0x02), f"add {name}")
        finally:
            storm.require(storm.api.SFileCloseArchive(archive), "close archive")
        storm.require(storm.api.SFileOpenArchive(str(temporary), 0, 0, ctypes.byref(archive)), "reopen archive")
        try:
            for name, digest in member_hashes.items():
                if hashlib.sha256(storm.read(archive, name)).hexdigest() != digest:
                    raise ValueError(f"MPQ member does not match its source hash: {name}")
        finally:
            storm.require(storm.api.SFileCloseArchive(archive), "close verified archive")
    temporary.replace(output)
    report = {"format": 1, "archive": str(output), "members": len(member_hashes),
              "dbc_tables": len(manifest["sha256"]), "assets": len(assets),
              "sha256": member_hashes, "archive_sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Created and verified {output} ({len(manifest['sha256'])} DBC tables, {len(assets)} assets)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stormlib", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        package(arguments.bundle, arguments.output, arguments.stormlib)
    except (ValueError, OSError) as error:
        parser.exit(1, f"ADVAI CLIENT PACKAGE FAILURE: {error}\n")
