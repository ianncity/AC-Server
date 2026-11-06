"""Compile the authored Dracthyr syllable catalog into explicit male/female name pools."""

from itertools import product
from pathlib import Path

START = "ar az bal bel cal dal dra ek es gal haz ir iz jar kal kor mal nal or raz".split()
MIDDLE = "ael al an ar ath dar en er eth in ir ith kar kel len nar or ral ren sar".split()
MALE = "ak ar as ath ax az ek er es eth ik ir is ith or os oth rak ren ryn".split()
FEMALE = "a ae ai ala ana ara asa atha aya ela ena era esa etha ia ila ina ira isa yna".split()


def compile_sql() -> str:
    rows = []
    used = set()
    for gender, endings in ((18, MALE), (19, FEMALE)):
        for first, middle, end in product(START, MIDDLE, endings):
            name = (first + middle + end).capitalize()
            if name.casefold() in used:
                continue
            if not name.isascii() or not name.isalpha() or not 2 <= len(name) <= 12:
                raise ValueError(f"Invalid authored name: {name}")
            used.add(name.casefold())
            rows.append((name, gender))
    values = ",\n".join(f"('{name}',{gender})" for name, gender in rows)
    return (
        "-- Generated from apps/advai/backport_names.py; dedicated Dracthyr categories 18 and 19.\n"
        "START TRANSACTION;\n"
        "DROP TEMPORARY TABLE IF EXISTS advai_dracthyr_names;\n"
        "CREATE TEMPORARY TABLE advai_dracthyr_names (name VARCHAR(12) PRIMARY KEY, gender TINYINT UNSIGNED);\n"
        "INSERT INTO advai_dracthyr_names VALUES\n" + values + ";\n"
        "DELETE n FROM playerbots_names n JOIN advai_dracthyr_names a ON n.name=a.name AND n.gender=a.gender;\n"
        "INSERT INTO playerbots_names (name_id,name,gender) "
        "SELECT last_id+ROW_NUMBER() OVER (ORDER BY a.name),a.name,a.gender FROM advai_dracthyr_names a "
        "CROSS JOIN (SELECT COALESCE(MAX(name_id),-1) AS last_id FROM playerbots_names) current_ids;\n"
        "DROP TEMPORARY TABLE advai_dracthyr_names;\nCOMMIT;\n"
    )


if __name__ == "__main__":
    target = Path(__file__).resolve().parents[2] / "data/sql/characters/updates/2026_10_17_01_advai_dracthyr_names.sql"
    target.write_text(compile_sql(), encoding="utf-8", newline="\n")
    print(f"Compiled dedicated Dracthyr name pools: {target}")
