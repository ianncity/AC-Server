-- Restore Dracthyr Cultist/Necromancer creation rows after those classes were
-- removed from the Draenei and Blood Elf donor races.
-- Keep the faction start locations from the October 18 Dracthyr start pass.
START TRANSACTION;

INSERT INTO `playercreateinfo`
    (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`)
VALUES
    (12, 12, 0, 1519, -8840.4, 628.7, 94.1, 1.0),
    (12, 13, 0, 1519, -8840.4, 628.7, 94.1, 1.0),
    (13, 12, 1, 1637, 1530.0, -4417.0, 15.5, 0.0),
    (13, 13, 1, 1637, 1530.0, -4417.0, 15.5, 0.0)
ON DUPLICATE KEY UPDATE
    `map` = VALUES(`map`), `zone` = VALUES(`zone`),
    `position_x` = VALUES(`position_x`), `position_y` = VALUES(`position_y`),
    `position_z` = VALUES(`position_z`), `orientation` = VALUES(`orientation`);

DELETE FROM `playercreateinfo_action`
WHERE `race` IN (12, 13) AND `class` IN (12, 13);

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`)
VALUES
    (12, 12, 0, 500720, 0), (12, 12, 1, 800413, 0),
    (13, 12, 0, 500720, 0), (13, 12, 1, 800413, 0),
    (12, 13, 0, 500970, 0), (12, 13, 1, 804558, 0),
    (13, 13, 0, 500970, 0), (13, 13, 1, 804558, 0);

COMMIT;
