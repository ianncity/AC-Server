-- Reassert the verified WotLK Necromancer summon displays after the optional
-- class-content seed, which otherwise restores legacy display mappings.
START TRANSACTION;

DELETE FROM `creature_model_info` WHERE `DisplayID` = 136859;

DELETE FROM `creature_template_model` WHERE (`CreatureID`,`Idx`) IN ((50065,0),(50075,0),(50076,0),(50078,0),(50261,0),(51065,0),(500482,0),(500483,0),(500484,0),(95002,0));
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (50065, 0, 32758, 1, 1, NULL),
    (50075, 0, 32765, 1, 1, NULL),
    (50076, 0, 32757, 1, 1, NULL),
    (50078, 0, 29404, 1, 1, NULL),
    (50261, 0, 32773, 1, 1, NULL),
    (51065, 0, 32756, 1, 1, NULL),
    (500482, 0, 32756, 1, 1, NULL),
    (500483, 0, 32765, 1, 1, NULL),
    (500484, 0, 32782, 0.4, 1, NULL),
    (95002, 0, 32757, 1, 1, NULL)
ON DUPLICATE KEY UPDATE
    `CreatureDisplayID` = VALUES(`CreatureDisplayID`),
    `DisplayScale` = VALUES(`DisplayScale`),
    `Probability` = VALUES(`Probability`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);

COMMIT;
