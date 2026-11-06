-- Enable the Insanity-gated critical-hit shield event and its lifecycle aura.
DELETE FROM `spell_script_names` WHERE (`spell_id` = 255281 AND `ScriptName` = 'aura_ascension_cultist_lifecycle') OR (`spell_id` = 255283 AND `ScriptName` = 'aura_ascension_cultist_event');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(255281, 'aura_ascension_cultist_lifecycle'),
(255283, 'aura_ascension_cultist_event');

DELETE FROM `spell_proc` WHERE `SpellId` = 255283;
INSERT INTO `spell_proc`
    (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,
     `ProcFlags`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,
     `ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`)
VALUES (255283,0,0,0,0,0,1048575,7,2,12287,2,0,0,100,0,0);
