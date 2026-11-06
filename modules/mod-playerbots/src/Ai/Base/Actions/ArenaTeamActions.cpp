/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ArenaTeamActions.h"
#include <ctime>
#include "AdvaiGuild.h"
#include "AdvaiMgr.h"
#include "ArenaTeamMgr.h"
#include "PlayerbotTextMgr.h"
#include "PlayerbotSecurity.h"
#include "Playerbots.h"

bool ArenaTeamAcceptAction::Execute(Event event)
{
    if (auto queued = Advai::DispatchGuildTask(botAI, Advai::GuildTask::ArenaAccept, event))
        return *queued;
    WorldPacket p(event.getPacket());
    p.rpos(0);
    Player* inviter = nullptr;
    std::string Invitedname;
    p >> Invitedname;

    if (normalizePlayerName(Invitedname))
        inviter = ObjectAccessor::FindPlayerByName(Invitedname.c_str());

    if (!inviter)
        return false;

    ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(bot->GetArenaTeamIdInvited());
    if (!at)
        return false;

    bool accept = true;
    bool advai = Advai::Manager::Instance().Applies(bot);
    auto expectedTeam = at->GetId();
    auto expectedSlot = at->GetSlot();
    if (advai)
    {
        if (at->GetCaptain() != inviter->GetGUID())
            return false;
        accept = botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, inviter, true) &&
                 (botAI->HasGameClientMaster() || Advai::Manager::Instance().WantsArena(
                     bot->GetGUID().GetCounter(), inviter->GetGUID().GetCounter()));
    }

    if (bot->GetArenaTeamId(at->GetSlot()))
    {
        // bot is already in an arena team
        std::string text = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "arena_team_already_in_team", "Sorry, I am already in such team", {});
        if (!advai)
            bot->Say(text, LANG_UNIVERSAL);
        accept = false;
    }

    if (accept)
    {
        WorldPacket data(CMSG_ARENA_TEAM_ACCEPT);
        bot->GetSession()->HandleArenaTeamAcceptOpcode(data);
        if (advai)
        {
            if (bot->GetArenaTeamId(expectedSlot) != expectedTeam)
                return false;
            Advai::Manager::Instance().Observe(bot->GetGUID().GetCounter(),
                {Advai::SocialEventKind::Assistance, inviter->GetGUID().GetCounter(),
                 static_cast<Advai::Timestamp>(time(nullptr)), true});
            return true;
        }
        std::string text = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "arena_team_thanks_for_invite", "Thanks for the invite!", {});
        bot->Say(text, LANG_UNIVERSAL);
        LOG_INFO("playerbots", "Bot {} <{}> accepts Arena Team invite", bot->GetGUID().ToString().c_str(),
                 bot->GetName().c_str());
        return true;
    }
    else
    {
        WorldPacket data(CMSG_ARENA_TEAM_DECLINE);
        bot->GetSession()->HandleArenaTeamDeclineOpcode(data);
        LOG_INFO("playerbots", "Bot {} <{}> declines Arena Team invite", bot->GetGUID().ToString().c_str(),
                 bot->GetName().c_str());
        return advai;
    }

    return false;
}
