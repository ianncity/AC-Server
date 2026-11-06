-- The top-tier archetype weapons inherited unrelated non-aura DBC spell IDs.
-- Keep their weapon stats and remove the on-equip actions that summon the
-- Headless Horseman or apply a cosmetic combat animation.
UPDATE `item_template` SET `spellid_1` = 0, `spelltrigger_1` = 0
WHERE `entry` IN (95103, 95113) AND `spellid_1` = 42878;

UPDATE `item_template` SET `spellid_1` = 0, `spelltrigger_1` = 0
WHERE `entry` IN (95104, 95114) AND `spellid_1` = 42880;

-- These player gear entries also carried NPC damage spells as on-equip casts.
UPDATE `item_template` SET `spellid_1` = 0, `spelltrigger_1` = 0
WHERE `entry` = 13262 AND `spellid_1` = 23267;

UPDATE `item_template` SET `spellid_1` = 0, `spelltrigger_1` = 0
WHERE `entry` = 17782 AND `spellid_1` = 21987;
