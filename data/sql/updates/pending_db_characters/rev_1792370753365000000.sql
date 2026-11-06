-- Restore the configured level-one spell kits to existing Cultist and Necromancer characters.
DELETE FROM `character_spell` WHERE (`spell` IN (500720, 800413) AND `guid` IN (SELECT `guid` FROM `characters` WHERE `class` = 12)) OR (`spell` IN (500970, 804558) AND `guid` IN (SELECT `guid` FROM `characters` WHERE `class` = 13));
INSERT IGNORE INTO `character_spell` (`guid`, `spell`, `specMask`)
SELECT `c`.`guid`, `starter`.`spell`, 255
FROM `characters` `c`
INNER JOIN (
    SELECT 12 AS `class`, 500720 AS `spell`
    UNION ALL SELECT 12, 800413
    UNION ALL SELECT 13, 500970
    UNION ALL SELECT 13, 804558
) `starter` ON `starter`.`class` = `c`.`class`;
