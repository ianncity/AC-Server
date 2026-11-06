-- Items that exist only in the server's Item.dbc file (no item_dbc row) were also reverted to their old display at
-- startup. Add item_dbc rows carrying the corrected display so the core's DBC enforcement agrees with item_template.
REPLACE INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`)
SELECT `entry`, `class`, `subclass`, `SoundOverrideSubclass`, `Material`, `displayid`, `InventoryType`, `sheath`
FROM `item_template`
WHERE `entry` IN (2189, 3246, 7723, 21127, 50909, 50914, 50919, 51069, 51074, 51079, 51626, 51631, 51636);
