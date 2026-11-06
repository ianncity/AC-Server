-- Bind every WotLK custom-trigger spell that explicitly changes the Cultist
-- resource. These helpers are referenced by Ascension Spell.dbc but were not
-- attached to the active resource SpellScript, so their 175 effects were inert.
DELETE FROM `spell_script_names`
WHERE `ScriptName` = 'spell_ascension_cultist_resource'
  AND `spell_id` IN (520476, 520720, 520773, 804210, 807852);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (520476, 'spell_ascension_cultist_resource'),
    (520720, 'spell_ascension_cultist_resource'),
    (520773, 'spell_ascension_cultist_resource'),
    (804210, 'spell_ascension_cultist_resource'),
    (807852, 'spell_ascension_cultist_resource');
