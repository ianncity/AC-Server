-- Item DBC records required for the Necromancer and Cultist archetype weapons.
DELETE FROM `item_dbc` WHERE `ID` IN (95100, 95101, 95102, 95103, 95104, 95110, 95111, 95112, 95113, 95114);
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`) VALUES
(95100, 2, 6, -1, 1, 33061, 17, 2),
(95101, 2, 6, -1, 1, 45396, 17, 2),
(95102, 2, 6, -1, 1, 50800, 17, 2),
(95103, 2, 6, -1, 1, 50800, 17, 2),
(95104, 2, 6, -1, 1, 50800, 17, 2),
(95110, 2, 7, -1, 1, 33070, 13, 3),
(95111, 2, 7, -1, 1, 33120, 13, 3),
(95112, 2, 7, -1, 1, 33857, 13, 3),
(95113, 2, 7, -1, 1, 42646, 13, 3),
(95114, 2, 7, -1, 1, 42646, 13, 3);
