-- Archetype Weapons for Necromancer and Cultist (Dungeon & Raid Drops)
-- Necromancer Scythes: 95100 (Lvl 20), 95101 (Lvl 40), 95102 (Lvl 60), 95103 (Lvl 70), 95104 (Lvl 80)
-- Cultist Godblades:   95110 (Lvl 20), 95111 (Lvl 40), 95112 (Lvl 60), 95113 (Lvl 70), 95114 (Lvl 80)

DELETE FROM `item_template` WHERE `entry` BETWEEN 95100 AND 95114;

-- Necromancer Scythes (Polearms)
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `delay`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `spellid_1`, `spelltrigger_1`, `bonding`, `description`) VALUES
(95100, 2, 6, 'Scythe of the Tomb Initiate', 33061, 3, 0, 1000, 250, 17, -1, -1, 25, 20, 3200, 35, 54, 0, 5, 8, 7, 10, 9315, 1, 2, 'An ornate bone scythe recovered from the crypts of Shadowfang.'),
(95101, 2, 6, 'Scythe of the Bone Harvester', 45396, 3, 0, 5000, 1250, 17, -1, -1, 45, 40, 3400, 92, 139, 0, 5, 18, 7, 22, 9342, 1, 2, 'Harvested from the crypts beneath the Scarlet Monastery.'),
(95102, 2, 6, 'Soul-Reaver\'s Greatscythe', 50800, 4, 0, 20000, 5000, 17, -1, -1, 65, 60, 3500, 185, 278, 0, 5, 30, 7, 35, 18047, 1, 2, 'Crafted by Darkmaster Gandling in the depths of Scholomance.'),
(95103, 2, 6, 'Dreadscythe of the Cryptlord', 50800, 4, 0, 60000, 15000, 17, -1, -1, 115, 70, 3600, 340, 510, 0, 5, 55, 7, 60, 0, 0, 2, 'Pulsing with the necrotic energies of the Shadow Labyrinth.'),
(95104, 2, 6, 'Scythe of the Unholy Arch-Necromancer', 50800, 4, 0, 200000, 50000, 17, -1, -1, 264, 80, 3600, 720, 1080, 0, 5, 125, 7, 140, 0, 0, 2, 'Forged from saronite and chilled by the Frozen Throne of Icecrown.');

-- Cultist Godblades (One-Hand Swords)
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `delay`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `spellid_1`, `spelltrigger_1`, `bonding`, `description`) VALUES
(95110, 2, 7, 'Whispering Fang of Aku\'mai', 33070, 3, 0, 1000, 250, 13, -1, -1, 25, 20, 2600, 24, 46, 0, 3, 6, 7, 8, 9315, 1, 2, 'Carved from the scale of the Old God beast of Blackfathom Deeps.'),
(95111, 2, 7, 'Blade of the Black Empire', 33120, 3, 0, 5000, 1250, 13, -1, -1, 45, 40, 2600, 68, 127, 0, 3, 14, 7, 16, 9342, 1, 2, 'Etched with forbidden runic scripts from Zul\'Farrak.'),
(95112, 2, 7, 'Claw of the Void Harbinger', 33857, 4, 0, 20000, 5000, 13, -1, -1, 77, 60, 2600, 124, 231, 0, 3, 25, 7, 28, 18047, 1, 2, 'Bestowed by the Prophet Skeram in the Temple of Ahn\'Qiraj.'),
(95113, 2, 7, 'Void-Etched Godblade', 42646, 4, 0, 60000, 15000, 13, -1, -1, 115, 70, 2600, 215, 400, 0, 3, 42, 7, 48, 0, 0, 2, 'Resonating with the shadowy whispers of Sethekk Halls.'),
(95114, 2, 7, 'Edge of the Thousand Maws', 42646, 4, 0, 200000, 50000, 13, -1, -1, 245, 80, 2600, 480, 892, 0, 3, 85, 7, 92, 0, 0, 2, 'Pulsing with the maddening gaze of Yogg-Saron in Ulduar.');

-- Creature Loot Drop Integrations
-- 3887 (Baron Silverlaine - SFK) -> 95100 (20% drop)
DELETE FROM `creature_loot_template` WHERE `item` BETWEEN 95100 AND 95114;
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(3887,  95100, 20.0, 0, 1, 0, 1, 1, 'Baron Silverlaine - Scythe of the Tomb Initiate (Lvl 20)'),
(4543,  95101, 20.0, 0, 1, 0, 1, 1, 'Bloodmage Thalnos - Scythe of the Bone Harvester (Lvl 40)'),
(1853,  95102, 18.0, 0, 1, 0, 1, 1, 'Darkmaster Gandling - Soul-Reaver\'s Greatscythe (Lvl 60)'),
(18708, 95103, 15.0, 0, 1, 0, 1, 1, 'Murmur - Dreadscythe of the Cryptlord (Lvl 70)'),
(36855, 95104, 15.0, 0, 1, 0, 1, 1, 'Lady Deathwhisper (ICC) - Scythe of the Unholy Arch-Necromancer (Lvl 80)'),
(36597, 95104, 20.0, 0, 1, 0, 1, 1, 'The Lich King (ICC) - Scythe of the Unholy Arch-Necromancer (Lvl 80)'),

(4829,  95110, 20.0, 0, 1, 0, 1, 1, 'Aku\'mai (BFD) - Whispering Fang of Aku\'mai (Lvl 20)'),
(8127,  95111, 20.0, 0, 1, 0, 1, 1, 'Antu\'sul (ZF) - Blade of the Black Empire (Lvl 40)'),
(15263, 95112, 18.0, 0, 1, 0, 1, 1, 'The Prophet Skeram (AQ40) - Claw of the Void Harbinger (Lvl 60)'),
(18473, 95113, 15.0, 0, 1, 0, 1, 1, 'Talon King Ikiss (Sethekk) - Void-Etched Godblade (Lvl 70)'),
(33288, 95114, 18.0, 0, 1, 0, 1, 1, 'Yogg-Saron (Ulduar) - Edge of the Thousand Maws (Lvl 80)');
