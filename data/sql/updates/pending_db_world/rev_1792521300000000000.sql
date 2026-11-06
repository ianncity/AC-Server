-- Remove Necromancer summon registrations for spell IDs absent from WotLK Spell.dbc.
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_ascension_necromancer_summon' AND `spell_id` IN (500365,600992,805252,899900,899901);
