CREATE TABLE IF NOT EXISTS `playerbots_advai_state` (
    `guid` INT UNSIGNED NOT NULL,
    `seed` INT UNSIGNED NOT NULL,
    `state` MEDIUMTEXT NOT NULL,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
