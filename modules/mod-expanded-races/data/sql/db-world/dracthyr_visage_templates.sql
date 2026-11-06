-- SPELL_AURA_TRANSFORM resolves its misc value as a creature-template entry.
-- The Dracthyr Visage spells use 50004/50005, so provide templates whose
-- display rows resolve to the client-side Human visage models.
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (50004, 50005);
DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (50004, 50005);
DELETE FROM `creature_template_resistance` WHERE `CreatureID` IN (50004, 50005);
DELETE FROM `creature_immunities` WHERE `ID` IN (50004, 50005);
DELETE FROM `creature_template` WHERE `entry` IN (50004, 50005);

DROP TEMPORARY TABLE IF EXISTS `dracthyr_visage_template`;
CREATE TEMPORARY TABLE `dracthyr_visage_template` LIKE `creature_template`;
INSERT INTO `dracthyr_visage_template`
SELECT * FROM `creature_template` WHERE `entry` = 1;

UPDATE `dracthyr_visage_template`
SET `entry` = 50004,
    `name` = 'Dracthyr Visage Male',
    `subname` = '',
    `gossip_menu_id` = 0,
    `minlevel` = 1,
    `maxlevel` = 1,
    `faction` = 35,
    `npcflag` = 0,
    `unit_flags` = 0,
    `unit_flags2` = 0,
    `dynamicflags` = 0,
    `AIName` = '',
    `MovementType` = 0,
    `flags_extra` = 0,
    `ScriptName` = '';
INSERT INTO `creature_template` SELECT * FROM `dracthyr_visage_template`;

UPDATE `dracthyr_visage_template`
SET `entry` = 50005,
    `name` = 'Dracthyr Visage Female';
INSERT INTO `creature_template` SELECT * FROM `dracthyr_visage_template`;
DROP TEMPORARY TABLE `dracthyr_visage_template`;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (50004, 0, 10016, 1.0, 1.0, 12340),
    (50005, 0, 10017, 1.0, 1.0, 12340);

-- ObjectMgr requires model sizing metadata for every creature display. Match
-- the human model measurements and cross-link the two Dracthyr Visage genders.
INSERT INTO `creature_model_info`
    (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`, `VerifiedBuild`)
VALUES
    (10016, 0.306, 1.5, 0, 10017, 12340),
    (10017, 0.208, 1.5, 1, 10016, 12340)
ON DUPLICATE KEY UPDATE
    `BoundingRadius` = VALUES(`BoundingRadius`),
    `CombatReach` = VALUES(`CombatReach`),
    `Gender` = VALUES(`Gender`),
    `DisplayID_Other_Gender` = VALUES(`DisplayID_Other_Gender`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);
