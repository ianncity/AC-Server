-- Master of Prestige used CreatureDisplayInfo 49 / 51: the bare HumanMale / OrcMale character models with
-- no CreatureDisplayInfoExtra, which render as untextured, unclothed figures. Use fully dressed NPC
-- displays instead (Archmage Malin for the Alliance master, Overlord Runthak for the Horde master).
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (96000, 96002);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(96000, 0, 2968, 1, 1, 12340),
(96002, 0, 14429, 1, 1, 12340);
