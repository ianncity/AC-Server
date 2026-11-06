-- Item use-spell corrections (2026-11-06). Script-handled items (OnItemUse returns true) never cast their
-- spell, so these only fix the stored value. Live already has these values.
--   98500/98501 Tomes and 98502 Draught: handled by item_qol_shop; old spells were 483 (Learning) and 25696 (Drink).
--   98505 Portable Bank Vault: handled by item_qol_shop; old spell 67368 (Bank Errand) was an unrelated self aura.
--   98510 Gladiator's Spoils of War: handled by item_gladiator_spoils; old spell 3366 (Opening, open-lock) was unrelated.
-- 98503 (MOLL-E, spell 54710 -> summons gameobject 191605) and 98504 (Field Repair Bot, spell 44389 -> summons creature
-- 24780) are correct and are not touched.
UPDATE `item_template` SET `spellid_1` = 0 WHERE `entry` IN (98500, 98501, 98502, 98505, 98510);

-- Prestige Master of Prestige (96000 Alliance, 96002 Horde): the live dressed NPC displays (Archmage Malin 2968,
-- Overlord Runthak 14429), as in prestige_master_display_fix.sql. The cleanup file's 5566/4259 is superseded.
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (96000, 96002) AND `Idx` = 0;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(96000, 0, 2968, 1, 1, 12340),
(96002, 0, 14429, 1, 1, 12340);
