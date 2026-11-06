-- Remove obsolete Talent duplicates and the invalid Placeholder starter spell.
-- Keep 805133 (implemented Entropic Host) and 805674 (active Life Force gate).
DELETE FROM `trainer_spell`
WHERE (`TrainerId` = 98000 AND `SpellId` IN (806276, 704886))
   OR (`TrainerId` = 98001 AND `SpellId` IN (805042, 805386));

DELETE FROM `playercreateinfo_spell_custom`
WHERE `raceMask` = 0 AND `classMask` = 2048 AND `Spell` = 704886;
