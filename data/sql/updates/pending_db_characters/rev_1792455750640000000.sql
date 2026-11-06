-- Install the persistent Prestige rank table used by mod-prestige.
CREATE TABLE IF NOT EXISTS `character_prestige` (
    `guid` INT UNSIGNED NOT NULL COMMENT 'Character Global Unique Identifier',
    `prestige_rank` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Current active prestige rank',
    `total_prestiges` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Total count of prestige resets performed',
    `last_prestige_time` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Timestamp of last prestige reset',
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Player prestige system progression';
