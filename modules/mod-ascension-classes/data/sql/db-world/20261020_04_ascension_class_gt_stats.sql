-- Give the standalone classes their WotLK-native per-class game-table rows.
-- Class 12 (Cultist) inherits Priest (5) curves; class 13 (Necromancer)
-- inherits Warlock (9) curves, matching 20261017_01_ascension_standalone_classes.sql.
-- AzerothCore indexes these tables with GT_MAX_LEVEL=100 and
-- GT_MAX_RATING=32. Missing records made spirit mana regeneration, health
-- regeneration, crit, dodge, and combat-rating scaling return zero/defaults.
START TRANSACTION;

INSERT INTO `gtchancetomeleecrit_dbc` (`ID`, `Data`)
SELECT 1100 + (`ID` - 400), `Data` FROM `gtchancetomeleecrit_dbc`
WHERE `ID` BETWEEN 400 AND 499
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtchancetomeleecrit_dbc` (`ID`, `Data`)
SELECT 1200 + (`ID` - 800), `Data` FROM `gtchancetomeleecrit_dbc`
WHERE `ID` BETWEEN 800 AND 899
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtchancetospellcrit_dbc` (`ID`, `Data`)
SELECT 1100 + (`ID` - 400), `Data` FROM `gtchancetospellcrit_dbc`
WHERE `ID` BETWEEN 400 AND 499
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtchancetospellcrit_dbc` (`ID`, `Data`)
SELECT 1200 + (`ID` - 800), `Data` FROM `gtchancetospellcrit_dbc`
WHERE `ID` BETWEEN 800 AND 899
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtoctregenhp_dbc` (`ID`, `Data`)
SELECT 1100 + (`ID` - 400), `Data` FROM `gtoctregenhp_dbc`
WHERE `ID` BETWEEN 400 AND 499
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtoctregenhp_dbc` (`ID`, `Data`)
SELECT 1200 + (`ID` - 800), `Data` FROM `gtoctregenhp_dbc`
WHERE `ID` BETWEEN 800 AND 899
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtregenhpperspt_dbc` (`ID`, `Data`)
SELECT 1100 + (`ID` - 400), `Data` FROM `gtregenhpperspt_dbc`
WHERE `ID` BETWEEN 400 AND 499
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtregenhpperspt_dbc` (`ID`, `Data`)
SELECT 1200 + (`ID` - 800), `Data` FROM `gtregenhpperspt_dbc`
WHERE `ID` BETWEEN 800 AND 899
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtregenmpperspt_dbc` (`ID`, `Data`)
SELECT 1100 + (`ID` - 400), `Data` FROM `gtregenmpperspt_dbc`
WHERE `ID` BETWEEN 400 AND 499
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtregenmpperspt_dbc` (`ID`, `Data`)
SELECT 1200 + (`ID` - 800), `Data` FROM `gtregenmpperspt_dbc`
WHERE `ID` BETWEEN 800 AND 899
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtchancetomeleecritbase_dbc` (`ID`, `Data`)
SELECT 11, `Data` FROM `gtchancetomeleecritbase_dbc` WHERE `ID` = 4
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtchancetomeleecritbase_dbc` (`ID`, `Data`)
SELECT 12, `Data` FROM `gtchancetomeleecritbase_dbc` WHERE `ID` = 8
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtchancetospellcritbase_dbc` (`ID`, `Data`)
SELECT 11, `Data` FROM `gtchancetospellcritbase_dbc` WHERE `ID` = 4
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtchancetospellcritbase_dbc` (`ID`, `Data`)
SELECT 12, `Data` FROM `gtchancetospellcritbase_dbc` WHERE `ID` = 8
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

INSERT INTO `gtoctclasscombatratingscalar_dbc` (`ID`, `Data`)
SELECT 353 + (`ID` - 129), `Data` FROM `gtoctclasscombatratingscalar_dbc`
WHERE `ID` BETWEEN 129 AND 160
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);
INSERT INTO `gtoctclasscombatratingscalar_dbc` (`ID`, `Data`)
SELECT 385 + (`ID` - 257), `Data` FROM `gtoctclasscombatratingscalar_dbc`
WHERE `ID` BETWEEN 257 AND 288
ON DUPLICATE KEY UPDATE `Data` = VALUES(`Data`);

COMMIT;
