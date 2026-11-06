-- Remove script bindings for class shield spells that are absent from Spell.dbc.
DELETE FROM `spell_script_names`
WHERE (`spell_id` = 255281 AND `ScriptName` = 'aura_ascension_cultist_lifecycle')
   OR (`spell_id` = 255283 AND `ScriptName` = 'aura_ascension_cultist_event');

DELETE FROM `spell_proc` WHERE `SpellId` = 255283;
