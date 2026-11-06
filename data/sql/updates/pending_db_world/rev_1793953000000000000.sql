-- The scripted container handles its own contents; use an opening spell instead of MOLL-E.
UPDATE `item_template` SET `spellid_1` = 3366 WHERE `entry` = 98510 AND `ScriptName` = 'item_gladiator_spoils' AND `spellid_1` = 54710;
