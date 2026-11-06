-- Existing Dracthyr characters need the same language skills as newly created ones.
INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`)
SELECT `guid`, IF(`race` = 12, 98, 109), 300, 300
FROM `characters`
WHERE `race` IN (12, 13)
ON DUPLICATE KEY UPDATE
    `value` = GREATEST(`value`, VALUES(`value`)),
    `max` = GREATEST(`max`, VALUES(`max`));

INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`)
SELECT `guid`, 138, 300, 300
FROM `characters`
WHERE `race` IN (12, 13)
ON DUPLICATE KEY UPDATE
    `value` = GREATEST(`value`, VALUES(`value`)),
    `max` = GREATEST(`max`, VALUES(`max`));
