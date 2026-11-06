-- Restore Night Elf (race 4) Cultist/Necromancer starts. Race 3 is Dwarf;
-- race 4 is Night Elf and remains eligible in the client CharBaseInfo table.
START TRANSACTION;

REPLACE INTO `playercreateinfo`
    (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`)
SELECT 4, 12, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`
FROM `playercreateinfo` AS `donor`
WHERE `donor`.`race` = 4 AND `donor`.`class` = 5
  AND NOT EXISTS (SELECT 1 FROM `playercreateinfo` WHERE `race` = 4 AND `class` = 12);

REPLACE INTO `playercreateinfo`
    (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`)
SELECT 4, 13, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`
FROM `playercreateinfo` AS `donor`
WHERE `donor`.`race` = 4 AND `donor`.`class` = 5
  AND NOT EXISTS (SELECT 1 FROM `playercreateinfo` WHERE `race` = 4 AND `class` = 13);

DELETE FROM `playercreateinfo_action` WHERE `race` = 4 AND `class` IN (12, 13);
INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`) VALUES
    (4, 12, 0, 500720, 0),
    (4, 12, 1, 800413, 0),
    (4, 13, 0, 500970, 0),
    (4, 13, 1, 804558, 0);

COMMIT;
