-- Creature Templates for Necromancer Summons (Vanilla 3.3.5 / AzerothCore)
-- 95000: Necromancer Initiate (Skeleton Melee)
-- 95001: Grave Mage (Skeleton Frost/Shadow Caster)
-- 95002: Skeletal Archer (Skeleton Ranged)
-- 95003: Flesh Golem (Undead Construct Tank)

DELETE FROM `creature_template` WHERE `entry` BETWEEN 95000 AND 95003;
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (95000,95001,95002,95003);
DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (95000,95001,95002,95003);
DELETE FROM `creature_template_resistance` WHERE `CreatureID` IN (95000,95001,95002,95003);
DELETE FROM `creature_immunities` WHERE `ID` IN (95000,95001,95002,95003);
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(95000, 0, 0, 0, 0, 0, 'Necromancer Initiate', 'Undead Minion', '', 0, 1, 80, 0, 35, 0, 1, 1.14286, 20, 0, 0, 1, 2000, 0, 1, 1, 1, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 'npc_ascension_necromancer', 12340),
(95001, 0, 0, 0, 0, 0, 'Grave Mage', 'Undead Caster', '', 0, 1, 80, 0, 35, 0, 1, 1.14286, 20, 0, 0, 1, 2000, 2000, 1, 1, 8, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1.5, 0.8, 1, 0, 0, 1, 0, 0, 'npc_ascension_necromancer', 12340),
(95002, 0, 0, 0, 0, 0, 'Skeletal Archer', 'Undead Ranged', '', 0, 1, 80, 0, 35, 0, 1, 1.14286, 20, 0, 0, 1.1, 2000, 1800, 1, 1, 2, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 'npc_ascension_necromancer', 12340),
(95003, 0, 0, 0, 0, 0, 'Flesh Golem', 'Undead Construct', '', 0, 1, 80, 0, 35, 0, 1, 1.14286, 20, 0, 0, 1.2, 2500, 0, 1, 1, 1, 0, 2048, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1.8, 0.5, 2.0, 1, 0, 0, 1, 0, 0, 'npc_ascension_necromancer', 12340);
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(95000,0,158,1,1,12340),
(95001,0,2606,1,1,12340),
(95002,0,3507,1,1,12340),
(95003,0,1693,1.25,1,12340);
INSERT INTO `creature_template_spell` (`CreatureID`,`Index`,`Spell`) VALUES
(95001,0,116),
(95001,1,686),
(95002,0,75);
