-- The bot-summoned tentacle (95010) had been switched to display 25547, a bare HumanFemale character
-- model with no extra display data, so it rendered as an untextured shape. Restore the C'Thun claw
-- tentacle display this module intends (15789, Creature\ClawofKathune with TentacleBrown).
DELETE FROM `creature_template_model` WHERE `CreatureID` = 95010;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(95010, 0, 15789, 0.8, 1, 12340);
