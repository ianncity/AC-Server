-- Primal Elementalist: original MoP appearances with native Wrath elemental stat budgets.
-- Four source abilities per elemental fit the four native guardian spell buttons.
-- Empty ScriptName/AIName allows native controlled-guardian PetAI; no old autonomous AI.
REPLACE INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(61029, 0, 0, 0, 0, 0, 'Primal Fire Elemental', '', NULL, 0, 68, 80, 0, 14, 0, 1.0, 1.14286, 1.0, 1.0, 20.0, 0, 2, 1.0, 2000, 2000, 1.0, 1.0, 2, 0, 2048, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 1.0, 1.0, 1.0, 1.0, 0, 0, 1, -4, 0, '', 0),
(61056, 0, 0, 0, 0, 0, 'Primal Earth Elemental', '', NULL, 0, 66, 66, 1, 14, 0, 1.55556, 1.14286, 1.0, 1.0, 20.0, 0, 0, 1.5, 2000, 2000, 1.0, 1.0, 1, 0, 2048, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 1.3, 1.0, 1.0, 1.0, 0, 0, 1, -5, 0, '', 0);

REPLACE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(61029, 0, 37986, 1.0, 1.0, 18019),
(61056, 0, 37987, 1.0, 1.0, 18019);

REPLACE INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`, `VerifiedBuild`) VALUES
(37986, 1.5, 3.0, 2, 0, 0),
(37987, 2.0, 4.2, 2, 0, 0);

DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (61029, 61056);
INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`, `VerifiedBuild`) VALUES
(61029, 0, 57984, 0),
(61029, 1, 12470, 0),
(61029, 2, 991039, 0),
(61029, 3, 991034, 0),
(61056, 0, 36213, 0),
(61056, 1, 991037, 0),
(61056, 2, 991036, 0),
(61056, 3, 991035, 0);
