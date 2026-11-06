-- Portable Bank Vault: the shop entry promises "Opens your bank from anywhere" but previously only opened
-- the bank once per 120 DP purchase. Make it a reusable soulbound item handled by item_qol_shop.
DELETE FROM `item_template` WHERE `entry` = 98505;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `description`, `ScriptName`) VALUES
(98505, 15, 0, -1, 'Portable Bank Vault', 16028, 4, 64, 1, 0, 0, 0, -1, -1, 80, 1, 1, 1, 1, 54710, 0, 0, 'Opens your bank from anywhere. Not consumed on use.', 'item_qol_shop');
