-- ---------------------------------------------------------------------------
-- Character Database Schema for mod-talent-overrank
-- Tracks overranked talents per character GUID and talent spec
-- ---------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `character_overranked_talents` (
    `guid` INT UNSIGNED NOT NULL COMMENT 'Character Global Unique Identifier',
    `talent_id` INT UNSIGNED NOT NULL COMMENT 'Talent ID from Talent.dbc',
    `spec` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Active spec slot (0 = primary, 1 = secondary)',
    `learned_date` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Timestamp when talent was overranked',
    PRIMARY KEY (`guid`, `talent_id`, `spec`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Player talent overranking system progression';
