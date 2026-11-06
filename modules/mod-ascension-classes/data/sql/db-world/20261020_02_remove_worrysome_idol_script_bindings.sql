-- These spells have native DBC effects and must use the core's default handlers.
DELETE FROM `spell_script_names`
WHERE `spell_id` IN (706914, 706917)
  AND `ScriptName` = 'spell_ascension_cultist_worrysome_idol';
