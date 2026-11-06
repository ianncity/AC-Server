-- Source-verified Ascension talent mechanics: Plague Doctor.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 803087 AND `ScriptName` = 'aura_ascension_necromancer_plague_doctor';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (803087, 'aura_ascension_necromancer_plague_doctor');
