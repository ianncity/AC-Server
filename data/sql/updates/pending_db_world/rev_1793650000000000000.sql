DELETE FROM `spell_script_names`
WHERE `spell_id` = 805133
  AND `ScriptName` = 'aura_ascension_cultist_entropic_host';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (805133, 'aura_ascension_cultist_entropic_host');
