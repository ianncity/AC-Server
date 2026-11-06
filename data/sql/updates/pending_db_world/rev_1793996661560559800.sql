-- Corrective drift fixes for the item modules. The live realm already has these values
-- (applied 2026-11-06 or earlier); this file makes fresh installs match.
-- Verified against the client: displays 32235, 33086, 39287, 39425, 42655, 46297, 28586,
-- 16538, 20218, 43090, 60840 and the Bloodforged displays below resolve icon/model/texture.

-- Prestige Helm of the Eternal Paragon: 31393 had an empty icon row; use the sibling Crown of the Ascended display.
UPDATE `item_template` SET `displayid` = 32235 WHERE `entry` = 96014;
UPDATE `item_dbc` SET `DisplayInfoID` = 32235 WHERE `ID` = 96014;

-- Bloodforged Vanguard Blade: ItemLevel 200 was out of line with its stats (see item-modules audit); stat ladder fits ~96.
UPDATE `item_template` SET `ItemLevel` = 96 WHERE `entry` = 96280;

-- Archetype weapons: Necromancer scythes (4096) and Cultist godblades (2048) only.
UPDATE `item_template` SET `AllowableClass` = 4096 WHERE `entry` BETWEEN 95100 AND 95104;
UPDATE `item_template` SET `AllowableClass` = 2048 WHERE `entry` BETWEEN 95110 AND 95114;

-- Archetype display IDs as live (item_dbc already carries these).
UPDATE `item_template` SET `displayid` = 33086 WHERE `entry` = 95100;
UPDATE `item_template` SET `displayid` = 39287 WHERE `entry` = 95101;
UPDATE `item_template` SET `displayid` = 39425 WHERE `entry` = 95102;
UPDATE `item_template` SET `displayid` = 42655 WHERE `entry` = 95103;
UPDATE `item_template` SET `displayid` = 46297 WHERE `entry` = 95104;
UPDATE `item_template` SET `displayid` = 28586 WHERE `entry` = 95110;
UPDATE `item_template` SET `displayid` = 16538 WHERE `entry` = 95111;
UPDATE `item_template` SET `displayid` = 20218 WHERE `entry` = 95112;
UPDATE `item_template` SET `displayid` = 43090 WHERE `entry` = 95113;
UPDATE `item_template` SET `displayid` = 60840 WHERE `entry` = 95114;

-- Bloodforged displays as live (the 20261105 fixes; bloodforged_items.sql reintroduces the broken ones).
UPDATE `item_template` SET `displayid` = 28586 WHERE `entry` = 96210;
UPDATE `item_template` SET `displayid` = 16538 WHERE `entry` = 96220;
UPDATE `item_template` SET `displayid` = 20010 WHERE `entry` = 96230;
UPDATE `item_template` SET `displayid` = 20035 WHERE `entry` = 96240;
UPDATE `item_template` SET `displayid` = 19997 WHERE `entry` = 96250;
UPDATE `item_template` SET `displayid` = 32197 WHERE `entry` = 96260;
UPDATE `item_template` SET `displayid` = 39374 WHERE `entry` = 96270;
UPDATE `item_template` SET `displayid` = 20218 WHERE `entry` = 96280;
UPDATE `item_template` SET `displayid` = 16539 WHERE `entry` = 96310;
UPDATE `item_template` SET `displayid` = 13488 WHERE `entry` = 96320;
UPDATE `item_template` SET `displayid` = 20218 WHERE `entry` = 96330;
UPDATE `item_template` SET `displayid` = 26477 WHERE `entry` = 96340;
UPDATE `item_template` SET `displayid` = 32356 WHERE `entry` = 96350;
UPDATE `item_template` SET `displayid` = 31966 WHERE `entry` = 96360;
UPDATE `item_template` SET `displayid` = 36378 WHERE `entry` = 96370;
UPDATE `item_template` SET `displayid` = 60617 WHERE `entry` = 96380;

-- Landslide (12203) drops Fist of Stone (17733). The earlier sources file inserted 17745 (Noxious Shooter, Noxxion's drop).
DELETE FROM `creature_loot_template` WHERE `Entry` = 12203 AND `Item` IN (17733, 17745);
INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
(12203, 17733, 25.0, 0, 1, 0, 1, 1, 'Landslide - Fist of Stone');
