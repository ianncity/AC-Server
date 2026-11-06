-- Match live character creation to the Ascension class race eligibility in
-- CharBaseInfo.dbc: no Dwarf, Blood Elf, or Draenei Cultists/Necromancers;
-- Orcs cannot be Cultists.
START TRANSACTION;

DELETE FROM `playercreateinfo_action`
WHERE (`race` = 2 AND `class` = 12)
   OR (`race` IN (3, 10, 11) AND `class` IN (12, 13));

DELETE FROM `playercreateinfo`
WHERE (`race` = 2 AND `class` = 12)
   OR (`race` IN (3, 10, 11) AND `class` IN (12, 13));

COMMIT;
