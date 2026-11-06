-- Dracthyr player creation data for the matching client/server DBC bundle.
-- Alliance/Horde Dracthyr use race IDs 12 and 13. They gain Cultist and
-- Necromancer, but remain ineligible for Druid, Paladin, and Shaman.
START TRANSACTION;
DROP TEMPORARY TABLE IF EXISTS `advai_dracthyr_donors`;
CREATE TEMPORARY TABLE `advai_dracthyr_donors` (
    `race` TINYINT UNSIGNED NOT NULL,
    `class` TINYINT UNSIGNED NOT NULL,
    `donor` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`race`, `class`)
);
INSERT INTO `advai_dracthyr_donors` VALUES
(12,1,1),(12,3,3),(12,4,1),(12,5,1),(12,6,1),(12,8,1),(12,9,1),
(12,12,1),(12,13,1),
(13,1,2),(13,3,2),(13,4,2),(13,5,8),(13,6,2),(13,8,10),(13,9,2),
(13,12,5),(13,13,5);

DELETE FROM `playercreateinfo` WHERE `race` IN (12,13);
INSERT INTO `playercreateinfo` (`race`,`class`,`map`,`zone`,`position_x`,`position_y`,`position_z`,`orientation`)
SELECT d.`race`,d.`class`,IF(d.`class`=6,p.`map`,IF(d.`race`=12,0,1)),
       IF(d.`class`=6,p.`zone`,IF(d.`race`=12,1519,1637)),
       IF(d.`class`=6,p.`position_x`,IF(d.`race`=12,-8840.4,1530.0)),
       IF(d.`class`=6,p.`position_y`,IF(d.`race`=12,628.7,-4417.0)),
       IF(d.`class`=6,p.`position_z`,IF(d.`race`=12,94.1,15.5)),
       IF(d.`class`=6,p.`orientation`,IF(d.`race`=12,1.0,0.0))
FROM `advai_dracthyr_donors` d JOIN `playercreateinfo` p ON p.`race`=d.`donor` AND p.`class`=d.`class`;

DELETE FROM `playercreateinfo_action` WHERE `race` IN (12,13);
INSERT INTO `playercreateinfo_action` (`race`,`class`,`button`,`action`,`type`)
SELECT d.`race`,d.`class`,a.`button`,a.`action`,a.`type`
FROM `advai_dracthyr_donors` d JOIN `playercreateinfo_action` a ON a.`race`=d.`donor` AND a.`class`=d.`class`;

DELETE FROM `playercreateinfo_item` WHERE `race` IN (12,13);
INSERT INTO `playercreateinfo_item` (`race`,`class`,`itemid`,`amount`)
SELECT d.`race`,d.`class`,i.`itemid`,i.`amount`
FROM `advai_dracthyr_donors` d JOIN `playercreateinfo_item` i ON i.`race`=d.`donor` AND i.`class`=d.`class`;

DELETE FROM `player_race_stats` WHERE `Race` IN (12,13);
INSERT INTO `player_race_stats` (`Race`,`Strength`,`Agility`,`Stamina`,`Intellect`,`Spirit`)
SELECT d.`race`,s.`Strength`,s.`Agility`,s.`Stamina`,s.`Intellect`,s.`Spirit`
FROM (SELECT 12 AS `race`,1 AS `donor` UNION ALL SELECT 13,2) d
JOIN `player_race_stats` s ON s.`Race`=d.`donor`;

DELETE FROM `playercreateinfo_skills` WHERE `raceMask` IN (2048,4096);
INSERT INTO `playercreateinfo_skills` (`raceMask`,`classMask`,`skill`,`rank`)
SELECT 1<<(d.`race`-1),1<<(d.`class`-1),s.`skill`,MAX(s.`rank`)
FROM `advai_dracthyr_donors` d JOIN `playercreateinfo_skills` s
ON (s.`raceMask`=0 OR (s.`raceMask` & (1<<(d.`donor`-1)))<>0)
AND (s.`classMask`=0 OR (s.`classMask` & (1<<(d.`class`-1)))<>0)
WHERE s.`raceMask` NOT IN (2048,4096)
GROUP BY d.`race`,d.`class`,s.`skill`;

DELETE FROM `playercreateinfo_spell_custom` WHERE `racemask` IN (2048,4096);
INSERT INTO `playercreateinfo_spell_custom` (`racemask`,`classmask`,`Spell`,`Note`) VALUES
(2048,0,668,'Dracthyr Common'),(4096,0,669,'Dracthyr Orcish'),
(2048,0,363404,'Dracthyr Glide'),(4096,0,363404,'Dracthyr Glide'),
(2048,0,363405,'Dracthyr Tail Swipe'),(4096,0,363405,'Dracthyr Tail Swipe'),
(2048,0,363406,'Dracthyr Wing Buffet'),(4096,0,363406,'Dracthyr Wing Buffet');
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (95012,95013);
DELETE FROM `creature_template` WHERE `entry` IN (95012,95013);
DROP TEMPORARY TABLE IF EXISTS `advai_visage_template`;
CREATE TEMPORARY TABLE `advai_visage_template` LIKE `creature_template`;
INSERT INTO `advai_visage_template` SELECT * FROM `creature_template` WHERE `entry`=1;
UPDATE `advai_visage_template` SET `entry`=95012,`name`='Dracthyr Male Visage',`subname`='',`npcflag`=0,
    `rank`=0,`lootid`=0,`pickpocketloot`=0,`skinloot`=0,`mingold`=0,`maxgold`=0,`flags_extra`=0,
    `ScriptName`='',`AIName`='';
INSERT INTO `creature_template` SELECT * FROM `advai_visage_template`;
UPDATE `advai_visage_template` SET `entry`=95013,`name`='Dracthyr Female Visage';
INSERT INTO `creature_template` SELECT * FROM `advai_visage_template`;
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`)
VALUES (95012,0,50004,1,1,12340),(95013,0,50005,1,1,12340);
DELETE FROM `creature_model_info` WHERE `DisplayID` IN (50004,50005);
INSERT INTO `creature_model_info`
(`DisplayID`,`BoundingRadius`,`CombatReach`,`Gender`,`DisplayID_Other_Gender`,`VerifiedBuild`)
SELECT 50004,`BoundingRadius`,`CombatReach`,0,50005,12340
FROM `creature_model_info` WHERE `DisplayID`=15476;
INSERT INTO `creature_model_info`
(`DisplayID`,`BoundingRadius`,`CombatReach`,`Gender`,`DisplayID_Other_Gender`,`VerifiedBuild`)
SELECT 50005,`BoundingRadius`,`CombatReach`,1,50004,12340
FROM `creature_model_info` WHERE `DisplayID`=15475;
DROP TEMPORARY TABLE `advai_visage_template`;
DROP TEMPORARY TABLE `advai_dracthyr_donors`;
-- Faction-wide restrictions include the new faction members; single-race restrictions stay specific.
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces` | 2048 WHERE `AllowableRaces`=1101;
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces` | 4096 WHERE `AllowableRaces`=690;
UPDATE `quest_template` SET `AllowableRaces`=`AllowableRaces` | 6144 WHERE `AllowableRaces`=1791;
UPDATE `item_template` SET `AllowableRace`=`AllowableRace` | 2048 WHERE `AllowableRace`=1101;
UPDATE `item_template` SET `AllowableRace`=`AllowableRace` | 4096 WHERE `AllowableRace`=690;
UPDATE `item_template` SET `AllowableRace`=`AllowableRace` | 6144 WHERE `AllowableRace`=1791;
COMMIT;
