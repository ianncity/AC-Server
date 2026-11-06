-- These scripted items use their spells only to trigger the item-use handler.
-- MOLL-E's spell advertised a mailbox on unrelated shop rewards.
UPDATE `item_template` SET `spellid_1` = 483 WHERE `entry` IN (98500, 98501) AND `ScriptName` = 'item_qol_shop' AND `spellid_1` = 54710;
UPDATE `item_template` SET `spellid_1` = 25696 WHERE `entry` = 98502 AND `ScriptName` = 'item_qol_shop' AND `spellid_1` = 54710;
UPDATE `item_template` SET `spellid_1` = 67368 WHERE `entry` = 98505 AND `ScriptName` = 'item_qol_shop' AND `spellid_1` = 54710;
