-- Ascendance (990126) transforms the shaman into creature 990502 ("Water Ascendant"). That creature's display (977372)
-- points at CREATURE\TWILIGHTASCENDANTWATER\TWILIGHTASCENDANTWATER.M2, a Mists-of-Pandaria model that is not in any
-- archive the 3.3.5 client loads, so the transformed player drew as a missing-model box. Until the MoP model is converted
-- and shipped, use the stock Water Elemental display (525), which every client has, at a larger scale.
UPDATE `creature_template_model` SET `CreatureDisplayID` = 525, `DisplayScale` = 1.6 WHERE `CreatureID` = 990502 AND `Idx` = 0;
