-- Remove obsolete, inactive Talent duplicates from the custom class trainers.
-- Keep 805133 (implemented Entropic Host) and 805674 (active Life Force gate).
DELETE FROM `trainer_spell`
WHERE (`TrainerId` = 98000 AND `SpellId` IN (806276, 704886))
   OR (`TrainerId` = 98001 AND `SpellId` IN (805042, 805386));

-- Ritual Codex is labeled Placeholder in the effective Spell.dbc and must not
-- be taught as a character-creation spell either.
DELETE FROM `playercreateinfo_spell_custom`
WHERE `raceMask` = 0 AND `classMask` = 2048 AND `Spell` = 704886;
