-- Restore the rank-one spell and its rank-two upgrade on the Necromancer trainer.
DELETE FROM `trainer_spell` WHERE `TrainerId` = 98001 AND `SpellId` IN (704669, 707742);
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`, `ClassMask`) VALUES
(98001, 704669, 10, 0, 0, 0, 0, 0, 1, 0, 4096),
(98001, 707742, 810, 0, 0, 704669, 0, 0, 9, 0, 4096);
