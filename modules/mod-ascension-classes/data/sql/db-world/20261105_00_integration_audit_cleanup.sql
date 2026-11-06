-- Integration audit cleanup (startup Errors.log findings).

-- Bogus rank links: 998302/998305/998308 are ranks 3/2/1 of Necromancer talents 11196/11210/11194,
-- not chain heads, and 998600-998602 do not exist. Talent ranks are already built from Talent.dbc.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (998302, 998305, 998308);

-- Class trainer spawns were cloned from stock trainers and inherited equipment ids that have no
-- creature_equip_template rows for these entries.
UPDATE `creature` SET `equipment_id` = 0 WHERE `id` = 98000 AND `guid` IN (9691865, 9692682, 9692738, 9693429, 9693431, 9693439, 9695131, 9695132, 9695274, 9696571, 9702088, 9718430, 9718431, 9718432, 9720128, 9720136, 9723524, 9729215, 9729230, 9733494, 9737840, 9737845, 9741518, 9741562, 9741593, 9746649, 9748643, 9748872, 9754655, 9771622, 9772006);
UPDATE `creature` SET `equipment_id` = 0 WHERE `id` = 98001 AND `guid` IN (9788456, 9789262, 9790041, 9790042, 9790045, 9791690, 9791699, 9793038, 9794627, 9794834, 9809181, 9815076, 9816705, 9820101, 9820329, 9821990, 9826309, 9830072, 9843227, 9845214, 9868204, 9868591, 9878698, 9878699);

-- The same clones inherited waypoint movement without a path.
UPDATE `creature` SET `MovementType` = 0 WHERE `id` = 98000 AND `guid` IN (9693436, 9729215, 9748872);

-- Model row for a creature entry that was never created (800936 is a spell id, not a creature).
DELETE FROM `creature_template_model` WHERE `CreatureID` = 800936;
