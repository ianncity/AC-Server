-- Repair existing Cultist/Necromancer characters and all Dracthyr characters.
INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`)
SELECT c.`guid`, languages.`skill`, 300, 300
FROM `characters` c
JOIN (
    SELECT 1 AS `race`, 98 AS `skill` UNION ALL
    SELECT 2, 109 UNION ALL
    SELECT 3, 98 UNION ALL SELECT 3, 111 UNION ALL
    SELECT 4, 98 UNION ALL SELECT 4, 113 UNION ALL
    SELECT 5, 109 UNION ALL SELECT 5, 673 UNION ALL
    SELECT 6, 109 UNION ALL SELECT 6, 115 UNION ALL
    SELECT 7, 98 UNION ALL SELECT 7, 313 UNION ALL
    SELECT 8, 109 UNION ALL SELECT 8, 315 UNION ALL
    SELECT 10, 109 UNION ALL SELECT 10, 137 UNION ALL
    SELECT 11, 98 UNION ALL SELECT 11, 759 UNION ALL
    SELECT 12, 98 UNION ALL SELECT 12, 138 UNION ALL
    SELECT 13, 109 UNION ALL SELECT 13, 138
) languages ON languages.`race` = c.`race`
WHERE c.`class` IN (12, 13) OR c.`race` IN (12, 13)
ON DUPLICATE KEY UPDATE
    `value` = GREATEST(`value`, VALUES(`value`)),
    `max` = GREATEST(`max`, VALUES(`max`));
