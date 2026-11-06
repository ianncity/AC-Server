-- Bind the reward container to the ItemScript that grants its contents.
UPDATE `item_template`
SET `ScriptName` = 'item_gladiator_spoils'
WHERE `entry` = 98510;
