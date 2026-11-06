-- Restore Necromancer summon displays to models present in the WotLK 3.3.5 client.
-- Display IDs and texture variations were checked against the local extracted
-- CreatureDisplayInfo.dbc and CreatureModelData.dbc; these are existing stock rows.
START TRANSACTION;

-- The module seed's 136859 display was a later-era model (CreatureModelData 112876).
DELETE FROM `creature_model_info` WHERE `DisplayID` = 136859;

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (50065,50067,50068,50073,50075,50076,50078,50115,50132,50133,50173,50177,50261,50303,50309,50320,50323,50385,51065,51275,500482,500483,500484,503030,503031,503032,503200,523032,542065) AND `Idx` = 0;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
    (50065, 0, 200,   1,    1, NULL),
    (50067, 0, 7534,  1,    1, NULL),
    (50068, 0, 7864,  0.5,  1, NULL),
    (50073, 0, 519,   1,    1, NULL),
    (50075, 0, 1245,  1,    1, NULL),
    (50076, 0, 7550,  1,    1, NULL),
    (50078, 0, 29404, 1,    1, NULL),
    (50115, 0, 27979, 0.25, 1, NULL),
    (50132, 0, 24889, 1,    1, NULL),
    (50133, 0, 7898,  1,    1, NULL),
    (50173, 0, 414,   1,    1, NULL),
    (50177, 0, 30902, 0.5,  1, NULL),
    (50261, 0, 29404, 1,    1, NULL),
    (50303, 0, 137,   1,    1, NULL),
    (50309, 0, 30810, 1,    1, NULL),
    (50320, 0, 22122, 0.75, 1, NULL),
    (50323, 0, 7845,  0.5,  1, NULL),
    (50385, 0, 7864,  0.35, 1, NULL),
    (51065, 0, 200,   1,    1, NULL),
    (51275, 0, 30843, 0.2,  1, NULL),
    (500482, 0, 200,  1,    1, NULL),
    (500483, 0, 1245, 1,    1, NULL),
    (500484, 0, 30810, 0.4, 1, NULL),
    (503030, 0, 1202, 1,   1, NULL),
    (503031, 0, 1202, 1,   1, NULL),
    (503032, 0, 1200, 1,   1, NULL),
    (503200, 0, 31092, 1,  1, NULL),
    (523032, 0, 25245, 0.3, 1, NULL),
    (542065, 0, 137,   1,   1, NULL);

COMMIT;
