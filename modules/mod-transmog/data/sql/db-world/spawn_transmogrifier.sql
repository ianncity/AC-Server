-- Spawn Warpweaver Transmogrifier NPC in Dalaran, Stormwind, and Orgrimmar
-- NPC entry 190010 (Warpweaver)

-- Dalaran (near Krasus' Landing / Eventide)
DELETE FROM `creature` WHERE `id` = 190010;
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `curhealth`, `curmana`, `MovementType`) VALUES
(190010, 571, 4395, 4395, 1, 1, 5812.8, 646.9, 647.4, 1.57, 120, 1000, 1000, 0), -- Dalaran
(190010, 0, 1519, 1519, 1, 1, -8838.2, 624.9, 93.9, 4.71, 120, 1000, 1000, 0),   -- Stormwind (Trade District)
(190010, 1, 1637, 1637, 1, 1, 1633.5, -4439.1, 15.7, 0.0, 120, 1000, 1000, 0);     -- Orgrimmar (Valley of Strength)
