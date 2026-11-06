-- Restore the Ancient Curse rank chain so every rank receives the Cultist Insanity grant.
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500712 OR `spell_id` IN (500712, 502152, 502153, 502154, 502155);
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
    (500712, 500712, 1),
    (500712, 502152, 2),
    (500712, 502153, 3),
    (500712, 502154, 4),
    (500712, 502155, 5);

-- Gate Eldritch Blast's Void Rune trigger on its source passive. The triggered spell
-- handler awards the rune once after this parent effect is allowed to execute.
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_ascension_cultist_resource' AND `spell_id` IN(800411,804095);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (800411, 'spell_ascension_cultist_resource'),
    (804095, 'spell_ascension_cultist_resource');
