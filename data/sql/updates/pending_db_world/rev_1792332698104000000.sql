-- Make Necromancer Rime's Frozen Reliquary a reachable one-gold purchase.
DELIMITER $$
DROP PROCEDURE IF EXISTS `advai_assert_frozen_reliquary_vendor`$$
CREATE PROCEDURE `advai_assert_frozen_reliquary_vendor`() BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM `item_template`
        WHERE `entry`=555555 AND `name`='Frozen Reliquary' AND `displayid`=167973
    ) THEN
        SIGNAL SQLSTATE '45000';
    END IF;
    IF NOT EXISTS (
        SELECT 1 FROM `creature_template`
        WHERE `entry`=90001 AND (`npcflag` & 128)=128
    ) THEN
        SIGNAL SQLSTATE '45000';
    END IF;
    IF EXISTS (
        SELECT 1 FROM `npc_vendor`
        WHERE `entry`=90001 AND `slot`=18 AND `item`<>555555
    ) THEN
        SIGNAL SQLSTATE '45000';
    END IF;
END$$
CALL `advai_assert_frozen_reliquary_vendor`()$$
DROP PROCEDURE `advai_assert_frozen_reliquary_vendor`$$
DELIMITER ;

START TRANSACTION;
UPDATE `item_template` SET `BuyPrice`=10000 WHERE `entry`=555555 AND `name`='Frozen Reliquary' AND `displayid`=167973;
DELETE FROM `npc_vendor` WHERE `entry`=90001 AND `slot`=18;
INSERT INTO `npc_vendor`
    (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`VerifiedBuild`)
VALUES
    (90001,18,555555,0,0,0,12340)
ON DUPLICATE KEY UPDATE
    `slot`=VALUES(`slot`),`maxcount`=VALUES(`maxcount`),`incrtime`=VALUES(`incrtime`),
    `ExtendedCost`=VALUES(`ExtendedCost`),`VerifiedBuild`=VALUES(`VerifiedBuild`);
COMMIT;
