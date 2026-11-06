-- Remove retired Prestige Emblems from existing character inventories.
DELETE FROM `character_inventory`
WHERE `item` IN (SELECT `guid` FROM `item_instance` WHERE `itemEntry` = 96001);

DELETE FROM `item_instance` WHERE `itemEntry` = 96001;
