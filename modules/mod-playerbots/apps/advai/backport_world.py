"""Compile the optional class-content migration for the supported Playerbot world schema."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PETS = (
    (95000, 1863, "Skeletal Initiate", 200, 2, ""),
    (95001, 416, "Grave Mage", 1421, 8, ""),
    (95002, 416, "Skeletal Archer", 1420, 1, ""),
    (95003, 1860, "Flesh Golem", 10971, 2, ""),
    (95010, 19668, "Bound Tentacle", 25547, 2, "advai_void_servant"),
    (95011, 19668, "Faceless Servant", 25677, 8, "advai_void_servant"),
)
TRAINERS = (
    (95020, 376, 95020, 5, "Cultist Instructor", "Cultist"),
    (95021, 6018, 95020, 5, "Cultist Instructor", "Cultist"),
    (95022, 461, 95022, 9, "Necromancer Instructor", "Necromancer"),
    (95023, 3325, 95022, 9, "Necromancer Instructor", "Necromancer"),
)


def compile_sql() -> str:
    specs = json.loads(Path(__file__).with_name("backport_spells.json").read_text(encoding="utf-8"))
    sql = ["-- Opt-in Cultist/Necromancer content. Requires the matching ADVAI DBC bundle.",
           "-- Native Priest/Warlock class slots preserve the installed client protocol.",
           "START TRANSACTION;",
           "DROP TEMPORARY TABLE IF EXISTS `advai_class_template`;",
           "CREATE TEMPORARY TABLE `advai_class_template` LIKE `creature_template`;"]
    for entry, donor, name, model, unit_class, script in PETS:
        sql += [f"DELETE FROM `creature_template_model` WHERE `CreatureID`={entry};",
                f"DELETE FROM `creature_template_spell` WHERE `CreatureID`={entry};",
                f"DELETE FROM `creature_template` WHERE `entry`={entry};",
                "DELETE FROM `advai_class_template`;",
                f"INSERT INTO `advai_class_template` SELECT * FROM `creature_template` WHERE `entry`={donor};",
                f"UPDATE `advai_class_template` SET `entry`={entry},`name`='{name}',`subname`='',"
                f"`minlevel`=1,`maxlevel`=80,`type`={6 if entry < 95010 else 10},`family`=0,"
                f"`unit_class`={unit_class},`npcflag`=0,`rank`=0,`PetSpellDataId`=0,"
                "`lootid`=0,`pickpocketloot`=0,`skinloot`=0,`mingold`=0,`maxgold`=0,"
                f"`AIName`='',`ScriptName`='{script}';",
                "INSERT INTO `creature_template` SELECT * FROM `advai_class_template`;",
                "INSERT INTO `creature_template_model` "
                "(`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) "
                f"VALUES ({entry},0,{model},1,1,12340);",
                f"DELETE FROM `pet_levelstats` WHERE `creature_entry`={entry};",
                "INSERT INTO `pet_levelstats` "
                "(`creature_entry`,`level`,`hp`,`mana`,`armor`,`str`,`agi`,`sta`,`inte`,`spi`,`min_dmg`,`max_dmg`) "
                f"SELECT {entry},`level`,`hp`,`mana`,`armor`,`str`,`agi`,`sta`,`inte`,`spi`,`min_dmg`,`max_dmg` "
                f"FROM `pet_levelstats` WHERE `creature_entry`={donor};"]
    # Default pet spells are loaded from creature_template_spell for each native summon effect.
    sql += ["INSERT INTO `creature_template_spell` (`CreatureID`,`Index`,`Spell`,`VerifiedBuild`) VALUES",
            "(95000,0,7814,12340),(95001,0,363419,12340),(95002,0,363422,12340),"
            "(95003,0,3716,12340),(95010,0,63619,12340),(95011,0,363419,12340);"]
    for entry, donor, trainer, base_class, name, profile in TRAINERS:
        sql += [f"DELETE FROM `creature_template_model` WHERE `CreatureID`={entry};",
                f"DELETE FROM `creature_default_trainer` WHERE `CreatureId`={entry};",
                f"DELETE FROM `creature_template` WHERE `entry`={entry};",
                "DELETE FROM `advai_class_template`;",
                f"INSERT INTO `advai_class_template` SELECT * FROM `creature_template` WHERE `entry`={donor};",
                f"UPDATE `advai_class_template` SET `entry`={entry},`name`='{name}',"
                "`subname`='ADVAI Class Trainer',`npcflag`=17,`AIName`='',`ScriptName`='';",
                "INSERT INTO `creature_template` SELECT * FROM `advai_class_template`;",
                "INSERT INTO `creature_template_model` "
                "(`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) "
                f"SELECT {entry},`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild` "
                f"FROM `creature_template_model` WHERE `CreatureID`={donor};",
                "INSERT INTO `creature_default_trainer` (`CreatureId`,`TrainerId`) "
                f"VALUES ({entry},{trainer});",
                f"DELETE FROM `creature` WHERE `guid`={9500000 + entry} AND `id`={entry};",
                "INSERT INTO `creature` (`guid`,`id`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,"
                "`position_x`,`position_y`,`position_z`,`orientation`,`spawntimesecs`,`Comment`) "
                f"SELECT {9500000 + entry},{entry},`map`,`zoneId`,`areaId`,1,1,"
                "`position_x`+1.5,`position_y`,`position_z`,`orientation`,300,'ADVAI class instructor' "
                f"FROM `creature` WHERE `id`={donor} ORDER BY `guid` LIMIT 1;"]
    for trainer, base_class, greeting in ((95020, 5, "Learn to bind the void and control your shadow."),
                                         (95022, 9, "Raise your companions. Their strength must be earned.")):
        sql += [f"DELETE FROM `trainer_spell` WHERE `TrainerId`={trainer};",
                f"DELETE FROM `trainer` WHERE `Id`={trainer};",
                "INSERT INTO `trainer` (`Id`,`Type`,`Requirement`,`Greeting`,`VerifiedBuild`) "
                f"VALUES ({trainer},0,{base_class},'{greeting}',12340);"]
        for spec in specs:
            if "summon" not in spec and spec["id"] in (363419, 363422):
                continue
            if (spec["id"] in (363420, 363421)) != (base_class == 5):
                continue
            level = spec["level"]
            cost = max(10, level * level * 10)
            sql.append("INSERT INTO `trainer_spell` "
                       "(`TrainerId`,`SpellId`,`MoneyCost`,`ReqLevel`,`VerifiedBuild`) "
                       f"VALUES ({trainer},{spec['id']},{cost},{level},12340);")
    for spec in specs:
        if "script" in spec:
            sql += [f"DELETE FROM `spell_script_names` WHERE `spell_id`={spec['id']};",
                    "INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) "
                    f"VALUES ({spec['id']},'{spec['script']}');"]
    sql += ["DROP TEMPORARY TABLE `advai_class_template`;", "COMMIT;", ""]
    return "\n".join(sql)


if __name__ == "__main__":
    target = ROOT / "data/sql/world/optional/2026_10_17_01_advai_classes.sql"
    target.write_text(compile_sql(), encoding="utf-8", newline="\n")
    print(f"Authored class world migration: {target}")
