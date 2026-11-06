-- The item script handles the use; this client-visible spell supplies the use action.
-- MOLL-E's spell incorrectly adds a mailbox description to the tome tooltip.
UPDATE `item_template` SET `spellid_1` = 43932, `description` = 'Unlocks all flight paths.' WHERE `entry` = 96020 AND `ScriptName` = 'item_tome_of_flightpaths';
