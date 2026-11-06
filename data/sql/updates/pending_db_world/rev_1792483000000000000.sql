-- Install the Prestige flight-path tome consumed by the rank-one reward.
DROP TEMPORARY TABLE IF EXISTS `prestige_flight_path_tome`;
CREATE TEMPORARY TABLE `prestige_flight_path_tome` LIKE `item_template`;
DELETE FROM `prestige_flight_path_tome` WHERE `entry` = 96020;
INSERT INTO `prestige_flight_path_tome`
SELECT * FROM `item_template` WHERE `entry` = 49177;

UPDATE `prestige_flight_path_tome` SET `entry` = 96020,
    `name` = 'Tome of Flight Paths',
    `BuyPrice` = 0,
    `SellPrice` = 0,
    `RequiredLevel` = 1,
    `RequiredSkill` = 0,
    `RequiredSkillRank` = 0,
    `spellid_1` = 0, `spelltrigger_1` = 0,
    `spellid_2` = 0, `spelltrigger_2` = 0,
    `spellid_3` = 0, `spelltrigger_3` = 0,
    `spellid_4` = 0, `spelltrigger_4` = 0,
    `spellid_5` = 0, `spelltrigger_5` = 0,
    `description` = 'Unlocks every flight path across Azeroth and Outland.',
    `ScriptName` = 'item_tome_of_flightpaths';

REPLACE INTO `item_template`
SELECT * FROM `prestige_flight_path_tome`;
DROP TEMPORARY TABLE `prestige_flight_path_tome`;
