-- Keep server creation data aligned with CharBaseInfo.dbc. Dracthyr have the
-- seven listed native classes plus Cultist and Necromancer; neither can be a
-- Druid, Paladin, or Shaman. Orcs cannot create Cultists.
START TRANSACTION;

DELETE FROM `playercreateinfo_action`
WHERE (`race` = 2 AND `class` = 12)
   OR (`race` IN (12, 13) AND `class` IN (1, 3, 4, 5, 6, 8, 9, 12, 13));

DELETE FROM `playercreateinfo`
WHERE (`race` = 2 AND `class` = 12)
   OR (`race` IN (12, 13) AND `class` IN (1, 3, 4, 5, 6, 8, 9, 12, 13));

CREATE TEMPORARY TABLE `dracthyr_create_mapping` (
    `race` TINYINT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `donorRace` TINYINT UNSIGNED NOT NULL,
    `donorClass` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`race`, `class`)
);

INSERT INTO `dracthyr_create_mapping` (`race`, `class`, `donorRace`, `donorClass`) VALUES
(12, 1, 11, 1), (12, 3, 11, 3), (12, 4, 1, 4), (12, 5, 11, 5),
(12, 6, 11, 6), (12, 8, 11, 8), (12, 9, 7, 9), (12, 12, 11, 5), (12, 13, 11, 5),
(13, 1, 10, 1), (13, 3, 10, 3), (13, 4, 10, 4), (13, 5, 10, 5),
(13, 6, 10, 6), (13, 8, 10, 8), (13, 9, 10, 9), (13, 12, 10, 9), (13, 13, 10, 9);

INSERT INTO `playercreateinfo`
    (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`)
SELECT mapping.`race`, mapping.`class`, donor.`map`, donor.`zone`, donor.`position_x`, donor.`position_y`, donor.`position_z`, donor.`orientation`
FROM `dracthyr_create_mapping` AS mapping
JOIN `playercreateinfo` AS donor
  ON donor.`race` = mapping.`donorRace` AND donor.`class` = mapping.`donorClass`;

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
SELECT mapping.`race`, mapping.`class`, donor.`button`, donor.`action`, donor.`type`
FROM `dracthyr_create_mapping` AS mapping
JOIN `playercreateinfo_action` AS donor
  ON donor.`race` = mapping.`donorRace` AND donor.`class` = mapping.`donorClass`
WHERE mapping.`class` IN (1, 3, 4, 5, 6, 8, 9);

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`) VALUES
(12, 12, 0, 500720, 0), (12, 12, 1, 800413, 0),
(13, 12, 0, 500720, 0), (13, 12, 1, 800413, 0),
(12, 13, 0, 500970, 0), (12, 13, 1, 804558, 0),
(13, 13, 0, 500970, 0), (13, 13, 1, 804558, 0);

DROP TEMPORARY TABLE `dracthyr_create_mapping`;
COMMIT;
