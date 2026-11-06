-- Cultist Mail proficiency unlock at Level 40 from Class Trainer (matching Shaman)
DELETE FROM `playercreateinfo_spell_custom` WHERE `Spell` = 8737 AND `classmask` = 2048;

UPDATE `trainer_spell`
SET `ReqLevel` = 40, `MoneyCost` = 12000, `ReqSkillLine` = 0, `ReqSkillRank` = 0
WHERE `TrainerId` = 98000 AND `SpellId` = 8737;
