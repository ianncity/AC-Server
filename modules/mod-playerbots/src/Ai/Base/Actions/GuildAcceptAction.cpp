/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GuildAcceptAction.h"

#include <ctime>

#include "AdvaiGuild.h"
#include "AdvaiMgr.h"
#include "Event.h"
#include "GuildPackets.h"
#include "PlayerbotSecurity.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"

bool GuildAcceptAction::Execute(Event event)
{
    if (auto queued = Advai::DispatchGuildTask(botAI, Advai::GuildTask::AcceptInvite, event))
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

    bool accept = true;
    uint32 guildId = inviter->GetGuildId();
    if (!guildId)
    {
        botAI->TellError(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "guild_accept_inviter_not_in_guild", "You are not in a guild!", {}));
        accept = false;
    }
    else if (bot->GetGuildId())
    {
        botAI->TellError(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "guild_accept_already_in_guild", "Sorry, I am in a guild already", {}));
        accept = false;
    }
    else if (!botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, inviter, true))
    {
        botAI->TellError(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "guild_accept_declined", "Sorry, I don't want to join your guild :(", {}));
        accept = false;
    }
    bool advai = Advai::Manager::Instance().Applies(bot);
    if (advai)
    {
        if (bot->GetGuildIdInvited() != guildId)
            return false;
        if (!botAI->HasGameClientMaster() &&
            !Advai::Manager::Instance().WantsGuild(bot->GetGUID().GetCounter(), inviter->GetGUID().GetCounter()))
            accept = false;
    }

    if (accept)
    {
        WorldPackets::Guild::AcceptGuildInvite data = WorldPacket(CMSG_GUILD_ACCEPT);
        bot->GetSession()->HandleGuildAcceptOpcode(data);
        if (advai && bot->GetGuildId() != guildId)
        {
            Advai::Manager::Instance().Report(bot->GetGUID().GetCounter(),
                {Advai::Goal::Socialize, Advai::Outcome::Blocked, "native guild acceptance rejected membership eligibility"});
            return false;
        }
        if (advai)
            Advai::Manager::Instance().Observe(bot->GetGUID().GetCounter(),
                {Advai::SocialEventKind::GuildParticipation, inviter->GetGUID().GetCounter(),
                 static_cast<Advai::Timestamp>(time(nullptr)), true});
    }
    else
    {
        WorldPackets::Guild::GuildDeclineInvitation data = WorldPacket(CMSG_GUILD_DECLINE);
        bot->GetSession()->HandleGuildDeclineOpcode(data);
    }

    return true;
}
