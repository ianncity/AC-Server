-- Restore the missing rank-one entries for source-defined Ascension spell chains.
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 300234 AND `spell_id` = 300234 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500110 AND `spell_id` = 500110 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500217 AND `spell_id` = 500217 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500338 AND `spell_id` = 500338 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500711 AND `spell_id` = 500711 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500714 AND `spell_id` = 500714 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500715 AND `spell_id` = 500715 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500719 AND `spell_id` = 500719 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500720 AND `spell_id` = 500720 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500721 AND `spell_id` = 500721 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500965 AND `spell_id` = 500965 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500967 AND `spell_id` = 500967 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500968 AND `spell_id` = 500968 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 500970 AND `spell_id` = 500970 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 704887 AND `spell_id` = 704887 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 706179 AND `spell_id` = 706179 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 800105 AND `spell_id` = 800105 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 800199 AND `spell_id` = 800199 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 800413 AND `spell_id` = 800413 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 800416 AND `spell_id` = 800416 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 800446 AND `spell_id` = 800446 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 801722 AND `spell_id` = 801722 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 801760 AND `spell_id` = 801760 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 801791 AND `spell_id` = 801791 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 801792 AND `spell_id` = 801792 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 801945 AND `spell_id` = 801945 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 804152 AND `spell_id` = 804152 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 805027 AND `spell_id` = 805027 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 805116 AND `spell_id` = 805116 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 805429 AND `spell_id` = 805429 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 805572 AND `spell_id` = 805572 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 806149 AND `spell_id` = 806149 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 806222 AND `spell_id` = 806222 AND `rank` = 1;
DELETE FROM `spell_ranks` WHERE `first_spell_id` = 806266 AND `spell_id` = 806266 AND `rank` = 1;
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
    (300234, 300234, 1),
    (500110, 500110, 1),
    (500217, 500217, 1),
    (500338, 500338, 1),
    (500711, 500711, 1),
    (500714, 500714, 1),
    (500715, 500715, 1),
    (500719, 500719, 1),
    (500720, 500720, 1),
    (500721, 500721, 1),
    (500965, 500965, 1),
    (500967, 500967, 1),
    (500968, 500968, 1),
    (500970, 500970, 1),
    (704887, 704887, 1),
    (706179, 706179, 1),
    (800105, 800105, 1),
    (800199, 800199, 1),
    (800413, 800413, 1),
    (800416, 800416, 1),
    (800446, 800446, 1),
    (801722, 801722, 1),
    (801760, 801760, 1),
    (801791, 801791, 1),
    (801792, 801792, 1),
    (801945, 801945, 1),
    (804152, 804152, 1),
    (805027, 805027, 1),
    (805116, 805116, 1),
    (805429, 805429, 1),
    (805572, 805572, 1),
    (806149, 806149, 1),
    (806222, 806222, 1),
    (806266, 806266, 1);
