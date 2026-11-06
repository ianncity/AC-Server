-- Route the DBC-defined Void Rune generation/consumption effects through the
-- Cultist resource script. The 800431 trigger is shared by passive procs and
-- direct spell effects; 680610 adds two runes and 570725 clears the resource.
DELETE FROM `spell_script_names`
WHERE (`spell_id`, `ScriptName`) IN
    ((680571, 'spell_ascension_cultist_resource'),
     (680610, 'spell_ascension_cultist_resource'),
     (800431, 'spell_ascension_cultist_resource'),
     (800431, 'spell_ascension_cultist_void_rune_trigger'),
     (570725, 'spell_ascension_cultist_resource'));

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (680571, 'spell_ascension_cultist_resource'),
    (680610, 'spell_ascension_cultist_resource'),
    (800431, 'spell_ascension_cultist_void_rune_trigger'),
    (570725, 'spell_ascension_cultist_resource');
