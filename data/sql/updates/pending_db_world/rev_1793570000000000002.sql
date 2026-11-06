-- Bind Crypt Plague to the Necromancer combat event handler.
DELETE FROM `spell_script_names` WHERE `spell_id` = 500117 AND `ScriptName` = 'aura_ascension_necromancer_event';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(500117, 'aura_ascension_necromancer_event');
