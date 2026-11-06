-- Bind the Necromancer proc passives and the Glacial Impact ability to their
-- runtime handlers. The source DBC proc effects are normalized to dummy auras
-- by the Necromancer spell contracts.
DELETE FROM `spell_script_names`
WHERE (`spell_id`, `ScriptName`) IN
    ((704698, 'aura_ascension_necromancer_event'),
     (704705, 'aura_ascension_necromancer_event'),
     (704689, 'aura_ascension_necromancer_event'),
     (806082, 'aura_ascension_necromancer_event'),
     (801735, 'aura_ascension_necromancer_event'),
     (704355, 'spell_ascension_necromancer_ability'),
     (707911, 'spell_ascension_necromancer_ability'),
     (707399, 'spell_ascension_necromancer_ability'),
     (707400, 'spell_ascension_necromancer_ability'),
     (707401, 'spell_ascension_necromancer_ability'),
     (707402, 'spell_ascension_necromancer_ability'));

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (704698, 'aura_ascension_necromancer_event'),
    (704705, 'aura_ascension_necromancer_event'),
    (704689, 'aura_ascension_necromancer_event'),
    (806082, 'aura_ascension_necromancer_event'),
    (801735, 'aura_ascension_necromancer_event'),
    (704355, 'spell_ascension_necromancer_ability'),
    (707911, 'spell_ascension_necromancer_ability'),
    (707399, 'spell_ascension_necromancer_ability'),
    (707400, 'spell_ascension_necromancer_ability'),
    (707401, 'spell_ascension_necromancer_ability'),
    (707402, 'spell_ascension_necromancer_ability')
ON DUPLICATE KEY UPDATE `ScriptName` = VALUES(`ScriptName`);
