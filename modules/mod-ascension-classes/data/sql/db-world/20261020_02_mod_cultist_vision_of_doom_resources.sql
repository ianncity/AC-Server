-- Attach the existing Cultist lifecycle handler to Vision of Doom and its
-- resource aura so the Ascension-defined periodic effects run on WotLK.
DELETE FROM `spell_script_names`
WHERE (`spell_id`, `ScriptName`) IN
    ((500725, 'aura_ascension_cultist_lifecycle'),
     (500732, 'aura_ascension_cultist_lifecycle'));

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (500725, 'aura_ascension_cultist_lifecycle'),
    (500732, 'aura_ascension_cultist_lifecycle');
