-- Connect Cultist and Necromancer summon templates to their registered server AIs.
UPDATE `creature_template`
SET `AIName`='', `ScriptName`='npc_ascension_necromancer'
WHERE `entry` BETWEEN 95000 AND 95003;

UPDATE `creature_template`
SET `AIName`='', `ScriptName`='advai_void_servant'
WHERE `entry` IN (95010,95011);

UPDATE `creature_template_model`
SET `CreatureDisplayID`=200, `VerifiedBuild`=12340
WHERE `CreatureID`=95000 AND `Idx`=0 AND `CreatureDisplayID`=1419;
