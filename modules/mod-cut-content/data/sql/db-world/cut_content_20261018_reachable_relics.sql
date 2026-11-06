-- The source tree does not include map 1807, its area-trigger DBC rows, or the
-- battleground-list DBC rows for IDs 33-36. Remove the unusable runtime records.
DELETE FROM `instance_template` WHERE `map` = 1807;
DELETE FROM `areatrigger_teleport` WHERE `ID` IN (9001, 9002);
DELETE FROM `battleground_template` WHERE `ID` IN (33, 34, 35, 36);
DELETE FROM `creature` WHERE `guid` BETWEEN 900002 AND 900009 AND `map` = 1807;

-- Keep custom items directly obtainable from the existing Deadwind Pass NPC.
UPDATE `creature_template` SET `npcflag` = `npcflag` | 128 WHERE `entry` = 90001;
DELETE FROM `npc_vendor` WHERE `entry` = 90001;
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`) VALUES
(90001, 1, 90101, 0, 0, 0, 12340),
(90001, 2, 90102, 0, 0, 0, 12340),
(90001, 3, 90103, 0, 0, 0, 12340),
(90001, 4, 90104, 0, 0, 0, 12340),
(90001, 5, 90105, 0, 0, 0, 12340),
(90001, 6, 90106, 0, 0, 0, 12340),
(90001, 7, 90201, 0, 0, 0, 12340),
(90001, 8, 90202, 0, 0, 0, 12340),
(90001, 9, 90203, 0, 0, 0, 12340),
(90001, 10, 90204, 0, 0, 0, 12340),
(90001, 11, 90205, 0, 0, 0, 12340),
(90001, 12, 90206, 0, 0, 0, 12340),
(90001, 13, 90207, 0, 0, 0, 12340),
(90001, 14, 90208, 0, 0, 0, 12340),
(90001, 15, 90209, 0, 0, 0, 12340),
(90001, 16, 95104, 0, 0, 0, 12340),
(90001, 17, 95114, 0, 0, 0, 12340),
(90001, 18, 555555, 0, 0, 0, 12340);

-- Keep the unused encounter's loot groups valid if the encounter is restored later.
UPDATE `creature_loot_template`
SET `Chance` = 15
WHERE `Entry` = 90020 AND `Item` IN (90101, 90102, 90103, 90104);

-- This added drop is independent; group 1 already sums to 100% in stock data.
UPDATE `creature_loot_template`
SET `GroupId` = 0
WHERE `Entry` = 4829 AND `Item` = 18878;
