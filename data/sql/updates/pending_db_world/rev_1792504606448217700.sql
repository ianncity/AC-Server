-- Bind every Necromancer spell present in the summon handler's table.
DELETE FROM `spell_script_names` WHERE (`spell_id`,`ScriptName`) IN ((706914,'spell_ascension_cultist_worrysome_idol'),(706917,'spell_ascension_cultist_worrysome_idol'),(500365,'spell_ascension_necromancer_summon'),(600992,'spell_ascension_necromancer_summon'),(805252,'spell_ascension_necromancer_summon'),(899900,'spell_ascension_necromancer_summon'),(899901,'spell_ascension_necromancer_summon'));
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(500365, 'spell_ascension_necromancer_summon'),
(600992, 'spell_ascension_necromancer_summon'),
(805252, 'spell_ascension_necromancer_summon'),
(899900, 'spell_ascension_necromancer_summon'),
(899901, 'spell_ascension_necromancer_summon');
