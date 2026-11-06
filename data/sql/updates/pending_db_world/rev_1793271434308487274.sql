-- Place the cloned class trainers beside their source, relative to its facing.
-- The original migration added 0.75 to world X irrespective of facing.
UPDATE `creature` c
JOIN `creature` source ON source.`map` = c.`map` AND source.`phaseMask` = c.`phaseMask`
    AND ABS(c.`position_x` - source.`position_x` - 0.75) < 0.01
    AND ABS(c.`position_y` - source.`position_y`) < 0.01
    AND ABS(c.`position_z` - source.`position_z`) < 0.01
JOIN `creature_default_trainer` d ON d.`CreatureId` = source.`id`
JOIN `trainer` t ON t.`Id` = d.`TrainerId` AND t.`Type` = 0
    AND ((c.`id` = 98000 AND t.`Requirement` = 5) OR (c.`id` = 98001 AND t.`Requirement` = 9))
SET c.`position_x` = source.`position_x` - 2.5 * SIN(source.`orientation`),
    c.`position_y` = source.`position_y` + 2.5 * COS(source.`orientation`);

-- Use existing cult and necromancer appearances, with their normal variations.
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (98000, 98001);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
SELECT CASE `CreatureID` WHEN 8547 THEN 98000 ELSE 98001 END,
       `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`
FROM `creature_template_model` WHERE `CreatureID` IN (8547, 8553);
