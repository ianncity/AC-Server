-- Give both Dracthyr factions their native and faction languages at creation.
-- The login hook also repairs existing characters that predate this update.
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`) VALUES
(2048, 0, 98, 0, 'Dracthyr Alliance: Common'),
(4096, 0, 109, 0, 'Dracthyr Horde: Orcish'),
(6144, 0, 138, 0, 'Dracthyr: Draconic')
ON DUPLICATE KEY UPDATE `rank`=VALUES(`rank`), `comment`=VALUES(`comment`);
