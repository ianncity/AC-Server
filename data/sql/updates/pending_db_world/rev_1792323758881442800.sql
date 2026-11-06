-- Move Bloodforged rewards out of Prestige's 96010-96168 item range.
-- The name guard leaves Prestige's pre-existing 96010 and 96020 entries intact.
DELETE `old`
FROM `item_template` AS `old`
INNER JOIN `item_template` AS `target`
    ON `target`.`entry` = `old`.`entry` + 200
   AND `target`.`name` = `old`.`name`
WHERE `old`.`entry` IN (
    96010, 96020, 96030, 96040, 96050, 96060, 96070, 96080,
    96110, 96120, 96130, 96140, 96150, 96160, 96170, 96180
)
  AND `old`.`name` LIKE 'Bloodforged %';

UPDATE `item_template` SET `entry` = `entry` + 200
WHERE `entry` IN (
    96010, 96020, 96030, 96040, 96050, 96060, 96070, 96080,
    96110, 96120, 96130, 96140, 96150, 96160, 96170, 96180
)
  AND `name` LIKE 'Bloodforged %';
