-- Playtime reward balances and persistent XP draught time belong to characters.
CREATE TABLE IF NOT EXISTS `character_dp` (
    `guid` INT UNSIGNED NOT NULL PRIMARY KEY,
    `spent_dp` INT UNSIGNED NOT NULL DEFAULT 0,
    `xpdraught_expires` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `last_update` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SET @qol_xpdraught_column_exists = (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE `TABLE_SCHEMA` = DATABASE()
      AND `TABLE_NAME` = 'character_dp'
      AND `COLUMN_NAME` = 'xpdraught_expires'
);
SET @qol_xpdraught_ddl = IF(
    @qol_xpdraught_column_exists = 0,
    'ALTER TABLE `character_dp` ADD COLUMN `xpdraught_expires` BIGINT UNSIGNED NOT NULL DEFAULT 0',
    'SELECT 1'
);
PREPARE qol_xpdraught_statement FROM @qol_xpdraught_ddl;
EXECUTE qol_xpdraught_statement;
DEALLOCATE PREPARE qol_xpdraught_statement;
