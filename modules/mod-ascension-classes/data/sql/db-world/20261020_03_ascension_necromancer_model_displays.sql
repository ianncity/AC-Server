-- Keep the live Necromancer summons on models supported by the WotLK client.
-- The earlier module migration already defines these IDs, but existing worlds
-- may have retained legacy CreatureDisplayInfo rows from older SQL imports.
START TRANSACTION;

DELETE FROM `creature_model_info` WHERE `DisplayID` = 136859;

INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (50065, 0, 158,   1,    1, NULL),
    (50075, 0, 2606,  1,    1, NULL),
    (50076, 0, 9790,  1.1,  1, NULL),
    (50078, 0, 158,   1,    1, NULL),
    (50261, 0, 158,   1,    1, NULL),
    (51065, 0, 775,   1.25, 1, NULL),
    (500482, 0, 158,  1,    1, NULL),
    (500483, 0, 2606, 1,    1, NULL),
    (500484, 0, 7857, 1,    1, NULL),
    (95002, 0, 9790,  1.1,  1, NULL)
ON DUPLICATE KEY UPDATE
    `CreatureDisplayID` = VALUES(`CreatureDisplayID`),
    `DisplayScale` = VALUES(`DisplayScale`),
    `Probability` = VALUES(`Probability`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);

-- Cultist Grimoire of Supremacy Demons & Doodads
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (58959, 0, 16890, 0.9, 1, 12340),
    (58960, 0, 18170, 1.0, 1, 12340),
    (58963, 0, 21248, 1.0, 1, 12340),
    (58964, 0, 20590, 0.9, 1, 12340),
    (58965, 0, 20045, 1.0, 1, 12340),
    (58997, 0, 22810, 1.0, 1, 12340),
    (59000, 0, 18373, 1.1, 1, 12340),
    (542064, 0, 977363, 1.0, 1, 12340)
ON DUPLICATE KEY UPDATE
    `CreatureDisplayID` = VALUES(`CreatureDisplayID`),
    `DisplayScale` = VALUES(`DisplayScale`),
    `Probability` = VALUES(`Probability`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);

-- Summon Equipment Configurations
DELETE FROM `creature_equip_template` WHERE `CreatureID` IN (50065, 50076, 50078, 51065, 95002, 58959, 58960, 58963, 58964, 58965, 58997, 59000, 542064);

INSERT INTO `creature_equip_template`
    (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
VALUES
    (51065, 1, 1897, 12893, 0, 12340),     -- Greater Skeletal Warrior: Scimitar + Black Skull Shield
    (50065, 1, 1897, 0, 0, 12340),         -- Lesser Skeletal Warrior: Scimitar
    (50078, 1, 2184, 2184, 0, 12340),     -- Skeletal Rogue: Dual Daggers
    (50076, 1, 0, 0, 14105, 12340),        -- Skeletal Archer: Scourge Bow
    (95002, 1, 0, 0, 14105, 12340),        -- Skeletal Archer: Scourge Bow
    (58965, 1, 10613, 10613, 0, 12340);    -- Wrathguard: Dual Katana Swords

COMMIT;
