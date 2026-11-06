-- Replace Prestige shoulder/shield displays accidentally copied from a weapon.
-- Active 3.3.5 ItemDisplayInfo rows: 64707 is shoulder armor; 65029 is a shield.
UPDATE `item_template` SET `displayid` = 64707 WHERE `entry` = 96016 AND `displayid` = 33857;
UPDATE `item_template` SET `displayid` = 65029 WHERE `entry` = 96017 AND `displayid` = 33857;
