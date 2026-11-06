-- Add Item.dbc records for custom items that are skipped by the item loader without a matching DBC row.

-- Use an existing cloak display because ItemDisplayInfo 26521 is absent from the active client and server DBCs.
UPDATE `item_template` SET `displayid` = 24175
WHERE `entry` = 90101 AND `displayid` = 26521;

-- Match Bloodforged one-hand swords to the active Item.dbc material and sheath conventions.
UPDATE `item_template` SET `Material` = 1, `sheath` = 3
WHERE `entry` IN (96210, 96220, 96230, 96240, 96250, 96260, 96270, 96280,
                  96310, 96320, 96330, 96340, 96350, 96360, 96370, 96380);

-- Keep the server item templates aligned with the custom Item.dbc display for these swords.
UPDATE `item_template` SET `displayid` = 15286
WHERE `entry` IN (96310, 96320, 96330, 96340, 96350, 96360, 96370, 96380)
  AND `displayid` = 45396;

DELETE FROM `item_dbc` WHERE `ID` IN (90101, 90102, 90103, 90104, 90105, 90106, 90201, 90202, 90203, 90204, 90205, 90206, 90207, 90208, 90209, 322691, 96210, 96220, 96230, 96240, 96250, 96260, 96270, 96280, 96310, 96320, 96330, 96340, 96350, 96360, 96370, 96380, 98510);
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`) VALUES
(90101, 4, 0, -1, 7, 24175, 16, 0),
(90102, 2, 6, -1, 1, 28014, 17, 1),
(90103, 2, 5, -1, 1, 29112, 17, 1),
(90104, 4, 1, -1, 7, 27015, 20, 0),
(90105, 4, 0, -1, 2, 18880, 11, 0),
(90106, 4, 0, -1, 2, 23214, 12, 0),
(90201, 4, 7, -1, 8, 29000, 28, 0),
(90202, 4, 7, -1, 8, 29001, 28, 0),
(90203, 4, 9, -1, 8, 29010, 28, 0),
(90204, 4, 9, -1, 8, 29011, 28, 0),
(90205, 4, 8, -1, 8, 29020, 28, 0),
(90206, 4, 8, -1, 8, 29021, 28, 0),
(90207, 4, 10, -1, 8, 29030, 28, 0),
(90208, 4, 10, -1, 8, 29035, 28, 0),
(90209, 4, 10, -1, 8, 29040, 28, 0),
(322691, 2, 7, -1, 1, 28308, 17, 1),
(96210, 2, 7, -1, 1, 15286, 13, 3),
(96220, 2, 7, -1, 1, 15286, 13, 3),
(96230, 2, 7, -1, 1, 15286, 13, 3),
(96240, 2, 7, -1, 1, 15286, 13, 3),
(96250, 2, 7, -1, 1, 15286, 13, 3),
(96260, 2, 7, -1, 1, 15286, 13, 3),
(96270, 2, 7, -1, 1, 15286, 13, 3),
(96280, 2, 7, -1, 1, 15286, 13, 3),
(96310, 2, 7, -1, 1, 15286, 13, 3),
(96320, 2, 7, -1, 1, 15286, 13, 3),
(96330, 2, 7, -1, 1, 15286, 13, 3),
(96340, 2, 7, -1, 1, 15286, 13, 3),
(96350, 2, 7, -1, 1, 15286, 13, 3),
(96360, 2, 7, -1, 1, 15286, 13, 3),
(96370, 2, 7, -1, 1, 15286, 13, 3),
(96380, 2, 7, -1, 1, 15286, 13, 3),
(98510, 15, 0, -1, 0, 1184, 0, 0);
