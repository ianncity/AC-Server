-- ============================================================================
-- Expanded Race and Class Combinations for AzerothCore (TrinityCore 3.3.5a)
-- Custom World Database SQL Script
-- ============================================================================

-- 1. playercreateinfo: Insert starting coordinates, map, and zone for new race/class combos
-- Using exact starting locations of each respective race.

INSERT INTO `playercreateinfo` (`race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`) VALUES
-- Human Hunter (1, 3) - Human start (Map 0, Zone 12)
(1, 3, 0, 12, -8949.95, -132.493, 83.5312, 0),

-- Orc Mage (2, 8) & Orc Priest (2, 5) - Orc start (Map 1, Zone 14)
(2, 8, 1, 14, -618.518, -4251.67, 38.718, 0),
(2, 5, 1, 14, -618.518, -4251.67, 38.718, 0),

-- Dwarf Shaman (3, 7), Dwarf Mage (3, 8), Dwarf Warlock (3, 9) - Dwarf start (Map 0, Zone 1)
(3, 7, 0, 1, -6240.32, 331.033, 382.758, 6.17716),
(3, 8, 0, 1, -6240.32, 331.033, 382.758, 6.17716),
(3, 9, 0, 1, -6240.32, 331.033, 382.758, 6.17716),

-- Night Elf Mage (4, 8) - Night Elf start (Map 1, Zone 141)
(4, 8, 1, 141, 10311.3, 832.463, 1326.41, 5.69632),

-- Undead Hunter (5, 3) & Undead Paladin (5, 2) - Undead start (Map 0, Zone 85)
(5, 3, 0, 85, 1676.71, 1678.31, 121.67, 2.70526),
(5, 2, 0, 85, 1676.71, 1678.31, 121.67, 2.70526),

-- Tauren Paladin (6, 2) & Tauren Priest (6, 5) - Tauren start (Map 1, Zone 215)
(6, 2, 1, 215, -2917.58, -257.98, 52.9968, 0),
(6, 5, 1, 215, -2917.58, -257.98, 52.9968, 0),

-- Gnome Priest (7, 5) & Gnome Hunter (7, 3) - Gnome start (Map 0, Zone 1)
(7, 5, 0, 1, -6240.32, 331.033, 382.758, 0),
(7, 3, 0, 1, -6240.32, 331.033, 382.758, 0),

-- Troll Druid (8, 11) & Troll Warlock (8, 9) - Troll start (Map 1, Zone 14)
(8, 11, 1, 14, -618.518, -4251.67, 38.718, 0),
(8, 9, 1, 14, -618.518, -4251.67, 38.718, 0),

-- Blood Elf Warrior (10, 1) - Blood Elf start (Map 530, Zone 3431)
(10, 1, 530, 3431, 10349.6, -6357.29, 33.4026, 5.31605),

-- Draenei Rogue (11, 4) - Draenei start (Map 530, Zone 3526)
(11, 4, 530, 3526, -3961.64, -13931.2, 100.615, 2.08364);


-- 2. playercreateinfo_action: Configure action bar layouts and racial spell buttons
-- Racial spells:
-- Human (1): 59752 (Every Man for Himself)
-- Orc (2): 20572 (Blood Fury)
-- Dwarf (3): 20594 (Stoneform)
-- Night Elf (4): 58984 (Shadowmeld)
-- Undead (5): 20577 (Will of the Forsaken)
-- Tauren (6): 20549 / War Stomp (Note: Tauren racial War Stomp spell ID is 20549)
-- Gnome (7): 20589 (Escape Artist)
-- Troll (8): 20554 (Berserking)
-- Blood Elf (10): 28730 (Arcane Torrent)
-- Draenei (11): 59544 (Gift of the Naaru)

INSERT INTO `playercreateinfo_action` (`race`, `class`, `button`, `action`, `type`) VALUES
-- Human Hunter (1, 3)
(1, 3, 0, 6603, 0),
(1, 3, 1, 2973, 0),
(1, 3, 2, 75, 0),
(1, 3, 3, 59752, 0), -- Every Man for Himself

-- Orc Mage (2, 8)
(2, 8, 0, 133, 0),
(2, 8, 1, 168, 0),
(2, 8, 9, 20572, 0), -- Blood Fury

-- Orc Priest (2, 5)
(2, 5, 0, 585, 0),
(2, 5, 1, 2050, 0),
(2, 5, 9, 20572, 0), -- Blood Fury

-- Dwarf Shaman (3, 7)
(3, 7, 0, 6603, 0),
(3, 7, 1, 403, 0),
(3, 7, 2, 331, 0),
(3, 7, 3, 20594, 0), -- Stoneform

-- Dwarf Mage (3, 8)
(3, 8, 0, 133, 0),
(3, 8, 1, 168, 0),
(3, 8, 9, 20594, 0), -- Stoneform

-- Dwarf Warlock (3, 9)
(3, 9, 0, 686, 0),
(3, 9, 1, 687, 0),
(3, 9, 2, 20594, 0), -- Stoneform

-- Night Elf Mage (4, 8)
(4, 8, 0, 133, 0),
(4, 8, 1, 168, 0),
(4, 8, 9, 58984, 0), -- Shadowmeld

-- Undead Hunter (5, 3)
(5, 3, 0, 6603, 0),
(5, 3, 1, 2973, 0),
(5, 3, 2, 75, 0),
(5, 3, 3, 20577, 0), -- Will of the Forsaken

-- Undead Paladin (5, 2)
(5, 2, 0, 6603, 0),
(5, 2, 1, 21084, 0),
(5, 2, 2, 635, 0),
(5, 2, 3, 20577, 0), -- Will of the Forsaken

-- Tauren Paladin (6, 2)
(6, 2, 0, 6603, 0),
(6, 2, 1, 21084, 0),
(6, 2, 2, 635, 0),
(6, 2, 10, 20549, 0), -- War Stomp

-- Tauren Priest (6, 5)
(6, 5, 0, 585, 0),
(6, 5, 1, 2050, 0),
(6, 5, 2, 20549, 0), -- War Stomp

-- Gnome Priest (7, 5)
(7, 5, 0, 585, 0),
(7, 5, 1, 2050, 0),
(7, 5, 9, 20589, 0), -- Escape Artist

-- Gnome Hunter (7, 3)
(7, 3, 0, 6603, 0),
(7, 3, 1, 2973, 0),
(7, 3, 2, 75, 0),
(7, 3, 3, 20589, 0), -- Escape Artist

-- Troll Druid (8, 11)
(8, 11, 0, 5176, 0),
(8, 11, 1, 5185, 0),
(8, 11, 2, 20554, 0), -- Berserking

-- Troll Warlock (8, 9)
(8, 9, 0, 686, 0),
(8, 9, 1, 687, 0),
(8, 9, 2, 20554, 0), -- Berserking

-- Blood Elf Warrior (10, 1)
(10, 1, 72, 6603, 0),
(10, 1, 73, 78, 0),
(10, 1, 74, 28730, 0), -- Arcane Torrent
(10, 1, 84, 6603, 0),
(10, 1, 96, 6603, 0),

-- Draenei Rogue (11, 4)
(11, 4, 0, 6603, 0),
(11, 4, 1, 1752, 0),
(11, 4, 2, 2098, 0),
(11, 4, 3, 2764, 0),
(11, 4, 4, 59544, 0); -- Gift of the Naaru
