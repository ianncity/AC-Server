-- Prestige rewards are granted directly on rank-up; the retired vendor stock is no longer exposed.
DELETE FROM `npc_vendor` WHERE `entry` IN (96000, 96002);
-- The old token shop was implemented as gossip menu options and NPC text.
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 96000;
DELETE FROM `gossip_menu` WHERE `MenuID` = 96000;
DELETE FROM `npc_text` WHERE `ID` = 96000;

-- Keep only the prestige gossip action. The old data marked this NPC as both a
-- vendor and a gossip NPC, exposing obsolete token purchases.
UPDATE `creature_template`
SET `npcflag` = 1, `gossip_menu_id` = 96000, `ScriptName` = 'npc_prestige_master'
WHERE `entry` = 96000;

-- Keep the existing Horde template and its spawn data. The old version cloned
-- the Alliance template on every update, which could erase Horde-specific data.
UPDATE `creature_template`
SET `faction` = 83, `npcflag` = 1, `gossip_menu_id` = 96000, `ScriptName` = 'npc_prestige_master'
WHERE `entry` = 96002;

-- Complete human/orc NPC appearances include baked skin and equipment.
-- Bare player displays 49/51 have no CreatureDisplayInfoExtra appearance.
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES (96000, 0, 5566, 1.0, 1.0, 12340), (96002, 0, 4259, 1.0, 1.0, 12340)
ON DUPLICATE KEY UPDATE `CreatureDisplayID` = VALUES(`CreatureDisplayID`),
    `DisplayScale` = VALUES(`DisplayScale`), `Probability` = VALUES(`Probability`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);

-- Preserve the existing spawn location while switching its template to Horde.
UPDATE `creature` SET `id` = 96002 WHERE `guid` = 960002 AND `map` = 1;
INSERT INTO `creature`
    (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
     `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
     `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`)
SELECT 960002, 96002, 1, 1, 1, 1580.4, -4410.2, 8.1, 0.45, 30, 0, 0, 50000, 50000, 0, 1, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM `creature` WHERE `guid` = 960002);

-- Remove the vendor bit from any already-loaded clone.
UPDATE `creature_template` SET `npcflag` = 1 WHERE `entry` IN (96000, 96002);

-- Model 51/52 are the male/female Orc displays. Supply their collision data
-- instead of leaving zero-sized Horde prestige masters in the world.
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
