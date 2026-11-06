-- Keep WotLK character creation data synchronized with the client Dracthyr
-- matrix. Existing characters are unaffected; this only controls new creation.
START TRANSACTION;

CREATE TEMPORARY TABLE `dracthyr_functional_create_map` (
    `race` TINYINT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `actionRace` TINYINT UNSIGNED NOT NULL,
    `actionClass` TINYINT UNSIGNED NOT NULL,
    `startRace` TINYINT UNSIGNED NOT NULL,
    `startClass` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`race`, `class`)
);

INSERT INTO `dracthyr_functional_create_map`
    (`race`, `class`, `actionRace`, `actionClass`, `startRace`, `startClass`)
VALUES
    (12, 1, 11, 1, 11, 1),
    (12, 3, 11, 3, 11, 1),
    (12, 4, 1, 4, 11, 1),
    (12, 5, 11, 5, 11, 1),
    (12, 6, 11, 6, 11, 6),
    (12, 8, 11, 8, 11, 1),
    (12, 9, 7, 9, 11, 1),
    (12, 12, 0, 0, 11, 1),
    (12, 13, 0, 0, 11, 1),
    (13, 1, 10, 1, 10, 1),
    (13, 3, 10, 3, 10, 1),
    (13, 4, 10, 4, 10, 1),
    (13, 5, 10, 5, 10, 1),
    (13, 6, 10, 6, 10, 6),
    (13, 8, 10, 8, 10, 1),
    (13, 9, 10, 9, 10, 1),
    (13, 12, 0, 0, 10, 1),
    (13, 13, 0, 0, 10, 1);

-- Preserve donor rows before replacing the Dracthyr creation rows. Both
-- source tables are rebuilt below, so selecting from them after DELETE would
-- silently produce no starter locations or native class action bars.
CREATE TEMPORARY TABLE `dracthyr_start_snapshot` AS
SELECT starts.`race`, starts.`class`, starts.`map`, starts.`zone`,
       starts.`position_x`, starts.`position_y`, starts.`position_z`, starts.`orientation`
FROM `playercreateinfo` AS starts
WHERE EXISTS (
    SELECT 1 FROM `dracthyr_functional_create_map` AS mapping
    WHERE mapping.`startRace` = starts.`race` AND mapping.`startClass` = starts.`class`
);

CREATE TEMPORARY TABLE `dracthyr_action_snapshot` AS
SELECT donor.`race`, donor.`class`, donor.`button`, donor.`action`, donor.`type`
FROM `playercreateinfo_action` AS donor
WHERE EXISTS (
    SELECT 1 FROM `dracthyr_functional_create_map` AS mapping
    WHERE mapping.`actionRace` = donor.`race` AND mapping.`actionClass` = donor.`class`
);

DELETE FROM `playercreateinfo_action` WHERE `race` IN (12, 13);
DELETE FROM `playercreateinfo` WHERE `race` IN (12, 13);

INSERT INTO `playercreateinfo`
    (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`)
SELECT mapping.`race`, mapping.`class`, starts.`map`, starts.`zone`,
       starts.`position_x`, starts.`position_y`, starts.`position_z`, starts.`orientation`
FROM `dracthyr_functional_create_map` AS mapping
JOIN `dracthyr_start_snapshot` AS starts
  ON starts.`race` = mapping.`startRace` AND starts.`class` = mapping.`startClass`;

-- Copy each permitted native class's opening action bar while stripping the
-- donor race's active/passive racials. The Dracthyr's own language and three
-- racial abilities are supplied by playercreateinfo_spell_custom.
INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
SELECT mapping.`race`, mapping.`class`, donor.`button`, donor.`action`, donor.`type`
FROM `dracthyr_functional_create_map` AS mapping
JOIN `dracthyr_action_snapshot` AS donor
  ON donor.`race` = mapping.`actionRace` AND donor.`class` = mapping.`actionClass`
WHERE mapping.`class` NOT IN (12, 13)
  AND donor.`action` NOT IN (
      20549, 20572, 20589, 20594, 20600, 2481, 26297,
      25046, 28730, 28880, 33697, 33702, 50613,
      59542, 59543, 59544, 59545, 59546, 59547, 59548, 59549,
      59752, 6562
  );

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`) VALUES
    (12, 12, 0, 500720, 0), (12, 12, 1, 800413, 0),
    (13, 12, 0, 500720, 0), (13, 12, 1, 800413, 0),
    (12, 13, 0, 500970, 0), (12, 13, 1, 804558, 0),
    (13, 13, 0, 500970, 0), (13, 13, 1, 804558, 0);

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
SELECT mapping.`race`, mapping.`class`, buttons.`button`, buttons.`spell`, 0
FROM `dracthyr_functional_create_map` AS mapping
CROSS JOIN (
    SELECT 9 AS `button`, 363404 AS `spell`
    UNION ALL SELECT 10, 363405
    UNION ALL SELECT 11, 363406
) AS buttons;

DROP TEMPORARY TABLE `dracthyr_functional_create_map`;
DROP TEMPORARY TABLE `dracthyr_start_snapshot`;
DROP TEMPORARY TABLE `dracthyr_action_snapshot`;
COMMIT;
