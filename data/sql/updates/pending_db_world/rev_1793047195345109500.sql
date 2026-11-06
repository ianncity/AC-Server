-- Original Wrath implementations of the requested MoP capstone mechanics.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_capstone_avatar', 'spell_capstone_binding_shot_cast', 'spell_capstone_binding_shot_targets', 'spell_capstone_binding_shot_tether', 'spell_capstone_binding_shot_zone', 'spell_capstone_chosen_of_elune', 'spell_capstone_death_prevention', 'spell_capstone_dragon_roar', 'spell_capstone_elemental_blast', 'spell_capstone_elemental_blast_stat', 'spell_capstone_execution_sentence', 'spell_capstone_execution_sentence_aura', 'spell_capstone_execution_sentence_damage_aura', 'spell_capstone_execution_sentence_heal_aura', 'spell_capstone_frost_bomb_aura', 'spell_capstone_frost_bomb_cast', 'spell_capstone_frost_bomb_damage', 'spell_capstone_frost_bomb_splash', 'spell_capstone_halo', 'spell_capstone_halo_impact', 'spell_capstone_holy_prism', 'spell_capstone_holy_prism_allies', 'spell_capstone_holy_prism_enemies', 'spell_capstone_king_of_the_jungle', 'spell_capstone_king_of_the_jungle_prowl', 'spell_capstone_lynx_rush_aura', 'spell_capstone_lynx_rush_bleed', 'spell_capstone_lynx_rush_cast', 'spell_capstone_marked_for_death', 'spell_capstone_mindbender', 'spell_capstone_mindbender_mana', 'spell_capstone_natures_vigil', 'spell_capstone_primal_elementalist', 'spell_capstone_primal_immolate', 'spell_capstone_primal_summon', 'spell_capstone_purgatory_shroud', 'spell_capstone_restoration_ascendance', 'spell_capstone_restorative_mists', 'spell_capstone_rune_of_power', 'spell_capstone_rune_of_power_buff', 'spell_capstone_rune_of_power_ground', 'spell_capstone_runic_corruption', 'spell_capstone_runic_regeneration', 'spell_capstone_shadow_blades', 'spell_capstone_shadow_blades_combo', 'spell_capstone_shadow_reflection_aura', 'spell_capstone_shadow_reflection_capture', 'spell_capstone_shadow_reflection_summon', 'spell_capstone_spirit_shell_heal', 'spell_capstone_storm_bolt', 'spell_capstone_supremacy_bulwark_owner', 'spell_capstone_supremacy_cauterize_master', 'spell_capstone_supremacy_clone_heal', 'spell_capstone_supremacy_clone_magic', 'spell_capstone_supremacy_doom_bolt', 'spell_capstone_supremacy_mortal_cleave', 'spell_capstone_supremacy_sear_magic', 'spell_capstone_supremacy_shadow_bulwark', 'spell_capstone_supremacy_suffering', 'spell_capstone_supremacy_summon', 'spell_capstone_supremacy_void_shield', 'spell_capstone_supremacy_void_shield_aura', 'spell_capstone_unholy_blight_pulse', 'spell_capstone_vigil_damage', 'spell_capstone_vigil_heal');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(990121, 'spell_capstone_marked_for_death'),
(990128, 'spell_capstone_avatar'),
(990129, 'spell_capstone_storm_bolt'),
(991000, 'spell_capstone_unholy_blight_pulse'),
(990127, 'spell_capstone_dragon_roar'),
(990106, 'spell_capstone_death_prevention'),
(991002, 'spell_capstone_purgatory_shroud'),
(990113, 'spell_capstone_death_prevention'),
(990107, 'spell_capstone_runic_corruption'),
(991006, 'spell_capstone_runic_regeneration'),
(990111, 'spell_capstone_natures_vigil'),
(991007, 'spell_capstone_vigil_heal'),
(991008, 'spell_capstone_vigil_damage'),
(-2054, 'spell_capstone_spirit_shell_heal'),
(-2061, 'spell_capstone_spirit_shell_heal'),
(-2060, 'spell_capstone_spirit_shell_heal'),
(-596, 'spell_capstone_spirit_shell_heal'),
(990122, 'spell_capstone_shadow_blades'),
(-53, 'spell_capstone_shadow_reflection_capture'),
(-1752, 'spell_capstone_shadow_reflection_capture'),
(-8676, 'spell_capstone_shadow_reflection_capture'),
(-16511, 'spell_capstone_shadow_reflection_capture'),
(-1329, 'spell_capstone_shadow_reflection_capture'),
(-2098, 'spell_capstone_shadow_reflection_capture'),
(-32645, 'spell_capstone_shadow_reflection_capture'),
(-1943, 'spell_capstone_shadow_reflection_capture'),
(-703, 'spell_capstone_shadow_reflection_capture'),
(-51723, 'spell_capstone_shadow_reflection_capture'),
(-1766, 'spell_capstone_shadow_reflection_capture'),
(-408, 'spell_capstone_shadow_reflection_capture'),
(-1833, 'spell_capstone_shadow_reflection_capture'),
(-2094, 'spell_capstone_shadow_reflection_capture'),
(-1776, 'spell_capstone_shadow_reflection_capture'),
(-51690, 'spell_capstone_shadow_reflection_capture'),
(-1752, 'spell_capstone_shadow_blades_combo'),
(-53, 'spell_capstone_shadow_blades_combo'),
(-8676, 'spell_capstone_shadow_blades_combo'),
(-703, 'spell_capstone_shadow_blades_combo'),
(-1833, 'spell_capstone_shadow_blades_combo'),
(-1776, 'spell_capstone_shadow_blades_combo'),
(-1329, 'spell_capstone_shadow_blades_combo'),
(-5940, 'spell_capstone_shadow_blades_combo'),
(-14278, 'spell_capstone_shadow_blades_combo'),
(-14251, 'spell_capstone_shadow_blades_combo'),
(-16511, 'spell_capstone_shadow_blades_combo'),
(990109, 'spell_capstone_chosen_of_elune'),
(990126, 'spell_capstone_restoration_ascendance'),
(991012, 'spell_capstone_restorative_mists'),
(990114, 'spell_capstone_frost_bomb_cast'),
(990114, 'spell_capstone_frost_bomb_aura'),
(991013, 'spell_capstone_frost_bomb_damage'),
(991014, 'spell_capstone_frost_bomb_damage'),
(991014, 'spell_capstone_frost_bomb_splash'),
(990115, 'spell_capstone_holy_prism'),
(991016, 'spell_capstone_holy_prism_allies'),
(991018, 'spell_capstone_holy_prism_enemies'),
(990103, 'spell_capstone_lynx_rush_cast'),
(991019, 'spell_capstone_lynx_rush_aura'),
(991020, 'spell_capstone_lynx_rush_bleed'),
(990104, 'spell_capstone_binding_shot_cast'),
(990104, 'spell_capstone_binding_shot_zone'),
(991021, 'spell_capstone_binding_shot_targets'),
(991021, 'spell_capstone_binding_shot_tether'),
(990119, 'spell_capstone_halo'),
(991024, 'spell_capstone_halo_impact'),
(991025, 'spell_capstone_halo_impact'),
(990112, 'spell_capstone_rune_of_power'),
(991026, 'spell_capstone_rune_of_power_ground'),
(991027, 'spell_capstone_rune_of_power_ground'),
(991028, 'spell_capstone_rune_of_power_buff'),
(990117, 'spell_capstone_execution_sentence'),
(991029, 'spell_capstone_execution_sentence_damage_aura'),
(991030, 'spell_capstone_execution_sentence_heal_aura'),
(990110, 'spell_capstone_king_of_the_jungle'),
(-5215, 'spell_capstone_king_of_the_jungle_prowl'),
(990120, 'spell_capstone_mindbender'),
(991032, 'spell_capstone_mindbender_mana'),
(990124, 'spell_capstone_primal_elementalist'),
(32982, 'spell_capstone_primal_summon'),
(33663, 'spell_capstone_primal_summon'),
(991039, 'spell_capstone_primal_immolate'),
(990125, 'spell_capstone_elemental_blast'),
(991040, 'spell_capstone_elemental_blast_stat'),
(991041, 'spell_capstone_elemental_blast_stat'),
(991042, 'spell_capstone_elemental_blast_stat'),
(991043, 'spell_capstone_elemental_blast_stat'),
(991047, 'spell_capstone_supremacy_summon'),
(991048, 'spell_capstone_supremacy_summon'),
(991049, 'spell_capstone_supremacy_summon'),
(991050, 'spell_capstone_supremacy_summon'),
(991051, 'spell_capstone_supremacy_summon'),
(991053, 'spell_capstone_supremacy_sear_magic'),
(991058, 'spell_capstone_supremacy_clone_magic'),
(991059, 'spell_capstone_supremacy_clone_heal'),
(991061, 'spell_capstone_supremacy_mortal_cleave'),
(991065, 'spell_capstone_supremacy_cauterize_master'),
(991066, 'spell_capstone_supremacy_void_shield'),
(991066, 'spell_capstone_supremacy_void_shield_aura'),
(991075, 'spell_capstone_supremacy_shadow_bulwark'),
(991076, 'spell_capstone_supremacy_bulwark_owner'),
(991078, 'spell_capstone_supremacy_suffering'),
(991084, 'spell_capstone_supremacy_doom_bolt'),
(991089, 'spell_capstone_supremacy_summon'),
(991090, 'spell_capstone_supremacy_summon'),
(990123, 'spell_capstone_shadow_reflection_aura'),
(990123, 'spell_capstone_shadow_reflection_summon');

DELETE FROM `spell_bonus_data` WHERE `entry` IN (990127, 991020);
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(990127, 0, 0, 1.4, 0, 'Capstone: Dragon Roar - original 140% attack power coefficient'),
(991020, 0, 0, 0, 0.038, 'Capstone: Lynx Rush - original 3.8% attack power per bleed tick');

DELETE FROM `spell_group` WHERE `id` = 1061 AND `spell_id` = 991061;
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES (1061, 991061);

-- Mindbender: native Wrath Shadowfiend scaling and the audited original MOP model.
REPLACE INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES (62982, 0, 0, 0, 0, 0, 'Mindbender', NULL, NULL, 0, 66, 70, 1, 14, 0, 1.2, 1.14286, 1.0, 1.0, 20.0, 0, 5, 1.0, 1500, 2000, 1.0, 1.0, 2, 0, 2048, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 1, 5.0, 1.7, 1.0, 1.15, 1.0, 0, 48, 1, 0, 0, '', 0);
REPLACE INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(62982,0,977371,1.0,1.0,12340);
REPLACE INTO `creature_model_info` (`DisplayID`,`BoundingRadius`,`CombatReach`,`Gender`,`DisplayID_Other_Gender`,`VerifiedBuild`) VALUES
(977371, 0.6111119985580444, 2.031280040740967, 2, 0, 0);

-- Shadow Reflection copies the owner display after summon; a native mirror-image shell keeps CreatureTemplate validation and first-frame display valid.
REPLACE INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(990777, 0, 0, 0, 0, 0, 'Shadow Reflection', '', NULL, 0, 80, 80, 0, 35, 0, 1.0, 1.14286, 1.0, 1.0, 20.0, 0, 0, 1.0, 1800, 1800, 1.0, 1.0, 4, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1.0, 1.0, 1.0, 1.0, 1.0, 0, 0, 1, 0, 0, 'npc_capstone_shadow_reflection', 0);
REPLACE INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(990777, 0, 11686, 1.0, 1.0, 12340);

-- Water Ascendant: original MOP display and model used by Restoration Ascendance's native transform aura.
REPLACE INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES (990502, 0, 0, 0, 0, 0, 'Water Ascendant', 'The Ascendant', NULL, 0, 80, 80, 0, 35, 0, 1.2, 1.14286, 1.0, 1.0, 20.0, 0, 0, 1.0, 2000, 2000, 1.0, 1.0, 1, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 0.0, 1.0, 1.0, 1.0, 0.0, 0, 0, 1, 0, 0, '', 0);
REPLACE INTO `creature_template_model` (`CreatureID`,`Idx`,`CreatureDisplayID`,`DisplayScale`,`Probability`,`VerifiedBuild`) VALUES
(990502,0,977372,1.0,1.0,12340);
REPLACE INTO `creature_model_info` (`DisplayID`,`BoundingRadius`,`CombatReach`,`Gender`,`DisplayID_Other_Gender`,`VerifiedBuild`) VALUES
(977372, 0.6111119985580444, 2.031280040740967, 2, 0, 0);
