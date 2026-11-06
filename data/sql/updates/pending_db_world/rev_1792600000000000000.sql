-- Add the missing Prestige rank-one flight path tome and bind its use script.
REPLACE INTO `item_template`
    (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `BuyPrice`,
     `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,
     `maxcount`, `stackable`, `bonding`, `spellid_1`, `spelltrigger_1`, `description`, `ScriptName`)
VALUES
    (96020, 12, 0, 'Tome of Flight Paths', 4501, 3, 64, 1, 0, 0, 0, -1, -1, 80, 1, 1, 1, 1, 54710, 0,
     'Unlocks all known flight paths.', 'item_tome_of_flightpaths');
