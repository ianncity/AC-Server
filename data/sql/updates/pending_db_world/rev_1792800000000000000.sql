-- The original Ascension race restriction migration omitted Dwarves (race 4).
-- Keep the live WotLK character creation table aligned with CharBaseInfo.dbc.
START TRANSACTION;

DELETE FROM `playercreateinfo_action`
WHERE `race` = 4 AND `class` IN (12, 13);

DELETE FROM `playercreateinfo`
WHERE `race` = 4 AND `class` IN (12, 13);

COMMIT;
