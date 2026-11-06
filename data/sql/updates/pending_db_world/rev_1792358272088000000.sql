-- Register Dracthyr Visage transform displays with server model sizing metadata.
DELETE FROM `creature_model_info` WHERE `DisplayID` IN (10016, 10017);
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
