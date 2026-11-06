-- Retire Dracthyr Visage transformation and passive scripts.
DELETE FROM `spell_script_names`
WHERE (`spell_id` IN (363402, 363403) AND `ScriptName` = 'spell_dracthyr_visage_toggle')
   OR (`spell_id` = 372014 AND `ScriptName` = 'spell_dracthyr_visage_regeneration');
