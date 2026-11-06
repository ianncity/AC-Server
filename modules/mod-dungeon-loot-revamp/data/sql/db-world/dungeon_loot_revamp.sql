-- Vanilla+ Dungeon Loot Revamp
-- Adds existing gear appropriate to each dungeon's level range, with accurate
-- creature and item names. No custom item templates are introduced.
--
-- Remove exact rows from the previous version and current additions so this
-- script can be applied repeatedly without deleting unrelated core loot.
DELETE FROM `creature_loot_template` WHERE
(`Entry` = 639 AND `Item` = 22401) OR
(`Entry` = 639 AND `Item` = 22402) OR
(`Entry` = 639 AND `Item` = 22403) OR
(`Entry` = 644 AND `Item` = 1300) OR
(`Entry` = 644 AND `Item` = 5183) OR
(`Entry` = 642 AND `Item` = 11920) OR
(`Entry` = 3669 AND `Item` = 6449) OR
(`Entry` = 3670 AND `Item` = 6505) OR
(`Entry` = 3673 AND `Item` = 10457) OR
(`Entry` = 3671 AND `Item` = 10458) OR
(`Entry` = 3887 AND `Item` = 13064) OR
(`Entry` = 4278 AND `Item` = 11927) OR
(`Entry` = 4275 AND `Item` = 23005) OR
(`Entry` = 4887 AND `Item` = 943) OR
(`Entry` = 4832 AND `Item` = 13000) OR
(`Entry` = 4829 AND `Item` = 22402) OR
(`Entry` = 6235 AND `Item` = 13065) OR
(`Entry` = 6229 AND `Item` = 9449) OR
(`Entry` = 7800 AND `Item` = 9453) OR
(`Entry` = 6487 AND `Item` = 23007) OR
(`Entry` = 3975 AND `Item` = 7724) OR
(`Entry` = 3977 AND `Item` = 23006) OR
(`Entry` = 8127 AND `Item` = 9447) OR
(`Entry` = 7273 AND `Item` = 9448) OR
(`Entry` = 7267 AND `Item` = 9449) OR
(`Entry` = 12201 AND `Item` = 17743) OR
(`Entry` = 12225 AND `Item` = 17744) OR
(`Entry` = 12203 AND `Item` = 17745) OR
(`Entry` = 5709 AND `Item` = 10796) OR
(`Entry` = 8443 AND `Item` = 10797) OR
(`Entry` = 5710 AND `Item` = 10838) OR
(`Entry` = 10440 AND `Item` = 13361) OR
(`Entry` = 10440 AND `Item` = 23005) OR
(`Entry` = 1853 AND `Item` = 14536) OR
(`Entry` = 1853 AND `Item` = 23006) OR
(`Entry` = 10508 AND `Item` = 18721) OR
(`Entry` = 639 AND `Item` = 920) OR
(`Entry` = 639 AND `Item` = 6580) OR
(`Entry` = 639 AND `Item` = 890) OR
(`Entry` = 646 AND `Item` = 5209) OR
(`Entry` = 646 AND `Item` = 1406) OR
(`Entry` = 644 AND `Item` = 2194) OR
(`Entry` = 3669 AND `Item` = 880) OR
(`Entry` = 3670 AND `Item` = 2035) OR
(`Entry` = 3673 AND `Item` = 2271) OR
(`Entry` = 3654 AND `Item` = 3055) OR
(`Entry` = 3887 AND `Item` = 12984) OR
(`Entry` = 4278 AND `Item` = 15530) OR
(`Entry` = 4275 AND `Item` = 5541) OR
(`Entry` = 4887 AND `Item` = 1155) OR
(`Entry` = 4832 AND `Item` = 2912) OR
(`Entry` = 4829 AND `Item` = 9836) OR
(`Entry` = 6235 AND `Item` = 5213) OR
(`Entry` = 6229 AND `Item` = 3850) OR
(`Entry` = 7800 AND `Item` = 3844) OR
(`Entry` = 6487 AND `Item` = 8175) OR
(`Entry` = 3977 AND `Item` = 15591) OR
(`Entry` = 8127 AND `Item` = 9423) OR
(`Entry` = 7273 AND `Item` = 2164) OR
(`Entry` = 7267 AND `Item` = 15216) OR
(`Entry` = 12201 AND `Item` = 12014) OR
(`Entry` = 12225 AND `Item` = 15274) OR
(`Entry` = 12203 AND `Item` = 17733) OR
(`Entry` = 5709 AND `Item` = 15646) OR
(`Entry` = 8443 AND `Item` = 10195) OR
(`Entry` = 5710 AND `Item` = 14806) OR
(`Entry` = 10440 AND `Item` = 23203) OR
(`Entry` = 1853 AND `Item` = 14538) OR
(`Entry` = 1853 AND `Item` = 22345) OR
(`Entry` = 10508 AND `Item` = 12709);

-- Restore three original core drops removed by the previous broad cleanup.
DELETE FROM `creature_loot_template` WHERE
(`Entry` = 4887 AND `Item` = 6908) OR
(`Entry` = 6229 AND `Item` = 9449) OR
(`Entry` = 3976 AND `Item` = 7724);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(4887, 6908, 0, 60, 0, 1, 1, 1, 1, 'Ghamoo-ra - Ghamoo-ra''s Bind'),
(6229, 9449, 0, 40, 0, 1, 1, 1, 1, 'Crowd Pummeler 9-60 - Manual Crowd Pummeler'),
(3976, 7724, 0, 0, 0, 1, 1, 1, 1, 'Scarlet Commander Mograine - Gauntlets of Divinity');

-- DEADMINES (Level 18-23)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(639, 920, 20.0, 0, 1, 0, 1, 1, 'Edwin VanCleef - Wicked Spiked Mace'),
(639, 6580, 20.0, 0, 1, 0, 1, 1, 'Edwin VanCleef - Defender Tunic'),
(639, 890, 20.0, 0, 1, 0, 1, 1, 'Edwin VanCleef - Twisted Chanter''s Staff'),
(646, 5209, 22.0, 0, 1, 0, 1, 1, 'Mr. Smite - Gloom Wand'),
(646, 1406, 25.0, 0, 1, 0, 1, 1, 'Mr. Smite - Pearl-encrusted Spear'),
(644, 2194, 25.0, 0, 1, 0, 1, 1, 'Rhahk''Zor - Diamond Hammer');

-- WAILING CAVERNS (Level 17-24)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(3669, 880, 25.0, 0, 1, 0, 1, 1, 'Lord Cobrahn - Staff of Horrors'),
(3670, 2035, 22.0, 0, 1, 0, 1, 1, 'Lord Pythas - Sword of the Night Sky'),
(3673, 2271, 24.0, 0, 1, 0, 1, 1, 'Lord Serpentis - Staff of the Blessed Seer'),
(3654, 3055, 25.0, 0, 1, 0, 1, 1, 'Mutanus the Devourer - Forest Leather Chestpiece');

-- SHADOWFANG KEEP (Level 22-30)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(3887, 12984, 25.0, 0, 1, 0, 1, 1, 'Baron Silverlaine - Skycaller'),
(4278, 15530, 28.0, 0, 1, 0, 1, 1, 'Commander Springvale - Sentry''s Shield'),
(4275, 5541, 22.0, 0, 1, 0, 1, 1, 'Archmage Arugal - Iridescent Hammer');

-- BLACKFATHOM DEEPS (Level 24-32)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(4887, 1155, 22.0, 0, 1, 0, 1, 1, 'Ghamoo-ra - Rod of the Sleepwalker'),
(4832, 2912, 25.0, 0, 1, 0, 1, 1, 'Twilight Lord Kelris - Claw of the Shadowmancer'),
(4829, 9836, 25.0, 0, 1, 0, 1, 1, 'Aku''mai - Banded Armor');

-- GNOMEREGAN (Level 29-38)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(6235, 5213, 24.0, 0, 1, 0, 1, 1, 'Electrocutioner 6000 - Scorching Wand'),
(6229, 3850, 25.0, 0, 1, 0, 1, 1, 'Crowd Pummeler 9-60 - Jade Serpentblade'),
(7800, 3844, 25.0, 0, 1, 0, 1, 1, 'Mekgineer Thermaplugg - Green Iron Hauberk');

-- SCARLET MONASTERY (Level 28-45)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(6487, 8175, 25.0, 0, 1, 0, 1, 1, 'Arcanist Doan - Nightscape Tunic'),
(3975, 7724, 28.0, 0, 1, 0, 1, 1, 'Herod - Gauntlets of Divinity'),
(3977, 15591, 25.0, 0, 1, 0, 1, 1, 'High Inquisitor Whitemane - Steadfast Breastplate');

-- ZUL'FARRAK (Level 44-54)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(8127, 9423, 22.0, 0, 1, 0, 1, 1, 'Antu''sul - The Jackhammer'),
(7273, 2164, 24.0, 0, 1, 0, 1, 1, 'Gahz''rilla - Gut Ripper'),
(7267, 15216, 25.0, 0, 1, 0, 1, 1, 'Chief Ukorz Sandscalp - Rune Sword');

-- MARAUDON (Level 46-55)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(12201, 12014, 24.0, 0, 1, 0, 1, 1, 'Princess Theradras - Arctic Ring'),
(12225, 15274, 25.0, 0, 1, 0, 1, 1, 'Celebras the Cursed - Diviner Long Staff'),
(12203, 17733, 25.0, 0, 1, 0, 1, 1, 'Landslide - Fist of Stone');

-- SUNKEN TEMPLE (Level 50-60)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(5709, 15646, 25.0, 0, 1, 0, 1, 1, 'Shade of Eranikus - Ironhide Legguards'),
(8443, 10195, 25.0, 0, 1, 0, 1, 1, 'Avatar of Hakkar - Crusader''s Shield'),
(5710, 14806, 25.0, 0, 1, 0, 1, 1, 'Jammal''an the Prophet - Bloodlust Epaulets');

-- STRATHOLME & SCHOLOMANCE (Level 58-60 Pre-Raid)
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(10440, 13361, 22.0, 0, 1, 0, 1, 1, 'Baron Rivendare - Skullforge Reaver'),
(10440, 23203, 25.0, 0, 1, 0, 1, 1, 'Baron Rivendare - Libram of Fervor'),
(1853, 14538, 25.0, 0, 1, 0, 1, 1, 'Darkmaster Gandling - Deadwalker Mantle'),
(1853, 22345, 25.0, 0, 1, 0, 1, 1, 'Darkmaster Gandling - Totem of Rebirth'),
(10508, 12709, 24.0, 0, 1, 0, 1, 1, 'Ras Frostwhisper - Finkle''s Skinner');

