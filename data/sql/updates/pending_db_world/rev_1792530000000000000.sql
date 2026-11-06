-- Run Cultist Vision of Doom's official Ascension resource auras through the
-- lifecycle script on WotLK. Spell 500725 supplies Insanity and Void Runes;
-- 500732 supplies the Faceless Destroyer periodic effects.
REPLACE INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (500725, 'aura_ascension_cultist_lifecycle'),
    (500732, 'aura_ascension_cultist_lifecycle');
