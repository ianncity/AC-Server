-- Keep cloned Cultist/Necromancer trainer spawns stationary.
UPDATE `creature_template` SET `MovementType` = 0
WHERE `entry` IN (98000, 98001);

UPDATE `creature` SET `MovementType` = 0
WHERE `id` IN (98000, 98001)
  AND `guid` BETWEEN 1 AND 4294967295;
