-- =========================================================================
-- Unfinished & Cut Classic Content Restored (AzerothCore World Database)
-- Karazhan Crypts, Custom Battlegrounds, Spawns & Restored Items
-- Level 80 Naxxramas Four Horsemen Drops & Ashbringer Restored
-- =========================================================================

-- 1. Instance Template for Karazhan Crypts (Map 1807)
REPLACE INTO `instance_template` (`map`, `parent`, `script`, `allowMount`) VALUES
(1807, 0, 'instance_karazhan_crypts', 0);

-- 2. AreaTrigger Teleporters for Karazhan Crypts Entrance & Exit
REPLACE INTO `areatrigger_teleport` (`ID`, `Name`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation`) VALUES
(9001, 'Deadwind Pass Morgan Plot to Karazhan Crypts', 1807, -10.0, -15.0, 25.0, 0.0),
(9002, 'Karazhan Crypts to Deadwind Pass Morgan Plot', 0, -11078.0, -1800.0, 52.8, 3.14);

-- 3. Battleground Templates for Restored Classic Battlegrounds
REPLACE INTO `battleground_template` (`ID`, `MinPlayersPerTeam`, `MaxPlayersPerTeam`, `MinLvl`, `MaxLvl`, `AllianceStartLoc`, `AllianceStartO`, `HordeStartLoc`, `HordeStartO`, `StartMaxDist`, `Weight`, `ScriptName`, `Comment`) VALUES
(33, 10, 40, 20, 80, 0, 0, 0, 0, 100, 1, '', 'Hillsbrad Foothills (battleground)'),
(34, 8, 15, 20, 80, 890, 3.91571, 889, 0.813671, 75, 1, '', 'Winter Arathi Basin (battleground)'),
(35, 5, 10, 10, 80, 769, 3.14159, 770, 0.151581, 75, 1, '', 'Winter Warsong Gulch (battleground)'),
(36, 10, 40, 50, 80, 0, 0, 0, 0, 100, 1, '', 'Azshara Crater (battleground)');

-- 4. Creature Templates for Karazhan Crypts NPCs & Bosses
DELETE FROM `creature_template` WHERE `entry` IN (90001, 90002, 90010, 90011, 90012, 90020);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (90001,90002,90010,90011,90012,90020);
DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (90001,90002,90010,90011,90012,90020);
DELETE FROM `creature_template_resistance` WHERE `CreatureID` IN (90001,90002,90010,90011,90012,90020);
DELETE FROM `creature_immunities` WHERE `ID` IN (90001,90002,90010,90011,90012,90020);
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(90001, 0, 0, 0, 0, 0, 'Keeper of the Crypts', 'Guardian of Morgan\'s Plot', '', 90001, 60, 60, 0, 35, 1, 1, 1.14286, 20, 1, 0, 1, 2000, 0, 1, 1, 1, 0, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 'npc_karazhan_crypt_keeper', 12340),
(90002, 0, 0, 0, 0, 0, 'Crypt Watcher', 'Guardian of the Forgotten', '', 90001, 60, 60, 0, 35, 1, 1, 1.14286, 20, 1, 0, 1, 2000, 0, 1, 1, 1, 0, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 'npc_karazhan_crypt_keeper', 12340),
(90010, 0, 0, 0, 0, 0, 'Restless Crypt Fiend', '', '', 0, 59, 60, 1, 14, 0, 1, 1.14286, 20, 1, 0, 1.8, 2000, 0, 1, 1, 1, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 500, 1500, '', 1, 1, 2.5, 1, 1, 1, 0, 0, 1, 0, 0, '', 12340),
(90011, 0, 0, 0, 0, 0, 'Unburied Tormented Soul', '', '', 0, 59, 60, 1, 14, 0, 1, 1.14286, 20, 1, 0, 1.8, 2000, 0, 1, 1, 1, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 500, 1500, '', 1, 1, 2.5, 1, 1, 1, 0, 0, 1, 0, 0, '', 12340),
(90012, 0, 0, 0, 0, 0, 'Catacomb Dark Weaver', '', '', 0, 60, 60, 1, 14, 0, 1, 1.14286, 20, 1, 0, 2.0, 2000, 0, 1, 1, 2, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 800, 2500, '', 1, 1, 2.5, 1.5, 1, 1, 0, 0, 1, 0, 0, '', 12340),
(90020, 0, 0, 0, 0, 0, 'The Tormentor of Upside-Down Sinners', 'Lord of the Catacombs', '', 0, 62, 62, 2, 14, 0, 1, 1.25, 20, 3, 0, 3.5, 1800, 0, 1, 1, 1, 0, 2048, 0, 0, 6, 0, 90020, 0, 0, 0, 0, 20000, 50000, '', 0, 1, 8.0, 5.0, 1.5, 1, 0, 0, 1, 90020, 0, 'boss_tormentor_upside_down_sinners', 12340);
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(90001,0,16369,1,1,12340),
(90002,0,16369,1,1,12340),
(90010,0,11582,1.1,1,12340),
(90011,0,1604,1.0,1,12340),
(90012,0,16369,1.1,1,12340),
(90020,0,21257,1.5,1,12340);
INSERT INTO `creature_template_resistance` (`CreatureID`,`School`,`Resistance`,`VerifiedBuild`) VALUES
(90020,1,50,12340),
(90020,2,50,12340),
(90020,3,50,12340),
(90020,4,50,12340),
(90020,5,50,12340),
(90020,6,50,12340);
INSERT INTO `creature_immunities` (`ID`,`SchoolMask`,`DispelTypeMask`,`MechanicsMask`,`Effects`,`Auras`,`ImmuneAoE`,`ImmuneChain`,`Comment`) VALUES
(90020,0,0,8388608,'','',0,0,'Converted legacy creature template immunity');

-- Enable Ashbringer Purification on High Inquisitor Fairbanks
UPDATE `creature_template` SET `ScriptName` = 'npc_ashbringer_purifier', `npcflag` = `npcflag` | 1 WHERE `entry` IN (4542, 20348);

-- 5. Creature Spawns in Deadwind Pass & Karazhan Crypts
DELETE FROM `creature` WHERE `id` IN (90001, 90002, 90010, 90011, 90012, 90020);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`) VALUES
-- Outside Entrance NPC (Deadwind Pass Morgan's Plot)
(900001, 90001, 0, 1, 1, -11082.0, -1801.0, 52.8, 3.14, 30, 0, 0, 10000, 10000, 0, 1, 0, 0),
-- Inside Entrance NPC (Karazhan Crypts)
(900002, 90002, 1807, 1, 1, -10.0, -18.0, 25.0, 0.0, 30, 0, 0, 10000, 10000, 0, 1, 0, 0),
-- Crypt Fiends and Tormented Souls throughout the halls
(900003, 90010, 1807, 1, 1, -25.0, -20.0, 20.0, 1.2, 120, 5, 0, 15000, 5000, 1, 0, 0, 0),
(900004, 90010, 1807, 1, 1, -40.0, -35.0, 15.0, 2.4, 120, 5, 0, 15000, 5000, 1, 0, 0, 0),
(900005, 90011, 1807, 1, 1, -55.0, -45.0, 10.0, 0.5, 120, 5, 0, 15000, 5000, 1, 0, 0, 0),
(900006, 90011, 1807, 1, 1, -70.0, -60.0, 5.0, 3.0, 120, 5, 0, 15000, 5000, 1, 0, 0, 0),
(900007, 90012, 1807, 1, 1, -85.0, -70.0, 0.0, 1.8, 120, 5, 0, 15000, 8000, 1, 0, 0, 0),
(900008, 90012, 1807, 1, 1, -100.0, -75.0, -10.0, 0.2, 120, 5, 0, 15000, 8000, 1, 0, 0, 0),
-- Boss: The Tormentor of Upside-Down Sinners at the Pool of Tears
(900009, 90020, 1807, 1, 1, -120.0, -80.0, -20.0, 0.0, 7200, 0, 0, 120000, 45000, 0, 0, 0, 0);

-- 6. Loot Table Additions for Level 80 Four Horsemen Chests (10-man & 25-man)

-- Four Horsemen Chest 10-Man (Level 80 Normal - Loot ID 25192)
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 25192 AND `Item` IN (322691, 13262, 95104, 95114);
INSERT INTO `gameobject_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(25192, 322691, 0, 15, 0, 1, 2, 1, 1, 'Four Horsemen 10m - Corrupted Ashbringer (Lvl 80)'),
(25192, 13262,  0, 5,  0, 1, 2, 1, 1, 'Four Horsemen 10m - The Ashbringer (Purified Lvl 80)'),
(25192, 95104,  0, 8,  0, 1, 2, 1, 1, 'Four Horsemen 10m - Scythe of the Arch-Necromancer (Lvl 80)'),
(25192, 95114,  0, 8,  0, 1, 2, 1, 1, 'Four Horsemen 10m - Edge of the Thousand Maws (Lvl 80)');

-- Four Horsemen Chest 25-Man (Level 80 Heroic - Loot ID 25193)
DELETE FROM `gameobject_loot_template` WHERE `Entry` = 25193 AND `Item` IN (322691, 13262, 95104, 95114);
INSERT INTO `gameobject_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(25193, 322691, 0, 20, 0, 1, 0, 1, 1, 'Four Horsemen 25m - Corrupted Ashbringer (Lvl 80)'),
(25193, 13262,  0, 8,  0, 1, 0, 1, 1, 'Four Horsemen 25m - The Ashbringer (Purified Lvl 80)'),
(25193, 95104,  0, 12, 0, 1, 0, 1, 1, 'Four Horsemen 25m - Scythe of the Arch-Necromancer (Lvl 80)'),
(25193, 95114,  0, 12, 0, 1, 0, 1, 1, 'Four Horsemen 25m - Edge of the Thousand Maws (Lvl 80)');

-- Karazhan Crypts End Boss (The Tormentor - Entry 90020)
DELETE FROM `creature_loot_template` WHERE `Entry` = 90020;
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(90020, 90101, 0, 25, 0, 1, 1, 1, 1, 'Shroud of the Upside-Down Sinner'),
(90020, 90102, 0, 25, 0, 1, 1, 1, 1, 'Tormentor''s Hooked Scythe'),
(90020, 90103, 0, 25, 0, 1, 1, 1, 1, 'Cryptbreaker''s Warhammer'),
(90020, 90104, 0, 25, 0, 1, 1, 1, 1, 'Robes of the Forgotten Necromancer'),
(90020, 90105, 0, 35, 0, 1, 2, 1, 1, 'Band of the Catacombs'),
(90020, 90106, 0, 35, 0, 1, 2, 1, 1, 'Talisman of the Crypt Watcher'),
(90020, 22691, 0, 8, 0, 1, 1, 1, 1, 'Tormentor - Corrupted Ashbringer (Lvl 60)'),
(90020, 13262, 0, 3, 0, 1, 1, 1, 1, 'Tormentor - The Ashbringer (Purified)');

-- Baron Geddon (Entry 12056) - Restored Talisman of Binding Shard
DELETE FROM `creature_loot_template` WHERE `Entry` = 12056 AND `Item` = 17782;
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(12056, 17782, 0, 3, 0, 1, 1, 1, 1, 'Baron Geddon - Talisman of Binding Shard');

-- Aku'mai (Entry 4829) in Blackfathom Deeps - Restored Tiara of the Deep
DELETE FROM `creature_loot_template` WHERE `Entry` = 4829 AND `Item` = 18878;
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(4829, 18878, 0, 12, 0, 1, 0, 1, 1, 'Aku\'mai - Tiara of the Deep');

-- Foreman Thistleneedle (Entry 626) in Deadmines - Peon's Mining Pick
DELETE FROM `creature_loot_template` WHERE `Entry` = 626 AND `Item` = 1819;
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(626, 1819, 0, 25, 0, 1, 1, 1, 1, 'Foreman Thistleneedle - Peon\'s Mining Pick');

-- 7. Restored Items in item_template
DELETE FROM `item_template` WHERE `entry` IN (
    13262, 322691, 17782, 18878, 13327, 12303, 1819,
    90101, 90102, 90103, 90104, 90105, 90106,
    90201, 90202, 90203, 90204, 90205, 90206, 90207, 90208, 90209
);

INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`, `stat_type6`, `stat_value6`, `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`, `stat_type10`, `stat_value10`, `ScalingStatDistribution`, `ScalingStatValue`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `dmg_min2`, `dmg_max2`, `dmg_type2`, `armor`, `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`, `delay`, `ammo_type`, `RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`, `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`, `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`, `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`, `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`, `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`, `RandomSuffix`, `block`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `duration`, `ItemLimitCategory`, `HolidayId`, `ScriptName`, `DisenchantID`, `FoodType`, `minMoneyLoot`, `maxMoneyLoot`, `flagsCustom`, `VerifiedBuild`) VALUES
-- 13262: The Ashbringer (Level 80 Legendary 2H Sword, ItemLevel 226)
(13262, 2, 7, -1, 'The Ashbringer', 24550, 5, 0, 0, 1, 500000, 125000, 17, -1, -1, 226, 80, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 145, 7, 152, 32, 84, 36, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 675, 1015, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3600, 0, 0, 23267, 1, 0, 1.8, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Blade of the Scarlet Highlord', 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 145, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 322691: Corrupted Ashbringer (Level 80 Epic 2H Sword, ItemLevel 213)
(322691, 2, 7, -1, 'Corrupted Ashbringer', 28308, 4, 0, 0, 1, 350000, 85000, 17, -1, -1, 213, 80, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 115, 7, 128, 32, 72, 31, 58, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 612, 919, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3600, 0, 0, 22667, 1, 0, 2.0, -1, 0, -1, 28282, 2, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Blade of the Scarlet Highlord', 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 145, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 17782: Talisman of Binding Shard (Legendary Neck)
(17782, 4, 0, -1, 'Talisman of Binding Shard', 30043, 5, 0, 0, 1, 150000, 40000, 2, -1, -1, 80, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 3, 13, 4, 12, 7, 10, 32, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 21987, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'The binding shard of Thunderaan and Garr.', 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 18878: Tiara of the Deep (Epic Cloth Helm)
(18878, 4, 1, -1, 'Tiara of the Deep', 31393, 4, 0, 0, 1, 80000, 20000, 1, -1, -1, 65, 58, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 7, 18, 5, 18, 45, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, '', 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 13327: Fluorescent Green Mechanostrider (Rare Mount)
(13327, 15, 5, -1, 'Fluorescent Green Mechanostrider', 23992, 3, 64, 0, 1, 100000, 25000, 0, -1, -1, 60, 40, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17454, 0, 1, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Restored cut Gnomish mount.', 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 12303: Tiger Riding Kodo (Rare Mount)
(12303, 15, 5, -1, 'Tiger Riding Kodo', 22444, 3, 64, 0, 1, 100000, 25000, 0, -1, -1, 60, 40, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18991, 0, 1, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Restored cut Tauren mount.', 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 1819: Peon's Mining Pick (1H Axe)
(1819, 2, 0, -1, 'Peon\'s Mining Pick', 1395, 2, 0, 0, 1, 500, 100, 13, -1, -1, 15, 5, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 2, 7, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2000, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 2, 'Work, work!', 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90101: Shroud of the Upside-Down Sinner (Epic Cloak, Level 60)
(90101, 4, 0, -1, 'Shroud of the Upside-Down Sinner', 24175, 4, 0, 0, 1, 75000, 18000, 16, -1, -1, 66, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 7, 16, 5, 14, 45, 22, 36, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1, 'Recovered from the depths of the Karazhan Catacombs.', 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90102: Tormentor's Hooked Scythe (Epic Polearm, Level 60)
(90102, 2, 6, -1, 'Tormentor\'s Hooked Scythe', 28014, 4, 0, 0, 1, 120000, 30000, 17, -1, -1, 66, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 28, 3, 26, 32, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 185, 278, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3500, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1, 'The chilling edge reaps both flesh and soul.', 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90103: Cryptbreaker's Warhammer (Epic 2H Mace, Level 60)
(90103, 2, 5, -1, 'Cryptbreaker\'s Warhammer', 29112, 4, 0, 0, 1, 120000, 30000, 17, -1, -1, 66, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 32, 7, 25, 38, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 289, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3600, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1, 'Forged to smash through subterranean sealed tombs.', 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90104: Robes of the Forgotten Necromancer (Epic Cloth Robe, Level 60)
(90104, 4, 1, -1, 'Robes of the Forgotten Necromancer', 27015, 4, 0, 0, 1, 90000, 22000, 20, -1, -1, 66, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 7, 24, 5, 22, 45, 34, 32, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 115, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 1, 'Imbued with the lingering necrotic echoes of the crypts.', 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90105: Band of the Catacombs (Epic Ring, Level 60)
(90105, 4, 0, -1, 'Band of the Catacombs', 18880, 4, 0, 0, 1, 60000, 15000, 11, -1, -1, 66, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 3, 14, 4, 14, 38, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, '', 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90106: Talisman of the Crypt Watcher (Epic Trinket, Level 60)
(90106, 4, 0, -1, 'Talisman of the Crypt Watcher', 23214, 4, 0, 0, 1, 60000, 15000, 12, -1, -1, 66, 60, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 7, 20, 45, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 23505, 0, 1, 0, 120000, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Use: Absorbs 500 shadow damage and increases spell power by 40 for 20 sec.', 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- Leveling Hybrid Relics, Librams, Totems, Idols
-- 90201: Libram of the Holy Lightbearer (Paladin Libram, Level 30)
(90201, 4, 7, -1, 'Libram of the Holy Lightbearer', 29000, 3, 0, 0, 1, 2000, 500, 28, 2, -1, 35, 30, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 5, 8, 45, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases healing and spell power of your Holy spells by up to 14.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90202: Libram of Righteous Zealousness (Paladin Libram, Level 50)
(90202, 4, 7, -1, 'Libram of Righteous Zealousness', 29001, 3, 0, 0, 1, 10000, 2500, 28, 2, -1, 55, 50, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 12, 38, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases attack power by 32 for Retribution Paladins.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90203: Totem of Raging Elements (Shaman Totem, Level 30)
(90203, 4, 9, -1, 'Totem of Raging Elements', 29010, 3, 0, 0, 1, 2000, 500, 28, 64, -1, 35, 30, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 5, 8, 45, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases damage done by Lightning Bolt and Chain Lightning by up to 14.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90204: Totem of the Earthwarden (Shaman Totem, Level 50)
(90204, 4, 9, -1, 'Totem of the Earthwarden', 29011, 3, 0, 0, 1, 10000, 2500, 28, 64, -1, 55, 50, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 12, 38, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases attack power by 32 for Enhancement Shamans.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90205: Idol of the Wild Prowler (Druid Idol, Level 30)
(90205, 4, 8, -1, 'Idol of the Wild Prowler', 29020, 3, 0, 0, 1, 2000, 500, 28, 1024, -1, 35, 30, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 3, 8, 38, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases attack power in Cat and Bear forms by 24.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90206: Idol of Moonglade Harmony (Druid Idol, Level 50)
(90206, 4, 8, -1, 'Idol of Moonglade Harmony', 29021, 3, 0, 0, 1, 10000, 2500, 28, 1024, -1, 55, 50, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 5, 12, 45, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases healing and spell damage done by Druid spells by up to 20.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90207: Sigil of Unholy Dread (Death Knight Sigil, Level 58)
(90207, 4, 10, -1, 'Sigil of Unholy Dread', 29030, 3, 0, 0, 1, 12000, 3000, 28, 32, -1, 60, 58, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 4, 14, 7, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases the damage of Death Coil and Scourge Strike by 35.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90208: Tome of the Dark Arts (Necromancer Relic, Level 40)
(90208, 4, 10, -1, 'Tome of the Dark Arts', 29035, 3, 0, 0, 1, 5000, 1200, 28, -1, -1, 45, 40, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 7, 12, 45, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Increases shadow and frost spell power by up to 18.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340),

-- 90209: Relic of the Old Gods (Cultist Relic, Level 40)
(90209, 4, 10, -1, 'Relic of the Old Gods', 29040, 3, 0, 0, 1, 5000, 1200, 28, -1, -1, 45, 40, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 7, 12, 45, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0, -1, 1, 'Equip: Whispers from the deep empower your dark rites, increasing spell power by 18.', 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, '', 0, 0, 0, 0, 0, 12340);
