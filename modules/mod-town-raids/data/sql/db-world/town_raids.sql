-- =========================================================================
-- Town Raids Database Schema (AzerothCore World Database)
-- Creature Templates and Models for Raid Varieties (Demons, Bandits, Scourge, Twilight)
-- =========================================================================

DELETE FROM `creature_template` WHERE `entry` BETWEEN 97001 AND 97054;
DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 97001 AND 97054;

-- 1. Burning Legion Demons
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `type_flags`, `AIName`, `ScriptName`) VALUES
(97001, 'Felguard Raider', 'Legion Invader', 30, 70, 90, 0, 1.0, 1.25, 1, 1, 0, 2048, 3, 0, '', ''),
(97002, 'Felhound Stalker', 'Legion Invader', 30, 70, 90, 0, 1.0, 1.30, 1, 1, 0, 2048, 3, 0, '', ''),
(97003, 'Succubus Tormentor', 'Legion Invader', 30, 70, 90, 0, 1.0, 1.20, 1, 8, 0, 2048, 3, 0, '', ''),
(97004, 'Wrathguard Invader', 'Legion Elite', 35, 72, 90, 0, 1.0, 1.30, 2, 1, 0, 2048, 3, 0, '', ''),
(97005, 'Infernal Juggernaut', 'Legion Siege Construct', 40, 75, 90, 0, 1.0, 1.15, 2, 1, 0, 2048, 3, 0, '', ''),
(97006, 'Pit Lord Destructor', 'Legion Commander', 45, 80, 90, 0, 1.0, 1.35, 3, 1, 0, 2048, 3, 0, '', '');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(97001, 0, 4726, 1.1, 1.0),
(97002, 0, 4487, 1.0, 1.0),
(97003, 0, 4449, 1.0, 1.0),
(97004, 0, 17926, 1.2, 1.0),
(97005, 0, 169, 1.3, 1.0),
(97006, 0, 11462, 1.4, 1.0);

-- 2. Defias Syndicate Bandits
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `type_flags`, `AIName`, `ScriptName`) VALUES
(97010, 'Defias Cutpurse', 'Town Raider', 25, 65, 14, 0, 1.0, 1.25, 1, 1, 0, 2048, 7, 0, '', ''),
(97011, 'Defias Highwayman', 'Town Raider', 25, 65, 14, 0, 1.0, 1.20, 1, 2, 0, 2048, 7, 0, '', ''),
(97012, 'Defias Pyromancer', 'Town Raider', 25, 65, 14, 0, 1.0, 1.15, 1, 8, 0, 2048, 7, 0, '', ''),
(97013, 'Defias Enforcer', 'Town Raider', 28, 68, 14, 0, 1.0, 1.25, 2, 1, 0, 2048, 7, 0, '', ''),
(97014, 'Defias Mastermind', 'Outlaw Warlord', 32, 75, 14, 0, 1.0, 1.30, 3, 1, 0, 2048, 7, 0, '', '');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(97010, 0, 2357, 1.0, 1.0),
(97011, 0, 184, 1.0, 1.0),
(97012, 0, 1695, 1.0, 1.0),
(97013, 0, 4283, 1.1, 1.0),
(97014, 0, 1696, 1.2, 1.0);

-- 3. Scourge Undead
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `type_flags`, `AIName`, `ScriptName`) VALUES
(97020, 'Plague Ghoul', 'Scourge Raider', 25, 65, 14, 0, 1.0, 1.25, 1, 1, 0, 2048, 6, 0, '', ''),
(97021, 'Skeletal Frostmage', 'Scourge Raider', 25, 65, 14, 0, 1.0, 1.15, 1, 8, 0, 2048, 6, 0, '', ''),
(97022, 'Crypt Fiend Ravager', 'Scourge Raider', 28, 68, 14, 0, 1.0, 1.25, 1, 1, 0, 2048, 6, 0, '', ''),
(97023, 'Plagued Monstrosity', 'Scourge Abomination', 32, 72, 14, 0, 1.0, 1.15, 2, 1, 0, 2048, 6, 0, '', ''),
(97024, 'Scourge Lich', 'Scourge Warlord', 35, 78, 14, 0, 1.0, 1.30, 3, 8, 0, 2048, 6, 0, '', '');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(97020, 0, 1056, 1.0, 1.0),
(97021, 0, 1500, 1.0, 1.0),
(97022, 0, 11333, 1.1, 1.0),
(97023, 0, 8570, 1.3, 1.0),
(97024, 0, 8573, 1.2, 1.0);

-- 4. Twilight Cultists & Elementals
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `type_flags`, `AIName`, `ScriptName`) VALUES
(97030, 'Twilight Cultist', 'Twilight Invader', 25, 65, 14, 0, 1.0, 1.20, 1, 8, 0, 2048, 7, 0, '', ''),
(97031, 'Twilight Dark Ascendant', 'Twilight Invader', 28, 68, 14, 0, 1.0, 1.25, 1, 8, 0, 2048, 7, 0, '', ''),
(97032, 'Raging Fire Elemental', 'Twilight Siege Elemental', 32, 72, 14, 0, 1.0, 1.20, 2, 1, 0, 2048, 4, 0, '', ''),
(97033, 'Twilight War-Bringer', 'Twilight Commander', 35, 78, 14, 0, 1.0, 1.30, 3, 1, 0, 2048, 7, 0, '', '');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(97030, 0, 11686, 1.0, 1.0),
(97031, 0, 11685, 1.1, 1.0),
(97032, 0, 845, 1.2, 1.0),
(97033, 0, 11687, 1.2, 1.0);

-- 5. Southsea & Bloodsail Pirates
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `type_flags`, `AIName`, `ScriptName`) VALUES
(97040, 'Southsea Freebooter', 'Town Raider', 25, 75, 14, 0, 1.0, 1.25, 1, 1, 0, 2048, 7, 0, '', ''),
(97041, 'Southsea Cannoneer', 'Town Raider', 25, 75, 14, 0, 1.0, 1.15, 1, 2, 0, 2048, 7, 0, '', ''),
(97042, 'Southsea Cutthroat', 'Town Raider', 28, 75, 14, 0, 1.0, 1.30, 1, 1, 0, 2048, 7, 0, '', ''),
(97043, 'Southsea First Mate', 'Pirate Elite', 30, 78, 14, 0, 1.0, 1.25, 2, 1, 0, 2048, 7, 0, '', ''),
(97044, 'Dread Pirate Captain', 'Pirate Warlord', 35, 82, 14, 0, 1.0, 1.35, 3, 1, 0, 2048, 7, 0, '', '');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(97040, 0, 3829, 1.0, 1.0),
(97041, 0, 3827, 1.0, 1.0),
(97042, 0, 3835, 1.0, 1.0),
(97043, 0, 3833, 1.15, 1.0),
(97044, 0, 4638, 1.25, 1.0);

-- 6. Silithid & Qiraji Swarm
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `type_flags`, `AIName`, `ScriptName`) VALUES
(97050, 'Silithid Swarmer', 'Hive Invader', 25, 75, 14, 0, 1.0, 1.30, 1, 1, 0, 2048, 1, 0, '', ''),
(97051, 'Silithid Spitfire', 'Hive Invader', 25, 75, 14, 0, 1.0, 1.20, 1, 8, 0, 2048, 1, 0, '', ''),
(97052, 'Silithid Reaver', 'Hive Invader', 28, 75, 14, 0, 1.0, 1.25, 1, 1, 0, 2048, 1, 0, '', ''),
(97053, 'Silithid Colossus', 'Hive Behemoth', 32, 78, 14, 0, 1.0, 1.15, 2, 1, 0, 2048, 1, 0, '', ''),
(97054, 'Sandstalker Broodmother', 'Hive Queen', 35, 82, 14, 0, 1.0, 1.30, 3, 1, 0, 2048, 1, 0, '', '');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
(97050, 0, 2731, 1.0, 1.0),
(97051, 0, 1307, 1.0, 1.0),
(97052, 0, 11096, 1.1, 1.0),
(97053, 0, 12151, 1.3, 1.0),
(97054, 0, 15432, 1.35, 1.0);

UPDATE `creature_template`
SET `ScriptName` = 'town_raid_invader'
WHERE `entry` IN (
    97001, 97002, 97003, 97004, 97005, 97006,
    97010, 97011, 97012, 97013, 97014,
    97020, 97021, 97022, 97023, 97024,
    97030, 97031, 97032, 97033,
    97040, 97041, 97042, 97043, 97044,
    97050, 97051, 97052, 97053, 97054
);
