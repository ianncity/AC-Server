-- Cultist and Necromancer creation must grant the languages of the selected
-- race. Their custom class IDs are 12 and 13 (class mask bits 11 and 12).
INSERT INTO `playercreateinfo_skills` (`raceMask`, `classMask`, `skill`, `rank`, `comment`) VALUES
(1, 6144, 98, 0, 'Ascension classes: Human Common'),
(2, 6144, 109, 0, 'Ascension classes: Orcish'),
(4, 6144, 98, 0, 'Ascension classes: Dwarf Common'),
(4, 6144, 111, 0, 'Ascension classes: Dwarven'),
(8, 6144, 98, 0, 'Ascension classes: Night Elf Common'),
(8, 6144, 113, 0, 'Ascension classes: Darnassian'),
(16, 6144, 109, 0, 'Ascension classes: Undead Orcish'),
(16, 6144, 673, 0, 'Ascension classes: Gutterspeak'),
(32, 6144, 109, 0, 'Ascension classes: Tauren Orcish'),
(32, 6144, 115, 0, 'Ascension classes: Taurahe'),
(64, 6144, 98, 0, 'Ascension classes: Gnome Common'),
(64, 6144, 313, 0, 'Ascension classes: Gnomish'),
(128, 6144, 109, 0, 'Ascension classes: Troll Orcish'),
(128, 6144, 315, 0, 'Ascension classes: Troll'),
(512, 6144, 109, 0, 'Ascension classes: Blood Elf Orcish'),
(512, 6144, 137, 0, 'Ascension classes: Thalassian'),
(1024, 6144, 98, 0, 'Ascension classes: Draenei Common'),
(1024, 6144, 759, 0, 'Ascension classes: Draenei'),
(2048, 6144, 98, 0, 'Ascension classes: Dracthyr Alliance Common'),
(2048, 6144, 138, 0, 'Ascension classes: Dracthyr Draconic'),
(4096, 6144, 109, 0, 'Ascension classes: Dracthyr Horde Orcish'),
(4096, 6144, 138, 0, 'Ascension classes: Dracthyr Draconic')
ON DUPLICATE KEY UPDATE `rank`=VALUES(`rank`), `comment`=VALUES(`comment`);
