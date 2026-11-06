-- Keep the official Skeletal Archer summon routed through the Necromancer
-- multicast handler on both clean installs and existing class-module worlds.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 500332
  AND `ScriptName` = 'spell_ascension_necromancer_summon';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
VALUES (500332, 'spell_ascension_necromancer_summon');
