-- Register the official Skeletal Archer multicast spell with Necromancer summon handling.
DELETE FROM `spell_script_names` WHERE `spell_id`=500332 AND `ScriptName`='spell_ascension_necromancer_summon';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
(500332,'spell_ascension_necromancer_summon');
