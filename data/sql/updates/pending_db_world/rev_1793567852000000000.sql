-- Remove script bindings copied from the retired talent spec. Custom talent
-- spells in the active candidate are marker auras; gameplay is implemented by
-- the official source spell granted for each selected talent.
DELETE FROM `spell_script_names` WHERE `spell_id` BETWEEN 998000 AND 998999;
