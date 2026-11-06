-- Restrict Dracthyr dragon-form racial attacks while in Visage and retain
-- their server scripts for characters created before this update.
DELETE FROM `spell_script_names`
WHERE `spell_id` IN (363405, 363406)
  AND `ScriptName` IN ('spell_dracthyr_tail_swipe', 'spell_dracthyr_wing_buffet');

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
    (363405, 'spell_dracthyr_tail_swipe'),
    (363406, 'spell_dracthyr_wing_buffet');
