-- Remove Plate Mail proficiency from Cultist (98000) and Necromancer (98001) class trainers
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (98000, 98001) AND `SpellId` IN (750, 680750, 705750);
