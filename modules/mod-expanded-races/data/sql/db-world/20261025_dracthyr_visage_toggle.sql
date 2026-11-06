-- Keep existing characters and spell IDs while making Visage a true toggle.
DELETE FROM `spell_script_names`
WHERE `spell_id` IN (363402, 363403) AND `ScriptName` = 'spell_dracthyr_visage_toggle';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (363402, 'spell_dracthyr_visage_toggle'),
    (363403, 'spell_dracthyr_visage_toggle');
DELETE FROM `spell_script_names`
WHERE `spell_id` = 360022 AND `ScriptName` = 'spell_dracthyr_chosen_identity';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (360022, 'spell_dracthyr_chosen_identity');
DELETE FROM `spell_script_names`
WHERE `spell_id` = 372014 AND `ScriptName` = 'spell_dracthyr_visage_regeneration';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (372014, 'spell_dracthyr_visage_regeneration');
