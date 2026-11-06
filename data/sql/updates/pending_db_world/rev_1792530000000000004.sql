-- Correct the transient Risen Ghoul to the source-defined WotLK Ghoul display.
START TRANSACTION;

REPLACE INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (542065, 0, 32774, 1, 1, NULL);

COMMIT;
