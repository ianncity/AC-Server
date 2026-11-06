-- Source-backed Cultist/Necromancer talent layout migration.
-- Remove only spells learned by the tracked source-talent grant system and
-- only the specialization bits it added; independently learned spell masks
-- remain intact. The custom talent markers are reset so points are refunded
-- before the new one-point source nodes are loaded.
CREATE TABLE IF NOT EXISTS `character_talent_source_migration_backup` LIKE `character_talent`;
CREATE TABLE IF NOT EXISTS `character_spell_source_migration_backup` LIKE `character_spell`;
CREATE TABLE IF NOT EXISTS `character_class_talent_source_spells_migration_backup` LIKE `character_class_talent_source_spells`;
CREATE TABLE IF NOT EXISTS `character_overranked_talents_source_migration_backup` LIKE `character_overranked_talents`;

INSERT IGNORE INTO `character_talent_source_migration_backup`
SELECT * FROM `character_talent`
WHERE `spell` BETWEEN 995000 AND 995599 OR `spell` BETWEEN 998000 AND 998229;

INSERT IGNORE INTO `character_spell_source_migration_backup`
SELECT DISTINCT spell.*
FROM `character_spell` AS spell
JOIN `character_class_talent_source_spells` AS source
    ON source.`guid` = spell.`guid` AND source.`spell_id` = spell.`spell`;

INSERT IGNORE INTO `character_class_talent_source_spells_migration_backup`
SELECT * FROM `character_class_talent_source_spells`;

INSERT IGNORE INTO `character_overranked_talents_source_migration_backup`
SELECT * FROM `character_overranked_talents`
WHERE `talent_id` BETWEEN 11000 AND 11299;

UPDATE `character_spell` AS spell
JOIN (
    SELECT `guid`, `spell_id`, BIT_OR(1 << `spec`) AS `spec_mask`
    FROM `character_class_talent_source_spells`
    GROUP BY `guid`, `spell_id`
) AS source ON source.`guid` = spell.`guid` AND source.`spell_id` = spell.`spell`
SET spell.`specMask` = spell.`specMask` & (255 ^ source.`spec_mask`)
WHERE (spell.`specMask` & source.`spec_mask`) <> 0;

DELETE spell FROM `character_spell` AS spell
JOIN `character_class_talent_source_spells` AS source
    ON source.`guid` = spell.`guid` AND source.`spell_id` = spell.`spell`
WHERE spell.`specMask` = 0;

DELETE FROM `character_class_talent_source_spells`;
DELETE FROM `character_talent`
WHERE `spell` BETWEEN 995000 AND 995599 OR `spell` BETWEEN 998000 AND 998229;
DELETE FROM `character_overranked_talents` WHERE `talent_id` BETWEEN 11000 AND 11299;
