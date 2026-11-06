CREATE TABLE IF NOT EXISTS `character_class_talent_source_spells` (
    `guid` INT UNSIGNED NOT NULL,
    `spec` TINYINT UNSIGNED NOT NULL,
    `spell_id` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `spec`, `spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
