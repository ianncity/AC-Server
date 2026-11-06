-- Bring the live class-trainer catalog in line with the current Ascension ability source.
-- Restore the lower Twilight Shieldtoss rank and require it before rank two.
DELETE FROM `trainer_spell` WHERE (`TrainerId` = 98000 AND `SpellId` IN (524876, 704874, 704886, 805133, 806276)) OR (`TrainerId` = 98001 AND `SpellId` IN (802124, 806151));
INSERT INTO `trainer_spell`
    (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`, `ClassMask`)
VALUES
    (98000, 524876, 1550, 0, 0, 0, 0, 0, 11, 0, 2048),
    (98000, 704874, 90, 0, 0, 0, 0, 0, 1, 0, 2048),
    (98000, 704886, 90, 0, 0, 0, 0, 0, 1, 0, 2048),
    (98000, 805133, 18390, 0, 0, 0, 0, 0, 51, 0, 2048),
    (98000, 806276, 16510, 0, 0, 0, 0, 0, 48, 0, 2048),
    (98001, 802124, 3220, 0, 0, 0, 0, 0, 18, 0, 4096),
    (98001, 806151, 8630, 0, 0, 0, 0, 0, 33, 0, 4096);

UPDATE `trainer_spell` SET `MoneyCost` = 3220,
    `ReqSkillLine` = 0,
    `ReqSkillRank` = 0,
    `ReqAbility1` = 524876,
    `ReqAbility2` = 0,
    `ReqAbility3` = 0,
    `ReqLevel` = 18
WHERE `TrainerId` = 98000 AND `SpellId` = 804208 AND `ClassMask` = 2048;
