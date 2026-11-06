-- Keep existing Bloodforged weapon instances and unlocked appearances when
-- their world item templates are moved to the collision-free item range.
UPDATE `item_instance` SET `itemEntry` = `itemEntry` + 200
WHERE `itemEntry` IN (
    96030, 96040, 96050, 96060, 96070, 96080,
    96110, 96120, 96130, 96140, 96150, 96160, 96170, 96180
);

UPDATE `custom_unlocked_appearances` SET `item_template_id` = `item_template_id` + 200
WHERE `item_template_id` IN (
    96030, 96040, 96050, 96060, 96070, 96080,
    96110, 96120, 96130, 96140, 96150, 96160, 96170, 96180
);
