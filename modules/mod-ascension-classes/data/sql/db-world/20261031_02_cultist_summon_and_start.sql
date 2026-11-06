-- Make the custom classes functional from character creation and from their summons.
--
-- 1. Two summon spells point at creature entries that do not exist, so Mindbender
--    (50328) and Faceless Servant (50095) spawn nothing at all. Both are cloned from
--    the Voidwalker templates, which already carry the pet flags, faction and AI the
--    summon code expects.
-- 2. Classes 12 and 13 had no starting weapon/armour skills at all and no level-1
--    abilities in playercreateinfo, so a fresh character had two action-bar buttons
--    and nothing else. Cultist mirrors the Warlock skill set, Necromancer the Priest.

DROP TEMPORARY TABLE IF EXISTS `advai_ct_tmp`, `advai_ct_model_tmp`;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (50328, 50095);
DELETE FROM `creature_template` WHERE `entry` IN (50328, 50095);

CREATE TEMPORARY TABLE `advai_ct_tmp` AS SELECT * FROM `creature_template` WHERE `entry` = 1860;
UPDATE `advai_ct_tmp` SET `entry` = 50328, `name` = 'Mindbender', `subname` = 'Void Minion';
INSERT INTO `creature_template` SELECT * FROM `advai_ct_tmp`;
DROP TEMPORARY TABLE `advai_ct_tmp`;

CREATE TEMPORARY TABLE `advai_ct_tmp` AS SELECT * FROM `creature_template` WHERE `entry` = 1861;
UPDATE `advai_ct_tmp` SET `entry` = 50095, `name` = 'Faceless Servant', `subname` = 'Void Servant';
INSERT INTO `creature_template` SELECT * FROM `advai_ct_tmp`;
DROP TEMPORARY TABLE `advai_ct_tmp`;

CREATE TEMPORARY TABLE `advai_ct_model_tmp` AS SELECT * FROM `creature_template_model` WHERE `CreatureID` = 1860;
UPDATE `advai_ct_model_tmp` SET `CreatureID` = 50328;
INSERT INTO `creature_template_model` SELECT * FROM `advai_ct_model_tmp`;
DROP TEMPORARY TABLE `advai_ct_model_tmp`;

CREATE TEMPORARY TABLE `advai_ct_model_tmp` AS SELECT * FROM `creature_template_model` WHERE `CreatureID` = 1861;
UPDATE `advai_ct_model_tmp` SET `CreatureID` = 50095;
INSERT INTO `creature_template_model` SELECT * FROM `advai_ct_model_tmp`;
DROP TEMPORARY TABLE `advai_ct_model_tmp`;

INSERT IGNORE INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`)
SELECT `raceMask`, 2048, `skill`, `rank`, 'Cultist'
FROM `playercreateinfo_skills` WHERE `classMask` = 256;

INSERT IGNORE INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`)
SELECT `raceMask`, 4096, `skill`, `rank`, 'Necromancer'
FROM `playercreateinfo_skills` WHERE `classMask` = 16;

INSERT IGNORE INTO `playercreateinfo_spell_custom` (`raceMask`, `classMask`, `spell`)
SELECT 0, 2048, `SpellId` FROM `trainer_spell`
WHERE `TrainerId` = 98000 AND `ReqLevel` = 1 AND (`ClassMask` & 2048);

INSERT IGNORE INTO `playercreateinfo_spell_custom` (`raceMask`, `classMask`, `spell`)
SELECT 0, 4096, `SpellId` FROM `trainer_spell`
WHERE `TrainerId` = 98001 AND `ReqLevel` = 1 AND (`ClassMask` & 4096);
