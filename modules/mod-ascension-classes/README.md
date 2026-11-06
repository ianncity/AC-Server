# Ascension class data

This module supplies the shared 3.3.5 world database records required by
`mod-cultist` and `mod-necromancer`. Enable all three modules. The module database
updater applies the class content, standalone character-creation and stat, the
WotLK game-table curves for Cultist and Necromancer, and trainer migrations from
the enabled modules. Cultist uses Priest curves and Necromancer uses Warlock
curves for class-indexed crit, regeneration, and combat-rating calculations.
The class content migration holds
the spell dependencies; the standalone class migration adds character creation,
starting spells, proficiencies, and level 1-80 stats for class IDs 12 and 13;
the race-restriction migration removes Dwarf, Blood Elf, and Draenei starts for both classes and Orc starts for Cultist; the trainer migration adds native class trainers and their spell catalogs.

The integrated `PlayerbotCore` tree also includes the compatible updates from
`mod-playerbots`. Configure CMake with the complete module set and use the same
module set for database updates. The alternate `WowServer` checkout is not a
source for this stack.

Install `WOW/Client/Data/patch-ZZZZZZZ-IntegratedRaceClasses.MPQ` in the client
alongside the earlier class archives, which still supply supporting assets.
Install `WOW/Client/Interface/AddOns/AscensionClassSupport` under the client's
`Interface/AddOns` directory. The matching merged DBC tables are in
`PlayerbotCore/data/dbc`. Keep `MaxPlayerLevel` at 80 and
`EnablePlayerSettings` enabled; Ascension spell charges are persisted in the existing character settings table.

The world update contains the class summon templates, spell scripts, rank links, proc data, spell coefficients, and
creature model metadata. Skill Card data is excluded. The SQL keeps an ID collision guard for the shared class creature
entries and fails the import if any of those IDs already name unrelated content.

## Class resources and Dracthyr languages

Apply `20261018_00_dracthyr_languages.sql` after the class trainer update to grant Common to Alliance Dracthyr,
Orcish to Horde Dracthyr, and Draconic to both for new characters. The expanded-races login hook repairs all three
skills on existing characters. The Cultist addon displays its 0-100 Insanity aura stacks. The Necromancer uses Mana
as its class power, while Ascension spell records retain Runic Power costs and effects; its addon displays Life Force
and summon capacity from the 525004 and 805011 aura stacks.

## Native spell training

Cultist trainers are spawned at Priest trainer locations, and Necromancer trainers are spawned at Warlock trainer
locations. Each uses the native 3.3.5 trainer window and class restriction. The trainers offer the donor class's
ordinary spell ranks plus all class-specific catalog spells; custom spell level requirements are scaled through level
80. The updated `SkillLineAbility.dbc` and `SkillRaceClassInfo.dbc` make the inherited trainer spells valid for the new
class IDs on eligible races. Training spells does not require the Ascension advancement commands.

## In-game progression

Create a Cultist or Necromancer from the character creation screen and visit the corresponding skill trainers to learn
spells as you level. The optional `.cultist` and `.necromancer` commands remain available for the separate class
advancement data; they are not required for native trainer spell learning.
