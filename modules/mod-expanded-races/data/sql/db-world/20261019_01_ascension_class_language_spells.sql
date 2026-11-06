-- Cultist (class 12) and Necromancer (class 13) need the language spells as
-- well as language skill records. Without these spells, chat rejects their
-- default faction language even when the skill value is 300.
INSERT INTO `playercreateinfo_spell_custom` (`racemask`, `classmask`, `Spell`, `Note`) VALUES
(1, 6144, 668, 'Ascension classes: Human Common language'),
(2, 6144, 669, 'Ascension classes: Orcish language'),
(4, 6144, 668, 'Ascension classes: Dwarf Common language'),
(4, 6144, 672, 'Ascension classes: Dwarven language'),
(8, 6144, 668, 'Ascension classes: Night Elf Common language'),
(8, 6144, 671, 'Ascension classes: Darnassian language'),
(16, 6144, 669, 'Ascension classes: Undead Orcish language'),
(16, 6144, 17737, 'Ascension classes: Gutterspeak language'),
(32, 6144, 669, 'Ascension classes: Tauren Orcish language'),
(32, 6144, 670, 'Ascension classes: Taurahe language'),
(64, 6144, 668, 'Ascension classes: Gnome Common language'),
(64, 6144, 7340, 'Ascension classes: Gnomish language'),
(128, 6144, 669, 'Ascension classes: Troll Orcish language'),
(128, 6144, 7341, 'Ascension classes: Troll language'),
(512, 6144, 669, 'Ascension classes: Blood Elf Orcish language'),
(512, 6144, 813, 'Ascension classes: Thalassian language'),
(1024, 6144, 668, 'Ascension classes: Draenei Common language'),
(1024, 6144, 29932, 'Ascension classes: Draenei language'),
(2048, 6144, 668, 'Ascension classes: Alliance Dracthyr Common language'),
(2048, 6144, 814, 'Ascension classes: Alliance Dracthyr Draconic language'),
(4096, 6144, 669, 'Ascension classes: Horde Dracthyr Orcish language'),
(4096, 6144, 814, 'Ascension classes: Horde Dracthyr Draconic language')
ON DUPLICATE KEY UPDATE `Note`=VALUES(`Note`);
