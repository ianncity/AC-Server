-- Landslide is creature entry 12203; entry 12202 is a human skull object.
DELETE FROM `creature_loot_template` WHERE `Entry` = 12202 AND `Item` = 17745;
DELETE FROM `creature_loot_template` WHERE `Entry` = 12203 AND `Item` = 17745;
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(12203, 17745, 25.0, 0, 1, 0, 1, 1, 'Landslide - Fist of Stone (Feral/Enh Fist Weapon)');

-- Keep the restored Aku'mai drop independent from its already full guaranteed group.
UPDATE `creature_loot_template` SET `GroupId` = 0 WHERE `Entry` = 4829 AND `Item` = 18878;
