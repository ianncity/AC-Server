-- Keep the prestige masters faction-correct and remove the obsolete token shop.
START TRANSACTION;

DELETE FROM `npc_vendor` WHERE `entry` IN (96000, 96002);
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 96000;
DELETE FROM `gossip_menu` WHERE `MenuID` = 96000;
DELETE FROM `npc_text` WHERE `ID` = 96000;

UPDATE `creature_template` SET `faction` = 35, `npcflag` = 1, `gossip_menu_id` = 96000, `ScriptName` = 'npc_prestige_master' WHERE `entry` = 96000;

UPDATE `creature_template` SET `faction` = 83, `npcflag` = 1, `gossip_menu_id` = 96000, `ScriptName` = 'npc_prestige_master' WHERE `entry` = 96002;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (96000, 96002);
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (96000, 0, 49, 1.0, 1.0, 12340),
    (96002, 0, 51, 1.0, 1.0, 12340)
ON DUPLICATE KEY UPDATE
    `CreatureDisplayID` = VALUES(`CreatureDisplayID`),
    `DisplayScale` = VALUES(`DisplayScale`),
    `Probability` = VALUES(`Probability`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);

-- The Horde display rows were missing collision metadata. Zero radii make the
-- model invalid to the server's creature model loader even when the display ID
-- itself is correct.
DELETE FROM `creature_model_info` WHERE `DisplayID` IN (49, 50, 51, 52);
INSERT INTO `creature_model_info`
    (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`, `VerifiedBuild`)
VALUES
    (49, 0.306, 1.5, 0, 50, 12340),
    (50, 0.208, 1.5, 1, 49, 12340),
    (51, 0.306, 1.5, 0, 52, 12340),
    (52, 0.208, 1.5, 1, 51, 12340)
ON DUPLICATE KEY UPDATE
    `BoundingRadius` = VALUES(`BoundingRadius`),
    `CombatReach` = VALUES(`CombatReach`),
    `Gender` = VALUES(`Gender`),
    `DisplayID_Other_Gender` = VALUES(`DisplayID_Other_Gender`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);

UPDATE `creature` SET `id` = 96002 WHERE `id` = 96000 AND `guid` = 960002 AND `map` = 1;
COMMIT;
