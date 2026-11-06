--
-- Use a valid cloak appearance for the Prestige reward and keep its Item.dbc row aligned.
UPDATE `item_template` SET `displayid` = 61260 WHERE `entry` = 96012 AND `displayid` = 26521;
DELETE FROM `item_dbc` WHERE `ID` = 96012;
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`) VALUES
(96012, 4, 1, -1, 7, 61260, 16, 0);
