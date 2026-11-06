-- Visage was retired, so the Dracthyr attack scripts that only restricted
-- dragon-form attacks during Visage are no longer registered in the module.
-- Leave Tail Swipe and Wing Buffet to their DBC spell effects and visuals.
DELETE FROM `spell_script_names`
WHERE `spell_id` IN (363405, 363406)
  AND `ScriptName` IN ('spell_dracthyr_tail_swipe', 'spell_dracthyr_wing_buffet');
