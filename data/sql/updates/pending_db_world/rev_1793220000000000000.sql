-- Scripted consumables need a valid WotLK item-use spell so the client sends
-- CMSG_USE_ITEM and the server reaches their ItemScript::OnUse handler.
UPDATE `item_template` SET `spellid_1` = 54710, `spelltrigger_1` = 0 WHERE `entry` IN (98500, 98501, 98502, 98510) AND `spellid_1` = 0;

UPDATE `item_template` SET `ScriptName` = 'item_qol_shop' WHERE `entry` IN (98500, 98501, 98502);

-- Match the shop's portable-device metadata to their Item.dbc rows.
UPDATE `item_template` SET `SoundOverrideSubclass` = 0 WHERE `entry` IN (98500, 98501, 98502, 98504) AND `SoundOverrideSubclass` = -1;
UPDATE `item_template` SET `Material` = 0 WHERE `entry` IN (98503, 98504) AND `Material` <> 0;

-- Prestige reward templates are equippable; override their incomplete client
-- Item.dbc rows with the server's actual equipment metadata.
DELETE FROM `item_dbc` WHERE `ID` IN (96016, 96017, 96167, 96168);
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`) VALUES
(96016, 4, 1, -1, 7, 64707, 3, 0),
(96017, 4, 6, -1, 1, 65029, 14, 0),
(96167, 4, 2, -1, 7, 26041, 6, 0),
(96168, 4, 2, -1, 7, 26042, 9, 0);
