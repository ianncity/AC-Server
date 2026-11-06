-- Remove orphan spell-rank rows whose first spell IDs are absent from Spell.dbc.
DELETE FROM `spell_ranks`
WHERE (`first_spell_id`,`spell_id`,`rank`) IN ((801693,501157,6),(800926,801152,6));
