"""Apply the ADVAI preset to a separate playerbots configuration file."""

import argparse
from pathlib import Path
import re


def merge(source: str, preset: str) -> str:
    assignments = {}
    for line in preset.splitlines():
        match = re.fullmatch(r"\s*(Playerbots\.[\w.]+)\s*=\s*(.*?)\s*", line)
        if match:
            if match[1] in assignments:
                raise ValueError(f"Duplicate preset setting: {match[1]}")
            assignments[match[1]] = match[2]
    seen = set()
    output = []
    for line in source.splitlines():
        match = re.match(r"\s*(Playerbots\.[\w.]+)\s*=", line)
        if match and match[1] in assignments:
            if match[1] in seen:
                raise ValueError(f"Duplicate configuration setting: {match[1]}")
            seen.add(match[1])
            output.append(f"{match[1]} = {assignments[match[1]]}")
        else:
            output.append(line)
    missing = assignments.keys() - seen
    if missing:
        raise ValueError(f"Preset references unknown settings: {', '.join(sorted(missing))}")
    return "\n".join(output) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if args.source.resolve() == args.output.resolve() or args.output.exists():
        parser.error("Output must be a new file separate from the source configuration")
    text = merge(args.source.read_text(encoding="utf-8"),
                 (root / "conf/playerbots.advai.preset").read_text(encoding="utf-8"))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
