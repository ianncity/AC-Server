-- Restore Zombie Plague's DBC-linked Gutwrencher damage and runic power effect.
DELETE FROM `spell_script_names` WHERE `spell_id` = 504022 AND `ScriptName` = 'spell_ascension_necromancer_zombie_plague';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (504022, 'spell_ascension_necromancer_zombie_plague');
