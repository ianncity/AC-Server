-- Connect class abilities to their registered scripts and remove stale trainer
-- rows that reference spell IDs absent from the WotLK client/server Spell.dbc.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 503740
  AND `ScriptName` = 'aura_ascension_necromancer_ghoul_mastery';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (503740, 'aura_ascension_necromancer_ghoul_mastery')
ON DUPLICATE KEY UPDATE `ScriptName` = VALUES(`ScriptName`);

DELETE FROM `trainer_spell`
WHERE `SpellId` IN (363410, 363411, 363412, 363413, 363414, 363415,
                    363416, 363417, 363418, 363420, 363421);

UPDATE `creature`
SET `equipment_id` = 0
WHERE `id` IN (98000, 98001);
