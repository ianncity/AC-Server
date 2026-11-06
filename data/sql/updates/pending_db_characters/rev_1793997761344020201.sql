-- Daily Bloodforged gold cap ledger (PvPBloodforged.DailyGoldCapGold). One row per character per server day.
CREATE TABLE IF NOT EXISTS `character_bloodforged_gold` (
    `guid` INT UNSIGNED NOT NULL,
    `day` INT UNSIGNED NOT NULL,
    `copper` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
