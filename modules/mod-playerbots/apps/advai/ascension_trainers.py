"""Generate native 3.3.5 class trainers for Cultist and Necromancer."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src" / "Advai" / "Ascension" / "AdvaiAscensionData.h"
PLAYERBOT_SQL = ROOT / "data" / "sql" / "world" / "updates" / "2026_10_17_01_advai_class_trainers.sql"
WOWSERVER_SQL = (ROOT.parent / "WowServer" / "modules" / "mod-ascension-classes" /
                 "data" / "sql" / "db-world" / "20261017_02_ascension_class_trainers.sql")

CREATURE_IDS = {12: 96000, 13: 96001}
TRAINER_IDS = {12: 96000, 13: 96001}
DONOR_CLASSES = {12: 5, 13: 9}
NAMES = {12: "Cultist Trainer", 13: "Necromancer Trainer"}

NODE_RE = re.compile(
    r"^\s*\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),"
    r"\s*\{\{([^}]*)\}\},\s*\{\{([^}]*)\}\}\},?\s*$"
)


def integer_list(value: str, *, keep_zero: bool = False) -> list[int]:
    values = [int(part.strip()) for part in value.split(",")]
    return values if keep_zero else [value for value in values if value]


def scale_level(level: int) -> int:
    if level <= 1:
        return 1
    return min(80, 1 + ((level - 1) * 79 + 48) // 49)


def read_nodes() -> list[dict]:
    nodes = []
    for line in HEADER.read_text(encoding="utf-8").splitlines():
        match = NODE_RE.match(line)
        if not match:
            continue
        node_id, class_id, spec_id, level, _ae, _te, _group = map(int, match.groups()[:7])
        if class_id not in CREATURE_IDS:
            continue
        nodes.append({
            "id": node_id,
            "class": class_id,
            "spec": spec_id,
            "level": level,
            "required": integer_list(match.group(8)),
            "spells": integer_list(match.group(9)),
        })
    if not nodes or {class_id for class_id in CREATURE_IDS} != {node["class"] for node in nodes}:
        raise ValueError("Cultist/Necromancer class nodes are missing from AdvaiAscensionData.h")
    if len({node["id"] for node in nodes}) != len(nodes):
        raise ValueError("Duplicate class advancement node IDs")
    return nodes


def spell_rows(nodes: list[dict], class_id: int) -> list[tuple[int, int, int, int, int, int, int, int, int]]:
    by_id = {node["id"]: node for node in nodes}
    class_nodes = {node["id"]: node for node in nodes if node["class"] == class_id}
    state: dict[int, int] = {}
    base_levels: dict[int, int] = {}

    def node_level(node_id: int) -> int:
        if state.get(node_id) == 1:
            raise ValueError(f"Cyclic prerequisite chain at class node {node_id}")
        if node_id in base_levels:
            return base_levels[node_id]
        node = class_nodes[node_id]
        state[node_id] = 1
        level = scale_level(node["level"])
        for required_id in node["required"]:
            required = by_id.get(required_id)
            if not required or required["class"] != class_id:
                raise ValueError(f"Invalid prerequisite {required_id} on class node {node_id}")
            level = max(level, node_level(required_id))
        state[node_id] = 2
        base_levels[node_id] = level
        return level

    # spell -> level, prerequisites; use the earliest native trainer unlock when a
    # spell is shared by more than one class advancement node.
    spells: dict[int, tuple[int, tuple[int, ...]]] = {}
    for node in class_nodes.values():
        level = node_level(node["id"])
        prereqs = tuple(dict.fromkeys(class_nodes[parent]["spells"][0] for parent in node["required"]))
        previous_rank = 0
        for rank, spell_id in enumerate(node["spells"]):
            rank_level = min(80, level + rank * 8)
            rank_prereqs = (previous_rank,) if previous_rank else prereqs
            existing = spells.get(spell_id)
            if existing is None or rank_level < existing[0]:
                spells[spell_id] = (rank_level, rank_prereqs[:3])
            elif rank_level == existing[0] and len(rank_prereqs) < len(existing[1]):
                spells[spell_id] = (rank_level, rank_prereqs[:3])
            previous_rank = spell_id

    return [
        (TRAINER_IDS[class_id], spell_id, level * level * 10, 0, 0,
         *(list(prereqs) + [0, 0, 0])[:3], level)
        for spell_id, (level, prereqs) in sorted(spells.items())
    ]


def render(nodes: list[dict]) -> str:
    lines = [
        "-- Native 3.3.5 trainers for standalone Cultist (12) and Necromancer (13).",
        "-- Copies each donor class's ordinary trainer spells and adds the class-specific catalog.",
        "-- Class-specific ability levels are scaled from the source level range to 1-80.",
        "-- No SkillCardData or Ascension advancement UI is required for spell learning.",
        "DELIMITER $$",
        "DROP PROCEDURE IF EXISTS `advai_assert_class_trainers`$$",
        "CREATE PROCEDURE `advai_assert_class_trainers`() BEGIN",
        "IF EXISTS (SELECT 1 FROM `creature_template` WHERE `entry`=96000 AND `name`<>'Cultist Trainer') OR",
        "   EXISTS (SELECT 1 FROM `creature_template` WHERE `entry`=96001 AND `name`<>'Necromancer Trainer') OR",
        "   EXISTS (SELECT 1 FROM `creature` c LEFT JOIN `creature_template` ct ON ct.`entry`=c.`id` WHERE c.`id`=96000 AND (ct.`entry` IS NULL OR ct.`name`<>'Cultist Trainer')) OR",
        "   EXISTS (SELECT 1 FROM `creature` c LEFT JOIN `creature_template` ct ON ct.`entry`=c.`id` WHERE c.`id`=96001 AND (ct.`entry` IS NULL OR ct.`name`<>'Necromancer Trainer')) OR",
        "   EXISTS (SELECT 1 FROM `trainer` WHERE (`Id`=96000 AND (`Type`<>0 OR `Requirement`<>12)) OR (`Id`=96001 AND (`Type`<>0 OR `Requirement`<>13))) THEN",
        "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Standalone class trainer ID conflicts with existing world content';",
        "END IF;",
        "IF NOT EXISTS (SELECT 1 FROM `creature` c JOIN `creature_default_trainer` d ON d.`CreatureId`=c.`id` JOIN `trainer` t ON t.`Id`=d.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=5) OR",
        "   NOT EXISTS (SELECT 1 FROM `creature` c JOIN `creature_default_trainer` d ON d.`CreatureId`=c.`id` JOIN `trainer` t ON t.`Id`=d.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=9) THEN",
        "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Could not locate spawned Priest and Warlock class trainers';",
        "END IF; END$$",
        "CALL `advai_assert_class_trainers`()$$",
        "DROP PROCEDURE `advai_assert_class_trainers`$$",
        "DELIMITER ;",
        "DELETE FROM `creature` WHERE `id` IN (96000,96001);",
        "DELETE FROM `creature_template_model` WHERE `CreatureID` IN (96000,96001);",
        "DELETE FROM `creature_default_trainer` WHERE `CreatureId` IN (96000,96001);",
        "DELETE FROM `creature_template` WHERE `entry` IN (96000,96001);",
        "DELETE FROM `trainer_spell` WHERE `TrainerId` IN (96000,96001);",
        "DELETE FROM `trainer` WHERE `Id` IN (96000,96001);",
        "DROP TEMPORARY TABLE IF EXISTS `advai_cultist_trainer_template`,`advai_necromancer_trainer_template`,",
        "`advai_cultist_trainer_model`,`advai_necromancer_trainer_model`,",
        "`advai_cultist_trainer_spawns`,`advai_necromancer_trainer_spawns`;",
        "INSERT INTO `trainer` (`Id`,`Type`,`Requirement`,`Greeting`,`VerifiedBuild`) VALUES",
        "(96000,0,12,'Train your Cultist abilities.',12340),",
        "(96001,0,13,'Train your Necromancer abilities.',12340);",
        "INSERT IGNORE INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`) ",
        "SELECT 96000,ts.`SpellId`,ts.`MoneyCost`,ts.`ReqSkillLine`,ts.`ReqSkillRank`,ts.`ReqAbility1`,ts.`ReqAbility2`,ts.`ReqAbility3`,ts.`ReqLevel` ",
        "FROM `trainer_spell` ts JOIN `trainer` t ON t.`Id`=ts.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=5;",
        "INSERT IGNORE INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`) ",
        "SELECT 96001,ts.`SpellId`,ts.`MoneyCost`,ts.`ReqSkillLine`,ts.`ReqSkillRank`,ts.`ReqAbility1`,ts.`ReqAbility2`,ts.`ReqAbility3`,ts.`ReqLevel` ",
        "FROM `trainer_spell` ts JOIN `trainer` t ON t.`Id`=ts.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=9;",
    ]

    for class_id in (12, 13):
        rows = spell_rows(nodes, class_id)
        lines.extend([
            f"-- {len(rows)} Ascension class spells for {NAMES[class_id]}.",
            "INSERT IGNORE INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`) VALUES",
            ",\n".join("(" + ",".join(map(str, row)) + ")" for row in rows) + ";",
        ])

    lines += [
        "SET @cultist_trainer_source := (SELECT d.`CreatureId` FROM `creature_default_trainer` d JOIN `trainer` t ON t.`Id`=d.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=5 ORDER BY d.`CreatureId` LIMIT 1);",
        "SET @necromancer_trainer_source := (SELECT d.`CreatureId` FROM `creature_default_trainer` d JOIN `trainer` t ON t.`Id`=d.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=9 ORDER BY d.`CreatureId` LIMIT 1);",
        "CREATE TEMPORARY TABLE `advai_cultist_trainer_template` AS SELECT * FROM `creature_template` WHERE `entry`=@cultist_trainer_source;",
        "UPDATE `advai_cultist_trainer_template` SET `entry`=96000,`name`='Cultist Trainer',`subname`='Class Trainer',`faction`=35,`npcflag`=(`npcflag` | 16),`trainer_type`=0,`trainer_class`=12,`trainer_race`=0,`trainer_spell`=0,`AIName`='',`ScriptName`='';",
        "INSERT INTO `creature_template` SELECT * FROM `advai_cultist_trainer_template`;",
        "CREATE TEMPORARY TABLE `advai_necromancer_trainer_template` AS SELECT * FROM `creature_template` WHERE `entry`=@necromancer_trainer_source;",
        "UPDATE `advai_necromancer_trainer_template` SET `entry`=96001,`name`='Necromancer Trainer',`subname`='Class Trainer',`faction`=35,`npcflag`=(`npcflag` | 16),`trainer_type`=0,`trainer_class`=13,`trainer_race`=0,`trainer_spell`=0,`AIName`='',`ScriptName`='';",
        "INSERT INTO `creature_template` SELECT * FROM `advai_necromancer_trainer_template`;",
        "CREATE TEMPORARY TABLE `advai_cultist_trainer_model` AS SELECT * FROM `creature_template_model` WHERE `CreatureID`=@cultist_trainer_source;",
        "UPDATE `advai_cultist_trainer_model` SET `CreatureID`=96000;",
        "INSERT INTO `creature_template_model` SELECT * FROM `advai_cultist_trainer_model`;",
        "CREATE TEMPORARY TABLE `advai_necromancer_trainer_model` AS SELECT * FROM `creature_template_model` WHERE `CreatureID`=@necromancer_trainer_source;",
        "UPDATE `advai_necromancer_trainer_model` SET `CreatureID`=96001;",
        "INSERT INTO `creature_template_model` SELECT * FROM `advai_necromancer_trainer_model`;",
        "INSERT INTO `creature_default_trainer` (`CreatureId`,`TrainerId`) VALUES (96000,96000),(96001,96001);",
        "SET @trainer_guid_offset := (SELECT COALESCE(MAX(`guid`),0)+1 FROM `creature`);",
        "CREATE TEMPORARY TABLE `advai_cultist_trainer_spawns` AS SELECT c.* FROM `creature` c JOIN `creature_default_trainer` d ON d.`CreatureId`=c.`id` JOIN `trainer` t ON t.`Id`=d.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=5;",
        "UPDATE `advai_cultist_trainer_spawns` SET `guid`=`guid`+@trainer_guid_offset,`id`=96000,`position_x`=`position_x`+0.75;",
        "INSERT INTO `creature` SELECT * FROM `advai_cultist_trainer_spawns`;",
        "SET @trainer_guid_offset := (SELECT COALESCE(MAX(`guid`),0)+1 FROM `creature`);",
        "CREATE TEMPORARY TABLE `advai_necromancer_trainer_spawns` AS SELECT c.* FROM `creature` c JOIN `creature_default_trainer` d ON d.`CreatureId`=c.`id` JOIN `trainer` t ON t.`Id`=d.`TrainerId` WHERE t.`Type`=0 AND t.`Requirement`=9;",
        "UPDATE `advai_necromancer_trainer_spawns` SET `guid`=`guid`+@trainer_guid_offset,`id`=96001,`position_x`=`position_x`+0.75;",
        "INSERT INTO `creature` SELECT * FROM `advai_necromancer_trainer_spawns`;",
        "DROP TEMPORARY TABLE `advai_cultist_trainer_template`,`advai_necromancer_trainer_template`,",
        "`advai_cultist_trainer_model`,`advai_necromancer_trainer_model`,",
        "`advai_cultist_trainer_spawns`,`advai_necromancer_trainer_spawns`;",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="validate generated output without writing files")
    args = parser.parse_args()
    sql = render(read_nodes())
    if args.check:
        print(f"Validated native class trainer SQL ({sql.count('INSERT IGNORE INTO `trainer_spell`') - 2} class spell groups)")
        return
    for path in (PLAYERBOT_SQL, WOWSERVER_SQL):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(sql, encoding="utf-8", newline="\n")
        print(path)


if __name__ == "__main__":
    main()
