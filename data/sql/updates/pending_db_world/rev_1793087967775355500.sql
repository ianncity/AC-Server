-- Grimoire of Supremacy: original demon identities and privately ported displays.
-- Native Wrath species retain their stat, faction and server reach budgets; client geometry uses source sizing.
-- New guardians use dedicated source-derived AI. Native Infernal addon auras are not copied.
-- Shivarra uses its original converted sword via a private NPC-only item/display.
REPLACE INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `stat_type1`, `stat_value1`, `stat_type2`, `stat_value2`, `stat_type3`, `stat_value3`, `stat_type4`, `stat_value4`, `stat_type5`, `stat_value5`, `stat_type6`, `stat_value6`, `stat_type7`, `stat_value7`, `stat_type8`, `stat_value8`, `stat_type9`, `stat_value9`, `stat_type10`, `stat_value10`, `ScalingStatDistribution`, `ScalingStatValue`, `dmg_min1`, `dmg_max1`, `dmg_type1`, `dmg_min2`, `dmg_max2`, `dmg_type2`, `armor`, `holy_res`, `fire_res`, `nature_res`, `frost_res`, `shadow_res`, `arcane_res`, `delay`, `ammo_type`, `RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`, `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`, `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`, `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`, `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`, `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`, `RandomSuffix`, `block`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `duration`, `ItemLimitCategory`, `HolidayId`, `ScriptName`, `DisenchantID`, `FoodType`, `minMoneyLoot`, `maxMoneyLoot`, `flagsCustom`, `VerifiedBuild`) VALUES
(555556, 2, 7, -1, 'NPC Equip Shivarra Source Sword', 167974, 0, 0, 0, 1, 0, 0, 13, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 1000, 0, 0.0, 0, 0, 0, 0.0, -1, 0, -1, 0, 0, 0, 0.0, -1, 0, -1, 0, 0, 0, 0.0, -1, 0, -1, 0, 0, 0, 0.0, -1, 0, -1, 0, 0, 0, 0.0, -1, 0, -1, 0, '', 0, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0.0, 0, 0, 0, '', 0, 0, 0, 0, 0, 0);

REPLACE INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(58959, 0, 0, 0, 0, 0, 'Fel Imp', '', '', 0, 5, 5, 0, 73, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 8, 0, 2048, 0, 23, 3, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 0.84, 0.33, 1.0, 1.0, 0, 121, 1, 0, 0, '', 0),
(58960, 0, 0, 0, 0, 0, 'Voidlord', '', '', 0, 24, 24, 0, 90, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 2, 0, 2048, 0, 16, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 1.1, 0.77, 1.0, 1.0, 0, 121, 1, 0, 0, '', 0),
(58963, 0, 0, 0, 0, 0, 'Shivarra', '', '', 0, 24, 24, 0, 90, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 2, 0, 2048, 0, 17, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 0.9, 0.77, 1.0, 1.0, 0, 121, 1, 0, 0, '', 0),
(58964, 0, 0, 0, 0, 0, 'Observer', '', '', 0, 80, 80, 2, 90, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 2, 0, 2048, 0, 15, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 0.95, 0.77, 1.0, 1.0, 0, 121, 1, 0, 0, '', 0),
(58965, 0, 0, 0, 0, 0, 'Wrathguard', '', '', 0, 80, 80, 2, 90, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 2, 32768, 2048, 0, 29, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 1, 1.0, 1.1, 0.77, 1.0, 1.0, 0, 121, 1, 0, 0, '', 0),
(58997, 0, 0, 0, 0, 0, 'Abyssal', '', '', 0, 66, 70, 2, 954, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 3.15, 2000, 2000, 1.0, 1.0, 1, 32768, 2048, 0, 19, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 1.9, 1.0, 1.85, 1.0, 0, 121, 1, 0, 0, 'npc_capstone_supremacy_abyssal', 0),
(59000, 0, 0, 0, 0, 0, 'Terrorguard', NULL, NULL, 0, 57, 73, 2, 954, 0, 1.0, 1.14286, 1.0, 1.0, 18.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 2, 0, 2048, 0, 19, 3, 0, 0, 0, 0, 0, 0, 0, 0, '', 1, 1.0, 1.3, 1.55998, 1.0, 1.0, 0, 121, 1, 0, 0, 'npc_capstone_supremacy_terrorguard', 0);

REPLACE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(58959, 0, 977364, 1.0, 1.0, 0),
(58960, 0, 977365, 1.0, 1.0, 0),
(58963, 0, 977366, 1.0, 1.0, 0),
(58964, 0, 977367, 1.0, 1.0, 0),
(58965, 0, 977368, 1.0, 1.0, 0),
(58997, 0, 977369, 1.0, 1.0, 0),
(59000, 0, 977370, 1.0, 1.0, 0);

REPLACE INTO `creature_model_info` (`DisplayID`, `BoundingRadius`, `CombatReach`, `Gender`, `DisplayID_Other_Gender`, `VerifiedBuild`) VALUES
(977364, 0.5, 0.75, 2, 0, 0),
(977365, 0.8, 2.0, 2, 0, 0),
(977366, 0.403, 1.5, 1, 0, 0),
(977367, 0.1735, 0.75, 0, 0, 0),
(977368, 1.15, 2.3, 0, 0, 0),
(977369, 0.903, 2.0, 2, 0, 0),
(977370, 0.9168, 1.8, 2, 0, 0);

DELETE FROM `creature_template_spell` WHERE `CreatureID` IN (58959, 58960, 58963, 58964, 58965, 58997, 59000);
INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`, `VerifiedBuild`) VALUES
(58959, 0, 991052, 0),
(58959, 1, 991053, 0),
(58959, 2, 991065, 0),
(58960, 0, 991066, 0),
(58960, 1, 991071, 0),
(58960, 2, 991072, 0),
(58960, 3, 991073, 0),
(58960, 4, 991074, 0),
(58960, 5, 991075, 0),
(58960, 6, 991078, 0),
(58963, 0, 991054, 0),
(58963, 1, 991063, 0),
(58963, 2, 991064, 0),
(58964, 0, 991055, 0),
(58964, 1, 991056, 0),
(58964, 2, 991058, 0),
(58965, 0, 991061, 0),
(58965, 1, 991079, 0),
(58965, 2, 991081, 0),
(58965, 3, 991082, 0),
(58997, 0, 991087, 0),
(59000, 0, 991084, 0);

DELETE FROM `creature_equip_template` WHERE `CreatureID` = 58963;
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`) VALUES
(58963, 1, 555556, 555556, 0, 0);
