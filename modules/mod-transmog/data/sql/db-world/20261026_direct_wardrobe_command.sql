-- Transmog opens its item wardrobe directly; the NPC summon command is removed.
DELETE FROM `command` WHERE `name` = 'transmog portable';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('transmog wardrobe', 0, 'Syntax: .transmog wardrobe\nOpens the item appearance wardrobe.')
ON DUPLICATE KEY UPDATE `security` = VALUES(`security`), `help` = VALUES(`help`);
