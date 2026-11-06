-- Custom item definitions for QoL Shop
DELETE FROM `item_template` WHERE `entry` IN (98500, 98501, 98502, 98503, 98504);
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `description`) VALUES
(98500, 12, 0, 'Tome of Advancement', 4501, 4, 64, 1, 0, 0, 0, -1, -1, 80, 1, 1, 1, 1, 54710, 0, 'Opens your Class Grimoire anywhere in the world to upgrade and learn talents.'),
(98501, 12, 0, 'Tome of Clean Slate', 4502, 3, 64, 1, 0, 0, 0, -1, -1, 80, 1, 1, 1, 1, 54710, 0, 'Resets your class abilities and talents anywhere in the world.'),
(98502, 0, 0, 'Draught of Accelerated Learning', 15715, 4, 64, 1, 0, 0, 0, -1, -1, 80, 1, 20, 20, 1, 54710, 0, 'Grants +100% increased experience gain from monster kills and quests for 1 hour.');

UPDATE `item_template` SET `ScriptName` = 'item_qol_shop' WHERE `entry` IN (98500, 98501, 98502);

-- Clone the authentic 3.3.5 devices so their stock summon spells and cooldowns
-- work, then remove Engineering requirements from the reward versions.
CREATE TEMPORARY TABLE `qol_shop_portable_item` LIKE `item_template`;
INSERT INTO `qol_shop_portable_item` SELECT * FROM `item_template` WHERE `entry` = 40768;
UPDATE `qol_shop_portable_item`
SET `entry` = 98503, `name` = 'Portable MOLL-E', `RequiredSkill` = 0, `RequiredSkillRank` = 0,
    `RequiredLevel` = 1, `maxcount` = 1, `bonding` = 1,
    `description` = 'Summons MOLL-E, a portable mailbox. Engineering is not required.', `ScriptName` = '';
INSERT INTO `item_template` SELECT * FROM `qol_shop_portable_item`;

TRUNCATE TABLE `qol_shop_portable_item`;
INSERT INTO `qol_shop_portable_item` SELECT * FROM `item_template` WHERE `entry` = 34113;
UPDATE `qol_shop_portable_item`
SET `entry` = 98504, `name` = 'Portable Field Repair Bot', `RequiredSkill` = 0, `RequiredSkillRank` = 0,
    `RequiredLevel` = 1, `maxcount` = 1, `bonding` = 1,
    `description` = 'Summons a repair bot. Engineering is not required.', `ScriptName` = '';
INSERT INTO `item_template` SELECT * FROM `qol_shop_portable_item`;
DROP TEMPORARY TABLE `qol_shop_portable_item`;
