"""Format modified C++ ranges and new C++ files without rewriting unrelated code."""

import argparse
from pathlib import Path
import re
import subprocess


def run(*args: str) -> str:
    return subprocess.run(args, check=True, text=True, capture_output=True, encoding="utf-8").stdout


def format_changed(formatter: Path, path: Path, arguments: list[str]) -> bool:
    formatted = run(str(formatter), *arguments, str(path))
    if path.read_text(encoding="utf-8") == formatted:
        return False
    path.write_text(formatted, encoding="utf-8", newline="\n")
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    ranges: dict[str, list[str]] = {}
    current = None
    diff = run("git", "-C", str(root), "diff", "--unified=0", "--", "*.cpp", "*.h")
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = line[6:]
        elif current and (match := re.match(r"@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)):
            start, count = int(match[1]), int(match[2] or "1")
            if count:
                ranges.setdefault(current, []).append(f"--lines={start}:{start + count - 1}")
    changed = 0
    for file, lines in ranges.items():
        changed += format_changed(args.clang_format, root / file, lines)
    new = run("git", "-C", str(root), "ls-files", "--others", "--exclude-standard", "--", "*.cpp", "*.h")
    for file in new.splitlines():
        changed += format_changed(args.clang_format, root / file, [])
    print(f"Checked {len(ranges)} changed files and {len(new.splitlines())} new files; reformatted {changed}")
