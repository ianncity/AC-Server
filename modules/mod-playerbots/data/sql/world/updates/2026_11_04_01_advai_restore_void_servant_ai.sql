-- Restore the registered owner-scaling AI after the old core repair cleared this binding.
UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'advai_void_servant'
WHERE `entry` IN (95010, 95011);
