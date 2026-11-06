-- Written in the same character transaction as the one-time initial character save.
CREATE TABLE IF NOT EXISTS `playerbots_advai_initialization` (
    `guid` INT UNSIGNED NOT NULL,
    `seed` INT UNSIGNED NOT NULL,
    `completed_at` BIGINT UNSIGNED NOT NULL,
    `level` TINYINT UNSIGNED NOT NULL,
    `profile` TINYINT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC DEFAULT CHARSET=utf8mb4;
