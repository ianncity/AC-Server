-- Use WotLK-native skeleton displays for Necromancer summons. The previous display IDs resolve to later model assets that the 3.3.5 client renders as a checkerboard cube.
START TRANSACTION;

UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 50065 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1245 WHERE `CreatureID` = 50075 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 50076 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 50078 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 50261 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 51065 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 500482 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 1245 WHERE `CreatureID` = 500483 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 200 WHERE `CreatureID` = 500484 AND `Idx` = 0;

COMMIT;
