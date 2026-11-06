-- Synchronize character creation data with the supported client race/class table.
-- Cultists are barred from Orcs; Dracthyr gain Cultist and Necromancer.
START TRANSACTION;

DELETE FROM `playercreateinfo_action` WHERE `race`=2 AND `class`=12;
DELETE FROM `playercreateinfo_item` WHERE `race`=2 AND `class`=12;
DELETE FROM `playercreateinfo` WHERE `race`=2 AND `class`=12;
DELETE FROM `playercreateinfo_action` WHERE `race` IN (12,13) AND `class`=11;
DELETE FROM `playercreateinfo_item` WHERE `race` IN (12,13) AND `class`=11;
DELETE FROM `playercreateinfo` WHERE `race` IN (12,13) AND `class`=11;

DROP TEMPORARY TABLE IF EXISTS `advai_dracthyr_custom_classes`;
CREATE TEMPORARY TABLE `advai_dracthyr_custom_classes` (
    `race` TINYINT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `donor` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`race`,`class`)
);
INSERT INTO `advai_dracthyr_custom_classes` (`race`,`class`,`donor`)
SELECT d.`race`,d.`class`,d.`donor`
FROM (
    SELECT 12 AS `race`,12 AS `class`,1 AS `donor` UNION ALL
    SELECT 12,13,1 UNION ALL SELECT 13,12,5 UNION ALL SELECT 13,13,5
) d
WHERE EXISTS (
    SELECT 1 FROM `playercreateinfo` p
    WHERE p.`race`=d.`race` AND p.`class` IN (1,3,4,5,6,8,9)
);

DELETE FROM `playercreateinfo_action` WHERE `race` IN (12,13) AND `class` IN (12,13);
INSERT INTO `playercreateinfo_action` (`race`,`class`,`button`,`action`,`type`)
SELECT d.`race`,d.`class`,a.`button`,a.`action`,a.`type`
FROM `advai_dracthyr_custom_classes` d
JOIN `playercreateinfo_action` a ON a.`race`=d.`donor` AND a.`class`=d.`class`;

DELETE FROM `playercreateinfo_item` WHERE `race` IN (12,13) AND `class` IN (12,13);
INSERT INTO `playercreateinfo_item` (`race`,`class`,`itemid`,`amount`)
SELECT d.`race`,d.`class`,i.`itemid`,i.`amount`
FROM `advai_dracthyr_custom_classes` d
JOIN `playercreateinfo_item` i ON i.`race`=d.`donor` AND i.`class`=d.`class`;

DELETE FROM `playercreateinfo` WHERE `race` IN (12,13) AND `class` IN (12,13);
INSERT INTO `playercreateinfo` (`race`,`class`,`map`,`zone`,`position_x`,`position_y`,`position_z`,`orientation`)
SELECT d.`race`,d.`class`,IF(d.`race`=12,0,1),IF(d.`race`=12,1519,1637),
       IF(d.`race`=12,-8840.4,1530.0),IF(d.`race`=12,628.7,-4417.0),IF(d.`race`=12,94.1,15.5),
       IF(d.`race`=12,1.0,0.0)
FROM `advai_dracthyr_custom_classes` d
JOIN `playercreateinfo` p ON p.`race`=d.`donor` AND p.`class`=d.`class`;

DELETE FROM `playercreateinfo_skills`
WHERE `raceMask` IN (2048,4096) AND `classMask` IN (2048,4096);
INSERT INTO `playercreateinfo_skills` (`raceMask`,`classMask`,`skill`,`rank`)
SELECT 1<<(d.`race`-1),1<<(d.`class`-1),s.`skill`,MAX(s.`rank`)
FROM `advai_dracthyr_custom_classes` d
JOIN `playercreateinfo_skills` s
  ON (s.`raceMask`=0 OR (s.`raceMask` & (1<<(d.`donor`-1)))<>0)
 AND (s.`classMask`=0 OR (s.`classMask` & (1<<(d.`class`-1)))<>0)
WHERE s.`raceMask` NOT IN (2048,4096)
GROUP BY d.`race`,d.`class`,s.`skill`;

DROP TEMPORARY TABLE `advai_dracthyr_custom_classes`;
COMMIT;
