-- The custom class trainers named skill line 78 (Shadow Magic) on every service.
--
-- A class trainer's service must not carry a skill line the player does not have:
-- the client's class trainer frame resolves each service's skill line against the
-- player's own skills, so all of them were dropped and the window rendered empty
-- even though SMSG_TRAINER_LIST carried the full list. Vanilla class trainers use 0.
UPDATE `trainer_spell` SET `ReqSkillLine` = 0, `ReqSkillRank` = 0
WHERE `TrainerId` IN (98000, 98001) AND `ReqSkillLine` = 78;
