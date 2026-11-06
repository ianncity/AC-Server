-- Historical contacts outlive the bounded resident relationship cache.
CREATE TABLE IF NOT EXISTS `playerbots_advai_relationship` (
    `guid` INT UNSIGNED NOT NULL,
    `peer` INT UNSIGNED NOT NULL,
    `affinity` SMALLINT NOT NULL DEFAULT 0,
    `encounters` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_event` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `last_reward` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `revision` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `peer`)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC DEFAULT CHARSET=utf8mb4;
