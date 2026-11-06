-- Give the Ascension class trainers dedicated, readable gossip instead of
-- reusing Priest and Warlock dialogue and prompts.
DELETE FROM `gossip_menu_option` WHERE `MenuID` IN (98000, 98001);
DELETE FROM `gossip_menu` WHERE `MenuID` IN (98000, 98001);
DELETE FROM `npc_text` WHERE `ID` IN (98000, 98001);
DELETE FROM `conditions`
WHERE `SourceTypeOrReferenceId` IN (14, 15) AND `SourceGroup` IN (98000, 98001);

UPDATE `creature_template` SET `gossip_menu_id`=98000 WHERE `entry`=98000;
UPDATE `creature_template` SET `gossip_menu_id`=98001 WHERE `entry`=98001;

DELETE FROM `npc_text` WHERE `ID` IN (98000, 98001);
INSERT INTO `npc_text` (`ID`,`text0_0`,`text0_1`,`lang0`,`Probability0`)
VALUES (98000,'The old powers answer those willing to pay their price. What would you learn?','The old powers answer those willing to pay their price. What would you learn?',0,1.0),
       (98001,'Death is a discipline, not an end. What would you learn?','Death is a discipline, not an end. What would you learn?',0,1.0);

DELETE FROM `gossip_menu` WHERE `MenuID` IN (98000, 98001);
INSERT INTO `gossip_menu` (`MenuID`,`TextID`) VALUES (98000,98000),(98001,98001);

-- Preserve the standard trainer, talent reset, and dual specialization actions.
DELETE FROM `gossip_menu_option` WHERE `MenuID`=98000;
INSERT INTO `gossip_menu_option`
(`MenuID`,`OptionID`,`OptionIcon`,`OptionText`,`OptionBroadcastTextID`,`OptionType`,`OptionNpcFlag`,`ActionMenuID`,`ActionPoiID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`)
SELECT 98000,`OptionID`,`OptionIcon`,
       CASE `OptionID` WHEN 0 THEN 'I seek training in the Cultist arts.' ELSE `OptionText` END,
       0,`OptionType`,`OptionNpcFlag`,`ActionMenuID`,`ActionPoiID`,`BoxCoded`,`BoxMoney`,`BoxText`,0
FROM `gossip_menu_option` WHERE `MenuID`=4665;

DELETE FROM `gossip_menu_option` WHERE `MenuID`=98001;
INSERT INTO `gossip_menu_option`
(`MenuID`,`OptionID`,`OptionIcon`,`OptionText`,`OptionBroadcastTextID`,`OptionType`,`OptionNpcFlag`,`ActionMenuID`,`ActionPoiID`,`BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`)
SELECT 98001,`OptionID`,`OptionIcon`,
       CASE `OptionID` WHEN 0 THEN 'I seek Necromancer training.' ELSE `OptionText` END,
       0,`OptionType`,`OptionNpcFlag`,`ActionMenuID`,`ActionPoiID`,`BoxCoded`,`BoxMoney`,`BoxText`,0
FROM `gossip_menu_option` WHERE `MenuID`=1503;
