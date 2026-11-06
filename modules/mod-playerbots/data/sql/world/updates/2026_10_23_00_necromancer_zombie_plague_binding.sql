-- Bind Zombie Plague's DBC script effect to the Necromancer runtime handler.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 504022 AND `ScriptName` = 'spell_ascension_necromancer_zombie_plague';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (504022, 'spell_ascension_necromancer_zombie_plague');
