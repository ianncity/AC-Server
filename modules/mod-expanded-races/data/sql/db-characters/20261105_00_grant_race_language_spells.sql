-- The 3.3.5 client treats a language as known only if the character has its "Language X" spell. Most characters (bots,
-- characters moved between accounts) lack them, which garbles same-faction chat and blocks speaking. Grant each
-- character the language spells of its race. Idempotent; the login hook in mod-expanded-races keeps them granted.
INSERT IGNORE INTO `character_spell` (`guid`, `spell`, `specMask`)
SELECT c.`guid`, l.`spell`, 255
FROM `characters` c
JOIN (
    SELECT 1 AS race, 668 AS spell UNION ALL SELECT 2, 669 UNION ALL SELECT 3, 668 UNION ALL SELECT 3, 672 UNION ALL
    SELECT 4, 668 UNION ALL SELECT 4, 671 UNION ALL SELECT 5, 669 UNION ALL SELECT 5, 17737 UNION ALL
    SELECT 6, 669 UNION ALL SELECT 6, 670 UNION ALL SELECT 7, 668 UNION ALL SELECT 7, 7340 UNION ALL
    SELECT 8, 669 UNION ALL SELECT 8, 7341 UNION ALL SELECT 10, 669 UNION ALL SELECT 10, 813 UNION ALL
    SELECT 11, 668 UNION ALL SELECT 11, 29932 UNION ALL SELECT 12, 668 UNION ALL SELECT 13, 669
) l ON l.`race` = c.`race`;
