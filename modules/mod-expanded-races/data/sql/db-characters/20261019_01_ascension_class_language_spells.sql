-- Restore the actual language spell as well as skill data for existing
-- Cultist/Necromancer and Dracthyr characters. Skill rows alone do not make
-- the 3.3.5 client able to speak that language.
INSERT INTO `character_spell` (`guid`, `spell`, `specMask`)
SELECT c.`guid`, languages.`spell`, 255
FROM `characters` c
JOIN (
    SELECT 1 AS `race`, 668 AS `spell` UNION ALL
    SELECT 2, 669 UNION ALL
    SELECT 3, 668 UNION ALL SELECT 3, 672 UNION ALL
    SELECT 4, 668 UNION ALL SELECT 4, 671 UNION ALL
    SELECT 5, 669 UNION ALL SELECT 5, 17737 UNION ALL
    SELECT 6, 669 UNION ALL SELECT 6, 670 UNION ALL
    SELECT 7, 668 UNION ALL SELECT 7, 7340 UNION ALL
    SELECT 8, 669 UNION ALL SELECT 8, 7341 UNION ALL
    SELECT 10, 669 UNION ALL SELECT 10, 813 UNION ALL
    SELECT 11, 668 UNION ALL SELECT 11, 29932 UNION ALL
    SELECT 12, 668 UNION ALL SELECT 12, 814 UNION ALL
    SELECT 13, 669 UNION ALL SELECT 13, 814
) languages ON languages.`race` = c.`race`
WHERE c.`class` IN (12, 13) OR c.`race` IN (12, 13)
ON DUPLICATE KEY UPDATE `specMask` = (`specMask` | VALUES(`specMask`));
