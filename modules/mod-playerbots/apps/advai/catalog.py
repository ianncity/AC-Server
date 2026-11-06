"""Validate authored ADVAI catalogs and compile their repeatable SQL migration."""

import argparse
import json
from pathlib import Path
import re
import sys

LOCALES = ("enUS", "koKR", "frFR", "deDE", "zhCN", "zhTW", "esES", "esMX", "ruRU")
REQUIRED_LOCALES = ("enUS",)
INTENTS = (
    "unknown greeting farewell thanks apology congratulations introduction activity location quest help group "
    "dungeon raid battleground arena trade buy sell profession equipment class guild friend ready wait follow "
    "stop combat death wipe joke smalltalk disagree clarify smalltalk_answer acknowledge group_offer progress trade_offer group_accept trade_cancel friend_remembered quest_known "
    "event_death event_wipe event_victory event_quest party_wait party_ready item_known spell_known "
    "pvp_focus pvp_regroup pvp_peel advice emote duel gold opinion playtime summon"
).split()
ROOT = Path(__file__).resolve().parents[2]


def read_facts(path: Path) -> dict[str, str]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or not data:
        raise ValueError(f"Factual catalog must be a nonempty object: {path}")
    for key, text in data.items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", key) or not isinstance(text, str) or not text.strip():
            raise ValueError(f"Invalid factual template {path}:{key}")
        tokens = re.findall(r"\{([0-3])\}", text)
        placeholders = set(tokens)
        remainder = re.sub(r"\{[0-3]\}", "", text)
        if ("{" in remainder or "}" in remainder or len(tokens) != len(placeholders)
                or placeholders != {str(i) for i in range(len(placeholders))}):
            raise ValueError(f"Invalid factual placeholders {path}:{key}")
        if len(text.encode("utf-8")) > 200:
            raise ValueError(f"Oversized factual template {path}:{key}")
    return data


def factual_catalogs(directory: Path, locales=REQUIRED_LOCALES) -> dict[str, dict[str, str]]:
    source = read_facts(directory / "facts/enUS.json")
    result = {}
    for locale in locales:
        path = directory / "facts" / f"{locale}.json"
        if not path.is_file():
            raise ValueError(f"Required factual catalog is missing: {path}")
        result[locale] = read_facts(path)
        if result[locale].keys() != source.keys():
            missing = sorted(source.keys() - result[locale].keys())
            extra = sorted(result[locale].keys() - source.keys())
            raise ValueError(f"Factual catalog keys differ for {locale}: missing={missing}, extra={extra}")
        for key, text in source.items():
            if set(re.findall(r"\{[0-3]\}", text)) != set(re.findall(r"\{[0-3]\}", result[locale][key])):
                raise ValueError(f"Factual translation placeholder mismatch: {locale}:{key}")
    return result


def sql_migration(rows: list[str], keys: list[str]) -> str:
    quoted_keys = ", ".join(f"'{key}'" for key in keys)
    return (
        "-- Generated from apps/advai/catalog by apps/advai/catalog.py.\n"
        "START TRANSACTION;\n"
        f"DELETE FROM `ai_playerbot_texts` WHERE `name` IN ({quoted_keys});\n"
        "INSERT INTO `ai_playerbot_texts`\n"
        "(`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, "
        "`text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES\n"
        + ",\n".join(rows) + ";\nCOMMIT;\n"
    )


def factual_rows(catalogs: dict[str, dict[str, str]]) -> list[str]:
    rows = []
    for key in catalogs[next(iter(catalogs))]:
        texts = [catalogs[locale][key] if locale in catalogs else "" for locale in LOCALES]
        values = ["'" + text.replace("\\", "\\\\").replace("'", "''") + "'" for text in texts]
        rows.append(f"('advai_fact_{key}', {values[0]}, 0, 0, {', '.join(values[1:])})")
    return rows


def compile_facts(directory: Path) -> str:
    catalogs = factual_catalogs(directory)
    return sql_migration(factual_rows(catalogs), [f"advai_fact_{key}" for key in catalogs["enUS"]])


def fact_header(directory: Path) -> str:
    facts = read_facts(directory / "facts/enUS.json")
    rows = []
    for key, text in facts.items():
        count = len(set(re.findall(r"\{([0-3])\}", text)))
        rows.append(f'    {{"{key}", {count}}}')
    return ("/*\n * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright\n"
            " * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,\n"
            " * or (at your option) any later version.\n */\n"
            "// Generated from catalog/facts/enUS.json by apps/advai/catalog.py.\n"
            "#ifndef PLAYERBOTS_ADVAI_FACT_DEFINITIONS_H\n#define PLAYERBOTS_ADVAI_FACT_DEFINITIONS_H\n"
            "#include <cstddef>\n#include <string_view>\nnamespace Advai\n{\n"
            "struct FactDefinition { std::string_view Key; std::size_t Parameters; };\n"
            "inline constexpr FactDefinition FACT_DEFINITIONS[] = {\n" + ",\n".join(rows) +
            "\n};\n}\n#endif\n")


def read_catalog(path: Path) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    current = None
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            if current not in INTENTS or current in result:
                raise ValueError(f"{path}:{number}: invalid or repeated outcome {current}")
            result[current] = []
        elif current is None:
            raise ValueError(f"{path}:{number}: text outside an outcome")
        else:
            if len(line.encode("utf-8")) > 255:
                raise ValueError(f"{path}:{number}: client message limit exceeded")
            if re.search(r"%(?!facts\b)", line):
                raise ValueError(f"{path}:{number}: unknown placeholder")
            result[current].append(line)
    for intent in INTENTS:
        lines = result.get(intent, [])
        normalized = [re.sub(r"[^\w\s]", "", text.casefold()).strip() for text in lines]
        if len(lines) < 36 or len(set(normalized)) != len(lines):
            raise ValueError(f"{path}: {intent} needs 36 distinct authored lines; found {len(lines)}")
    return result


def compile_sql(directory: Path, only_locale: str | None = "enUS") -> str:
    catalogs = {}
    factual = factual_catalogs(directory, (only_locale,) if only_locale else REQUIRED_LOCALES)
    canonical_facts = factual[only_locale or "enUS"]
    for locale in REQUIRED_LOCALES if only_locale is None else (only_locale,):
        if only_locale and locale != only_locale:
            continue
        path = directory / f"{locale}.txt"
        if not path.is_file():
            raise ValueError(f"Required translated catalog is missing: {path}")
        catalogs[locale] = read_catalog(path)
    rows = []
    for intent in INTENTS:
        count = len(catalogs[only_locale or "enUS"][intent])
        for locale in catalogs:
            if len(catalogs[locale][intent]) != count:
                raise ValueError(f"Variant alignment failed for {locale}:{intent}")
        for variant in range(count):
            texts = [catalogs[locale][intent][variant] if locale in catalogs else "" for locale in LOCALES]
            placeholders = [re.findall(r"%\w+", text) for text in texts]
            populated = [tokens for text, tokens in zip(texts, placeholders) if text]
            if any(tokens != populated[0] for tokens in populated):
                raise ValueError(f"Translation placeholder mismatch: {intent}:{variant}")
            values = ["'" + text.replace("\\", "\\\\").replace("'", "''") + "'" for text in texts]
            rows.append(f"('advai_{intent}', {values[0]}, 0, 0, {', '.join(values[1:])})")
    rows.extend(factual_rows(factual))
    return sql_migration(rows, [*(f"advai_{intent}" for intent in INTENTS),
                                *(f"advai_fact_{key}" for key in canonical_facts)])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--locale", choices=LOCALES, help="Check a single authored catalog")
    parser.add_argument("--output", type=Path, help="Write SQL only after the required English catalog passes")
    parser.add_argument("--facts-header", type=Path, help="Generate the required factual-template contract")
    parser.add_argument("--facts-output", type=Path, help="Compile only the required English factual templates")
    args = parser.parse_args()
    directory = Path(__file__).parent / "catalog"
    try:
        if args.facts_header:
            args.facts_header.write_text(fact_header(directory), encoding="utf-8", newline="\n")
        if args.facts_output:
            args.facts_output.write_text(compile_facts(directory), encoding="utf-8", newline="\n")
            print("English factual catalog validated")
        elif args.locale:
            content = read_catalog(directory / f"{args.locale}.txt")
            compile_sql(directory, args.locale)
            print(f"{args.locale}: {len(content)} outcomes, {sum(map(len, content.values()))} authored variants")
            if args.output:
                args.output.write_text(compile_sql(directory, args.locale), encoding="utf-8", newline="\n")
        else:
            sql = compile_sql(directory)
            if args.output:
                args.output.write_text(sql, encoding="utf-8", newline="\n")
            print("English ADVAI catalog validated")
    except (ValueError, OSError) as error:
        print(f"ADVAI CATALOG FAILURE: {error}", file=sys.stderr)
        sys.exit(1)
