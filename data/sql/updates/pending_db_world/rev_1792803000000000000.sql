-- Restore validated build-12340 item fields for custom and prestige rewards.
START TRANSACTION;

UPDATE `item_template` SET `InventoryType` = 21, `displayid` = 14039, `sheath` = 3
WHERE `entry` = 1819 AND `InventoryType` = 13 AND `displayid` = 1395 AND `sheath` = 2;
UPDATE `item_template` SET `displayid` = 17608 WHERE `entry` = 12303 AND `displayid` = 22444;
UPDATE `item_template` SET `subclass` = 8, `displayid` = 23875
WHERE `entry` = 13262 AND `subclass` = 7 AND `displayid` = 24550;
UPDATE `item_template` SET `displayid` = 17785 WHERE `entry` = 13327 AND `displayid` = 23992;
UPDATE `item_template` SET `Material` = -1, `displayid` = 6494
WHERE `entry` = 17782 AND `Material` = 2 AND `displayid` = 30043;
UPDATE `item_template` SET `class` = 2, `subclass` = 15, `Material` = 1,
`InventoryType` = 21, `displayid` = 31337, `sheath` = 3
WHERE `entry` = 18878 AND `class` = 4 AND `subclass` = 1 AND `Material` = 7 AND `InventoryType` = 1
AND `displayid` = 31393 AND `sheath` = 0;

COMMIT;
