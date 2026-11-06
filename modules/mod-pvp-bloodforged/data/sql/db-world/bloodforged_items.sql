-- Bloodforged PvP Reward Items & Gladiator's Spoils of War
DELETE FROM `item_template`
WHERE (`name` LIKE 'Bloodforged %' AND `entry` BETWEEN 96010 AND 96380)
   OR `entry` = 98510;

-- 98510: Gladiator's Spoils of War (Bag of Goodies)
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `description`, `ScriptName`) VALUES
(98510, 15, 0, 'Gladiator\'s Spoils of War', 1184, 3, 64, 0, 0, 0, -1, -1, 80, 10, 20, 20, 1, 54710, 0, 'Rewarded from honorable PvP combat. Open to receive scaled currency or a 20% chance at a Bloodforged weapon.', 'item_gladiator_spoils');

UPDATE `item_template` SET `ScriptName` = 'item_gladiator_spoils' WHERE `entry` = 98510;

-- Rare (Blue) Bloodforged Weapons (Bracket 10-19 to 80)
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`, `delay`, `dmg_min1`, `dmg_max1`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `description`) VALUES
(96210, 2, 7, 'Bloodforged Cleaver', 15286, 3, 0, 1000, 250, 13, 24, 19, 2400, 22, 42, 4, 5, 7, 6, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96220, 2, 7, 'Bloodforged Broadsword', 15286, 3, 0, 2500, 600, 13, 34, 29, 2400, 42, 78, 4, 8, 7, 10, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96230, 2, 7, 'Bloodforged Warmace', 15286, 3, 0, 6000, 1500, 13, 44, 39, 2600, 68, 126, 4, 12, 7, 15, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96240, 2, 7, 'Bloodforged Battleaxe', 15286, 3, 0, 12000, 3000, 13, 54, 49, 2600, 98, 184, 4, 16, 7, 20, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96250, 2, 7, 'Bloodforged Greatsword', 15286, 3, 0, 25000, 6000, 13, 64, 59, 2600, 142, 265, 4, 22, 7, 26, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96260, 2, 7, 'Bloodforged Warblade', 15286, 3, 0, 50000, 12500, 13, 74, 69, 2600, 195, 364, 4, 30, 7, 35, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96270, 2, 7, 'Bloodforged Decapitator', 15286, 3, 0, 80000, 20000, 13, 84, 79, 2600, 280, 520, 4, 40, 7, 45, 'Bloodforged in the crucible of battle (+15% PvP power).'),
(96280, 2, 7, 'Bloodforged Vanguard Blade', 15286, 3, 0, 120000, 30000, 13, 200, 80, 2600, 360, 672, 4, 55, 7, 60, 'Bloodforged in the crucible of battle (+15% PvP power).');

-- Epic (Purple) Bloodforged Weapons (Bracket 10-19 to 80)
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`, `delay`, `dmg_min1`, `dmg_max1`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `description`) VALUES
(96310, 2, 7, 'Bloodforged Berserker Edge', 45396, 4, 0, 2500, 600, 13, 28, 19, 2400, 28, 52, 4, 8, 7, 9, 'Empowered Epic Bloodforged weapon.'),
(96320, 2, 7, 'Bloodforged Slayer\'s Fang', 45396, 4, 0, 6000, 1500, 13, 38, 29, 2400, 52, 98, 4, 12, 7, 14, 'Empowered Epic Bloodforged weapon.'),
(96330, 2, 7, 'Bloodforged Glacial Doom', 45396, 4, 0, 15000, 3500, 13, 48, 39, 2600, 84, 158, 4, 18, 7, 21, 'Empowered Epic Bloodforged weapon.'),
(96340, 2, 7, 'Bloodforged Soulflayer', 45396, 4, 0, 30000, 7500, 13, 58, 49, 2600, 125, 235, 4, 24, 7, 28, 'Empowered Epic Bloodforged weapon.'),
(96350, 2, 7, 'Bloodforged Warlord Greatblade', 45396, 4, 0, 60000, 15000, 13, 68, 59, 2600, 180, 335, 4, 32, 7, 36, 'Empowered Epic Bloodforged weapon.'),
(96360, 2, 7, 'Bloodforged Brutal Carver', 45396, 4, 0, 100000, 25000, 13, 78, 69, 2600, 250, 465, 4, 42, 7, 48, 'Empowered Epic Bloodforged weapon.'),
(96370, 2, 7, 'Bloodforged Gladiator\'s Cleaver', 45396, 4, 0, 150000, 37500, 13, 88, 79, 2600, 350, 650, 4, 55, 7, 62, 'Empowered Epic Bloodforged weapon.'),
(96380, 2, 7, 'Bloodforged Wrathful Sovereign', 45396, 4, 0, 250000, 60000, 13, 245, 80, 2600, 480, 892, 4, 75, 7, 85, 'Empowered Epic Bloodforged weapon.');
