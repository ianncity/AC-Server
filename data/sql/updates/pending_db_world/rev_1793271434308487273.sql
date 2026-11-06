-- Repair item metadata and active feature loot rows after module SQL has loaded.

-- Keep weapon template metadata aligned with the corresponding item DBC records.
UPDATE `item_template` SET `Material` = 1, `sheath` = 2 WHERE `entry` BETWEEN 95100 AND 95104;

UPDATE `item_template` SET `Material` = 1, `sheath` = 3 WHERE `entry` BETWEEN 95110 AND 95114;

-- Portable shop devices are non-equippable misc items.
UPDATE `item_template` SET `Material` = 0 WHERE `entry` IN (98503, 98504);

UPDATE `item_template` SET `SoundOverrideSubclass` = 0 WHERE `entry` = 98504;

-- Restore valid client build item metadata used by the custom reward pools.
UPDATE `item_template` SET `InventoryType` = 21, `displayid` = 14039, `sheath` = 3 WHERE `entry` = 1819;

UPDATE `item_template` SET `displayid` = 17608 WHERE `entry` = 12303;

UPDATE `item_template` SET `subclass` = 8, `displayid` = 23875 WHERE `entry` = 13262;

UPDATE `item_template` SET `displayid` = 17785 WHERE `entry` = 13327;

UPDATE `item_template` SET `Material` = -1, `displayid` = 6494 WHERE `entry` = 17782;

-- Entry 18878 is a dagger in the client Item.dbc; the old custom helm row conflicted with it.
UPDATE `item_template` SET `class` = 2, `subclass` = 15, `Material` = 1, `InventoryType` = 21,
`displayid` = 31337, `sheath` = 3 WHERE `entry` = 18878;

-- Remove the old level-58 custom drop from level-20 Blackfathom Deeps.
DELETE FROM `creature_loot_template` WHERE `Entry` = 4829 AND `Item` = 18878;

-- The Tormentor's group 1 must stay at or below 100% total chance.
UPDATE `creature_loot_template` SET `Chance` = 14 WHERE `Entry` = 90020 AND `Item` = 90104 AND `GroupId` = 1;

-- advai_void_servant is registered by mod-playerbots; retain its owner-scaling ranged AI.
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'advai_void_servant'
WHERE `entry` IN (95010, 95011);
