-- Restore equipment metadata for custom Prestige items. Their client Item.dbc
-- rows carry zero inventory types, so the configured DBC enforcement otherwise
-- turns usable armor and weapons into non-equippable items at world startup.
DELETE FROM `item_dbc` WHERE `ID` IN (96010, 96011, 96012, 96013, 96014, 96015, 96016, 96017, 96018, 96019, 96020, 96021, 96101, 96102, 96103, 96104, 96105, 96106, 96107, 96108, 96111, 96112, 96113, 96114, 96115, 96116, 96117, 96118, 96121, 96122, 96123, 96124, 96125, 96126, 96127, 96128, 96131, 96132, 96133, 96134, 96135, 96136, 96137, 96138, 96141, 96142, 96143, 96144, 96145, 96146, 96147, 96148, 96151, 96152, 96153, 96154, 96155, 96156, 96157, 96158, 96161, 96162, 96163, 96164, 96165, 96166, 96167, 96168);
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`)
SELECT `entry`, `class`, `subclass`, `SoundOverrideSubclass`, `Material`, `displayid`, `InventoryType`, `sheath`
FROM `item_template`
WHERE `entry` IN (96010, 96011, 96012, 96013, 96014, 96015, 96016, 96017, 96018, 96019, 96020, 96021, 96101, 96102, 96103, 96104, 96105, 96106, 96107, 96108, 96111, 96112, 96113, 96114, 96115, 96116, 96117, 96118, 96121, 96122, 96123, 96124, 96125, 96126, 96127, 96128, 96131, 96132, 96133, 96134, 96135, 96136, 96137, 96138, 96141, 96142, 96143, 96144, 96145, 96146, 96147, 96148, 96151, 96152, 96153, 96154, 96155, 96156, 96157, 96158, 96161, 96162, 96163, 96164, 96165, 96166, 96167, 96168);
