-- Monotonic revisions prevent older asynchronous saves from replacing newer character memory.
SET @advai_revision_ddl = IF(
    EXISTS(SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'playerbots_advai_state' AND `COLUMN_NAME` = 'revision'),
    'DO 0',
    'ALTER TABLE `playerbots_advai_state` ADD COLUMN `revision` BIGINT UNSIGNED NOT NULL DEFAULT 0');
PREPARE advai_revision_statement FROM @advai_revision_ddl;
EXECUTE advai_revision_statement;
DEALLOCATE PREPARE advai_revision_statement;

SET @advai_revision_ddl = IF(
    EXISTS(SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'playerbots_advai_cognition' AND `COLUMN_NAME` = 'revision'),
    'DO 0',
    'ALTER TABLE `playerbots_advai_cognition` ADD COLUMN `revision` BIGINT UNSIGNED NOT NULL DEFAULT 0');
PREPARE advai_revision_statement FROM @advai_revision_ddl;
EXECUTE advai_revision_statement;
DEALLOCATE PREPARE advai_revision_statement;
SET @advai_revision_ddl = NULL;
