-- Complete the native spell dependency for Necromancer Rime's Frozen Reliquary.
-- Item.dbc in the client patch maps item 555555 to ItemDisplayInfo 167973.
DELIMITER $$
DROP PROCEDURE IF EXISTS `advai_assert_frozen_reliquary`$$
CREATE PROCEDURE `advai_assert_frozen_reliquary`() BEGIN
IF EXISTS (SELECT 1 FROM `item_template` WHERE `entry`=555555 AND
 `name`<>'Frozen Reliquary') THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Frozen Reliquary item ID conflicts with existing world content';
END IF; END$$
CALL `advai_assert_frozen_reliquary`()$$
DROP PROCEDURE `advai_assert_frozen_reliquary`$$
DELIMITER ;
START TRANSACTION;
INSERT INTO `item_template`
 (`entry`,`class`,`subclass`,`name`,`displayid`,`Quality`,`BuyCount`,`BuyPrice`,`SellPrice`,`InventoryType`,
  `AllowableClass`,`AllowableRace`,`ItemLevel`,`RequiredLevel`,`stackable`,`spellid_1`,`spelltrigger_1`,`description`)
VALUES
 (555555,0,0,'Frozen Reliquary',167973,1,1,10000,0,0,-1,-1,1,16,20,803773,0,'A charm packed with the mana of dead mages.')
ON DUPLICATE KEY UPDATE
 `class`=VALUES(`class`),`subclass`=VALUES(`subclass`),`name`=VALUES(`name`),`displayid`=VALUES(`displayid`),
 `Quality`=VALUES(`Quality`),`BuyCount`=VALUES(`BuyCount`),`BuyPrice`=VALUES(`BuyPrice`),`SellPrice`=VALUES(`SellPrice`),
 `InventoryType`=VALUES(`InventoryType`),`AllowableClass`=VALUES(`AllowableClass`),`AllowableRace`=VALUES(`AllowableRace`),
 `ItemLevel`=VALUES(`ItemLevel`),`RequiredLevel`=VALUES(`RequiredLevel`),`stackable`=VALUES(`stackable`),
 `spellid_1`=VALUES(`spellid_1`),`spelltrigger_1`=VALUES(`spelltrigger_1`),`description`=VALUES(`description`);
COMMIT;
