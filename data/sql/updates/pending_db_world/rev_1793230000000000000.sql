-- Remove custom runtime records for map 1807, which has no WotLK map data.
DELETE FROM `instance_template` WHERE `map` = 1807;
DELETE FROM `areatrigger_teleport` WHERE `ID` IN (9001, 9002);
DELETE FROM `battleground_template` WHERE `ID` IN (33, 34, 35, 36);
DELETE FROM `creature`
WHERE `id` IN (90002, 90010, 90011, 90012, 90020)
  AND `guid` BETWEEN 900002 AND 900009
  AND `map` = 1807;
