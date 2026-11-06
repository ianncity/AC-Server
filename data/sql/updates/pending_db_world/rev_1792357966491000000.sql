-- Replace obsolete prestige token purchases with rank-up rewards and Horde NPC presentation.
DELETE FROM `npc_vendor` WHERE `entry` IN (96000, 96002);
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 96000;
DELETE FROM `gossip_menu` WHERE `MenuID` = 96000;
DELETE FROM `npc_text` WHERE `ID` = 96000;
UPDATE `creature_template` SET `npcflag` = 1, `gossip_menu_id` = 96000, `ScriptName` = 'npc_prestige_master' WHERE `entry` IN (96000, 96002);
UPDATE `creature_template` SET `faction` = 83 WHERE `entry` = 96002;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 49, `DisplayScale` = 1.0, `Probability` = 1.0, `VerifiedBuild` = 12340 WHERE `CreatureID` = 96000 AND `Idx` = 0;
UPDATE `creature_template_model` SET `CreatureDisplayID` = 51, `DisplayScale` = 1.0, `Probability` = 1.0, `VerifiedBuild` = 12340 WHERE `CreatureID` = 96002 AND `Idx` = 0;
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (96000, 96002);
INSERT INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES (96000,0,49,1.0,1.0,12340),(96002,0,51,1.0,1.0,12340) ON DUPLICATE KEY UPDATE `CreatureDisplayID`=VALUES(`CreatureDisplayID`),`DisplayScale`=VALUES(`DisplayScale`),`Probability`=VALUES(`Probability`),`VerifiedBuild`=VALUES(`VerifiedBuild`);

-- Install portable versions of the stock mailbox and field repair bot, retaining their native summon spells.
UPDATE `item_template` SET `displayid`=15715 WHERE `entry`=98502;
CREATE TEMPORARY TABLE `qol_shop_portable_item` LIKE `item_template`;
REPLACE INTO `qol_shop_portable_item` SELECT * FROM `item_template` WHERE `entry` = 40768;
UPDATE `qol_shop_portable_item` SET `entry`=98503,`name`='Portable MOLL-E',`RequiredSkill`=0,`RequiredSkillRank`=0,`RequiredLevel`=1,`maxcount`=1,`bonding`=1,`description`='Summons MOLL-E, a portable mailbox. Engineering is not required.',`ScriptName`='';
REPLACE INTO `item_template` SELECT * FROM `qol_shop_portable_item`;
DELETE FROM `qol_shop_portable_item`;
REPLACE INTO `qol_shop_portable_item` SELECT * FROM `item_template` WHERE `entry` = 34113;
UPDATE `qol_shop_portable_item` SET `entry`=98504,`name`='Portable Field Repair Bot',`RequiredSkill`=0,`RequiredSkillRank`=0,`RequiredLevel`=1,`maxcount`=1,`bonding`=1,`description`='Summons a repair bot. Engineering is not required.',`ScriptName`='';
REPLACE INTO `item_template` SELECT * FROM `qol_shop_portable_item`;
DROP TEMPORARY TABLE `qol_shop_portable_item`;
