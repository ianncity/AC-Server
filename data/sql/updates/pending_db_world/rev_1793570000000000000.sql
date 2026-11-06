-- Route source talent event auras through their class damage handlers.
DELETE FROM `spell_script_names` WHERE (`spell_id` = 300276 AND `ScriptName` = 'aura_ascension_cultist_event') OR (`spell_id` = 681087 AND `ScriptName` = 'aura_ascension_cultist_event') OR (`spell_id` = 704883 AND `ScriptName` = 'aura_ascension_cultist_event') OR (`spell_id` = 500119 AND `ScriptName` = 'aura_ascension_necromancer_event');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(300276, 'aura_ascension_cultist_event'),
(681087, 'aura_ascension_cultist_event'),
(704883, 'aura_ascension_cultist_event'),
(500119, 'aura_ascension_necromancer_event');
