-- Give archetype weapons displays whose models match their weapon slots.
UPDATE `item_template` SET `displayid` = 33086 WHERE `entry` = 95100 AND `displayid` = 33061;
UPDATE `item_template` SET `displayid` = 39287 WHERE `entry` = 95101 AND `displayid` = 45396;
UPDATE `item_template` SET `displayid` = 39425 WHERE `entry` = 95102 AND `displayid` = 50800;
UPDATE `item_template` SET `displayid` = 42655 WHERE `entry` = 95103 AND `displayid` = 50800;
UPDATE `item_template` SET `displayid` = 46297 WHERE `entry` = 95104 AND `displayid` = 50800;
UPDATE `item_template` SET `displayid` = 28586 WHERE `entry` = 95110 AND `displayid` = 33070;
UPDATE `item_template` SET `displayid` = 16538 WHERE `entry` = 95111 AND `displayid` = 33120;
UPDATE `item_template` SET `displayid` = 20218 WHERE `entry` = 95112 AND `displayid` = 33857;
UPDATE `item_template` SET `displayid` = 42655 WHERE `entry` = 96015 AND `displayid` = 33061;
UPDATE `item_dbc` SET `DisplayInfoID` = 33086 WHERE `ID` = 95100 AND `DisplayInfoID` = 33061;
UPDATE `item_dbc` SET `DisplayInfoID` = 39287 WHERE `ID` = 95101 AND `DisplayInfoID` = 45396;
UPDATE `item_dbc` SET `DisplayInfoID` = 39425 WHERE `ID` = 95102 AND `DisplayInfoID` = 50800;
UPDATE `item_dbc` SET `DisplayInfoID` = 42655 WHERE `ID` = 95103 AND `DisplayInfoID` = 50800;
UPDATE `item_dbc` SET `DisplayInfoID` = 46297 WHERE `ID` = 95104 AND `DisplayInfoID` = 50800;
UPDATE `item_dbc` SET `DisplayInfoID` = 28586 WHERE `ID` = 95110 AND `DisplayInfoID` = 33070;
UPDATE `item_dbc` SET `DisplayInfoID` = 16538 WHERE `ID` = 95111 AND `DisplayInfoID` = 33120;
UPDATE `item_dbc` SET `DisplayInfoID` = 20218 WHERE `ID` = 95112 AND `DisplayInfoID` = 33857;
UPDATE `item_dbc` SET `DisplayInfoID` = 42655 WHERE `ID` = 96015 AND `DisplayInfoID` = 33061;

-- Preserve the death knight polearm entry while making it available to necromancers.
DELETE FROM `skillraceclassinfo_dbc` WHERE `ID` = 915;
INSERT INTO `skillraceclassinfo_dbc` (`ID`, `SkillID`, `RaceMask`, `ClassMask`, `Flags`, `MinLevel`, `SkillTierID`, `SkillCostIndex`) VALUES
(915, 229, 262143, 4128, 128, 0, 0, 0);

-- The core teaches missing default skills on login as well as on character creation.
DELETE FROM `playercreateinfo_skills` WHERE `raceMask` = 0 AND `classMask` = 2048 AND `skill` = 43;
DELETE FROM `playercreateinfo_skills` WHERE `raceMask` = 0 AND `classMask` = 4096 AND `skill` = 229;
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`) VALUES
(0, 2048, 43, 0, 'Cultist swords'),
(0, 4096, 229, 0, 'Necromancer polearms');
