-- Give each prestige master a display that matches its faction template.
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
