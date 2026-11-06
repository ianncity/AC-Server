-- Bloodforged Vanguard Blade drew as a blue/white checkered cube on the test paladin. Point it at a stock one-handed
-- sword display (Sword_1H_Short_B_02, present unchanged in the base game archives) instead of the Wrath patch display.
UPDATE `item_template` SET `displayid` = 20218 WHERE `entry` = 96280;
