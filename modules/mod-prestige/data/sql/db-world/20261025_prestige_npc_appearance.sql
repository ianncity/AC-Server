-- Fully specified human and orc NPC appearances, rather than bare player
-- displays with no baked texture or equipment data.
UPDATE `creature_template_model`
SET `CreatureDisplayID` = CASE `CreatureID` WHEN 96000 THEN 5566 WHEN 96002 THEN 4259 END
WHERE `CreatureID` IN (96000, 96002) AND `Idx` = 0;
