-- The custom trainer loader reads ClassMask from trainer_spell, including on
-- databases created from the upstream base schema where this column is absent.
-- Keep the migration safe for worlds where the column was added manually.
SET @trainer_spell_classmask_ddl = IF(
    EXISTS (
        SELECT 1
        FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = 'trainer_spell'
          AND COLUMN_NAME = 'ClassMask'
    ),
    'SELECT 1',
    'ALTER TABLE `trainer_spell` ADD COLUMN `ClassMask` INT UNSIGNED NOT NULL DEFAULT 0'
);
PREPARE trainer_spell_classmask_stmt FROM @trainer_spell_classmask_ddl;
EXECUTE trainer_spell_classmask_stmt;
DEALLOCATE PREPARE trainer_spell_classmask_stmt;
