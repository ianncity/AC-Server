-- Standalone Cultist (12) and Necromancer (13) character creation and level 1-80 stats.
-- Their base stat curves use the closest 3.3.5 caster class; custom class abilities are
-- acquired through the class progression modules and never through Skill Cards.
START TRANSACTION;

DELETE FROM `player_class_stats` WHERE `Class` IN (12, 13);
INSERT INTO `player_class_stats`
    (`Class`, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`, `Stamina`, `Intellect`, `Spirit`)
SELECT 12, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`, `Stamina`, `Intellect`, `Spirit`
FROM `player_class_stats` WHERE `Class` = 5 AND `Level` BETWEEN 1 AND 80;
INSERT INTO `player_class_stats`
    (`Class`, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`, `Stamina`, `Intellect`, `Spirit`)
SELECT 13, `Level`, `BaseHP`, `BaseMana`, `Strength`, `Agility`, `Stamina`, `Intellect`, `Spirit`
FROM `player_class_stats` WHERE `Class` = 9 AND `Level` BETWEEN 1 AND 80;

DELETE FROM `playercreateinfo` WHERE `class` IN (12, 13);
INSERT INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`)
SELECT donor.`race`, mapping.`class_id`, donor.`map`, donor.`zone`, donor.`position_x`, donor.`position_y`, donor.`position_z`, donor.`orientation`
FROM `playercreateinfo` AS donor
JOIN (
    SELECT 1 AS `race`, 8 AS `donor_class`, 12 AS `class_id` UNION ALL
    SELECT 4, 5, 12 UNION ALL SELECT 5, 9, 12 UNION ALL SELECT 6, 11, 12 UNION ALL
    SELECT 7, 8, 12 UNION ALL SELECT 8, 9, 12 UNION ALL
    SELECT 1, 8, 13 UNION ALL SELECT 2, 9, 13 UNION ALL SELECT 4, 5, 13 UNION ALL
    SELECT 5, 9, 13 UNION ALL SELECT 6, 11, 13 UNION ALL
    SELECT 7, 8, 13 UNION ALL SELECT 8, 9, 13
) AS mapping ON mapping.`race` = donor.`race` AND mapping.`donor_class` = donor.`class`;

DELETE FROM `playercreateinfo_skills` WHERE `classMask` IN (2048, 4096);
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`)
SELECT DISTINCT `raceMask`, 2048, `skill`, `rank`, 'Cultist weapon proficiency'
FROM `playercreateinfo_skills` WHERE (`classMask` & 16) <> 0 AND `skill` IN (54, 136, 173, 228);
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`)
SELECT DISTINCT `raceMask`, 4096, `skill`, `rank`, 'Necromancer weapon proficiency'
FROM `playercreateinfo_skills` WHERE (`classMask` & 256) <> 0 AND `skill` IN (43, 136, 173, 228);

DELETE FROM `playercreateinfo_spell_custom` WHERE `classmask` IN (2048, 4096);
INSERT INTO `playercreateinfo_spell_custom` (`racemask`, `classmask`, `Spell`, `Note`) VALUES
(0, 2048, 500720, 'Cultist starting ability'),
(0, 2048, 800413, 'Cultist starting ability'),
(0, 4096, 500970, 'Necromancer starting ability'),
(0, 4096, 804558, 'Necromancer starting ability');

DELETE FROM `playercreateinfo_action` WHERE `class` IN (12, 13);
INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
SELECT `race`, 12, 0, 500720, 0 FROM `playercreateinfo` WHERE `class` = 12 UNION ALL
SELECT `race`, 12, 1, 800413, 0 FROM `playercreateinfo` WHERE `class` = 12 UNION ALL
SELECT `race`, 13, 0, 500970, 0 FROM `playercreateinfo` WHERE `class` = 13 UNION ALL
SELECT `race`, 13, 1, 804558, 0 FROM `playercreateinfo` WHERE `class` = 13;

COMMIT;
