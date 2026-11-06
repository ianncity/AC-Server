-- Run after disabling mod-zone-wardens and before removing its module directory.
DELETE FROM `creature` WHERE `guid` BETWEEN 8000000 AND 8000146 AND `id` BETWEEN 1200001 AND 1200147;
DELETE FROM `creature_addon` WHERE `guid` BETWEEN 8000000 AND 8000146;
DELETE FROM `waypoint_data` WHERE `id` BETWEEN 8500000 AND 8500097;
DELETE FROM `smart_scripts` WHERE `entryorguid` BETWEEN 1200001 AND 1200147 AND `source_type` = 0;
DELETE FROM `creature_text` WHERE `CreatureID` BETWEEN 1200001 AND 1200147;
DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 1200001 AND 1200147;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 1200001 AND 1200147;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 1200001 AND 1200147;

-- Restore original base values only when the module-generated Wrath health value is still present.
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 828, `rangedattackpower` = 0, `damage_exp2` = 181.124 WHERE `level` = 84 AND `class` = 1 AND `basehp2` = 15130;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 840, `rangedattackpower` = 0, `damage_exp2` = 181.61 WHERE `level` = 84 AND `class` = 2 AND `basehp2` = 15130;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 828, `rangedattackpower` = 0, `damage_exp2` = 181.124 WHERE `level` = 84 AND `class` = 4 AND `basehp2` = 15130;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 425, `rangedattackpower` = 0, `damage_exp2` = 169.819 WHERE `level` = 84 AND `class` = 8 AND `basehp2` = 12104;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 851, `rangedattackpower` = 0, `damage_exp2` = 185.174 WHERE `level` = 85 AND `class` = 1 AND `basehp2` = 16416;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 851, `rangedattackpower` = 0, `damage_exp2` = 185.174 WHERE `level` = 85 AND `class` = 4 AND `basehp2` = 16416;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 467, `rangedattackpower` = 0, `damage_exp2` = 174.135 WHERE `level` = 85 AND `class` = 8 AND `basehp2` = 13133;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 86 AND `class` = 1 AND `basehp2` = 17812;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 86 AND `class` = 2 AND `basehp2` = 17812;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 86 AND `class` = 4 AND `basehp2` = 17812;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 86 AND `class` = 8 AND `basehp2` = 14249;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 87 AND `class` = 1 AND `basehp2` = 19326;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 87 AND `class` = 2 AND `basehp2` = 19326;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 87 AND `class` = 4 AND `basehp2` = 19326;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 87 AND `class` = 8 AND `basehp2` = 15461;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 88 AND `class` = 1 AND `basehp2` = 20968;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 88 AND `class` = 4 AND `basehp2` = 20968;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 88 AND `class` = 8 AND `basehp2` = 16775;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 89 AND `class` = 1 AND `basehp2` = 22751;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 89 AND `class` = 2 AND `basehp2` = 22751;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 89 AND `class` = 4 AND `basehp2` = 22751;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 89 AND `class` = 8 AND `basehp2` = 18201;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 90 AND `class` = 1 AND `basehp2` = 24685;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 90 AND `class` = 2 AND `basehp2` = 24685;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 90 AND `class` = 4 AND `basehp2` = 24685;
UPDATE `creature_classlevelstats` SET `basehp2` = 1, `basearmor` = 1, `attackpower` = 0, `rangedattackpower` = 0, `damage_exp2` = 0 WHERE `level` = 90 AND `class` = 8 AND `basehp2` = 19748;
