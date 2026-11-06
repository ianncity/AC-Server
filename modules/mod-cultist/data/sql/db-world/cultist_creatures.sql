-- Creature templates for Cultist summons, using the current AzerothCore model table.

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 95010 AND 95011;
DELETE FROM `creature_template_spell` WHERE `CreatureID` BETWEEN 95010 AND 95011;
DELETE FROM `creature_template` WHERE `entry` BETWEEN 95010 AND 95011;

INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `dmgschool`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `type`, `type_flags`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `DamageModifier`, `ExperienceModifier`, `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(95010, 'Tentacle of N''Zoth', 'Void Minion', 1, 80, 35, 0, 1, 1.14286, 0, 0, 2000, 0, 1, 1, 1, 2048, 0, 0, 6, 0, '', 0, 1, 1.2, 1, 1, 1.1, 1, 1, 0, 'advai_void_servant', 12340),
(95011, 'Faceless Servant', 'Old God Guardian', 1, 80, 35, 0, 1, 1.14286, 0, 0, 2200, 0, 1, 1, 8, 2048, 0, 0, 6, 0, '', 0, 1, 1.6, 1.5, 1.5, 1.3, 1, 1, 0, 'advai_void_servant', 12340);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(95010, 0, 15789, 0.8, 1, 12340),
(95011, 0, 27849, 1.0, 1, 12340);

INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`) VALUES
(95011, 3, 15407),
(95011, 4, 8092);
