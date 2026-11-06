-- Complete the native spell dependency for Necromancer Rime's Frozen Reliquary.
-- Item.dbc in the client patch maps item 555555 to ItemDisplayInfo 167973.
DELIMITER $$
DROP PROCEDURE IF EXISTS `advai_assert_frozen_reliquary`$$
CREATE PROCEDURE `advai_assert_frozen_reliquary`() BEGIN
IF EXISTS (SELECT 1 FROM `item_template` WHERE `entry`=555555 AND
 `name`<>'Frozen Reliquary') THEN
SIGNAL SQLSTATE '45000';
END IF; END$$
CALL `advai_assert_frozen_reliquary`()$$
DROP PROCEDURE `advai_assert_frozen_reliquary`$$
DELIMITER ;
START TRANSACTION;
REPLACE INTO `item_template`
 (`entry`,`class`,`subclass`,`name`,`displayid`,`Quality`,`Material`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,
  `AllowableClass`,`AllowableRace`,`ItemLevel`,`RequiredLevel`,`stackable`,`spellid_1`,`spelltrigger_1`,`description`)
VALUES
 (555555,0,0,'Frozen Reliquary',167973,1,8,1,0,0,0,-1,-1,1,16,20,803773,0,'A charm packed with the mana of dead mages.');
COMMIT;
