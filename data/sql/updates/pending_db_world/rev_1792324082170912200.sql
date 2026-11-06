-- Ensure the reward bag is connected to its item script after the module base
-- item SQL has refreshed the template row.
UPDATE `item_template` SET `ScriptName` = 'item_gladiator_spoils'
WHERE `entry` = 98510
  AND `name` = 'Gladiator\'s Spoils of War';
