-- 20261103_02_class_armor_proficiencies_and_gear.sql
-- Rule:
-- 1. Necromancer (Class 13, Mask 4096): Cloth armor only.
--    - Starting skills: Cloth (415). No Leather, Mail, Plate, or Shields.
--    - Starting spells: Cloth Armor (9078).
--    - Gear: All Mage gear is usable by Necromancer (AllowableClass |= 4096).
-- 2. Cultist (Class 12, Mask 2048): Cloth through Mail (Cloth, Leather, Mail).
--    - Starting skills: Cloth (415), Leather (414), Mail (413). No Plate.
--    - Starting spells: Cloth Armor (9078), Leather Armor (9077), Mail Armor (8737).
--    - Gear: All Mage, Hunter, and Shaman gear is usable by Cultist (AllowableClass |= 2048).

-- Step 1: Cultist Skills in playercreateinfo_skills
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`) VALUES
(0, 2048, 415, 0, 'Cultist Cloth Armor'),
(0, 2048, 414, 0, 'Cultist Leather Armor'),
(0, 2048, 413, 0, 'Cultist Mail Armor'),
(2048, 2048, 415, 0, 'Cultist Cloth Armor'),
(2048, 2048, 414, 0, 'Cultist Leather Armor'),
(2048, 2048, 413, 0, 'Cultist Mail Armor'),
(4096, 2048, 415, 0, 'Cultist Cloth Armor'),
(4096, 2048, 414, 0, 'Cultist Leather Armor'),
(4096, 2048, 413, 0, 'Cultist Mail Armor')
ON DUPLICATE KEY UPDATE `comment` = VALUES(`comment`);

DELETE FROM `playercreateinfo_skills` WHERE `classMask` = 2048 AND `skill` = 293;

-- Step 2: Necromancer Skills in playercreateinfo_skills
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`) VALUES
(0, 4096, 415, 0, 'Necromancer Cloth Armor'),
(2048, 4096, 415, 0, 'Necromancer Cloth Armor'),
(4096, 4096, 415, 0, 'Necromancer Cloth Armor')
ON DUPLICATE KEY UPDATE `comment` = VALUES(`comment`);

DELETE FROM `playercreateinfo_skills` WHERE `classMask` = 4096 AND `skill` IN (414, 413, 293, 433);

-- Step 3: Starting Spells in playercreateinfo_spell_custom
INSERT IGNORE INTO `playercreateinfo_spell_custom` (`raceMask`, `classMask`, `Spell`, `Note`) VALUES
(0, 2048, 9078, 'Cultist Cloth Armor'),
(0, 2048, 9077, 'Cultist Leather Armor'),
(0, 2048, 8737, 'Cultist Mail Armor'),
(0, 4096, 9078, 'Necromancer Cloth Armor');

DELETE FROM `playercreateinfo_spell_custom` WHERE `classMask` = 2048 AND `Spell` = 750;
DELETE FROM `playercreateinfo_spell_custom` WHERE `classMask` = 4096 AND `Spell` IN (9077, 8737, 750, 9116);

-- Step 4: AllowableClass in item_template
-- Mage gear -> Necromancer (excluding leather, mail, plate, shields, relics, and non-caster weapons)
UPDATE `item_template`
SET `AllowableClass` = `AllowableClass` | 4096
WHERE `AllowableClass` > 0
  AND (`AllowableClass` & 128) != 0
  AND (`AllowableClass` & 4096) = 0
  AND NOT (`class` = 4 AND `subclass` IN (2, 3, 4, 6, 7, 8, 9, 10))
  AND NOT (`class` = 2 AND `subclass` IN (0, 1, 2, 3, 4, 5, 8, 13));

-- Mage, Hunter, Shaman gear -> Cultist (excluding plate and relics)
UPDATE `item_template`
SET `AllowableClass` = `AllowableClass` | 2048
WHERE `AllowableClass` > 0
  AND (`AllowableClass` & (128 | 4 | 64)) != 0
  AND (`AllowableClass` & 2048) = 0
  AND NOT (`class` = 4 AND `subclass` IN (4, 7, 8, 9, 10));
