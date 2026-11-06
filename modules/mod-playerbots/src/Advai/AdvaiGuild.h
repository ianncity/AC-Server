/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_GUILD_H
#define PLAYERBOTS_ADVAI_GUILD_H

#include <cstdint>
#include <optional>
#include <string>

class Event;
class PlayerbotAI;

namespace Advai
{
enum class GuildTask : std::uint8_t
{
    BuyPetition,
    OfferPetition,
    OfferNearby,
    TurnIn,
    Sign,
    AcceptInvite,
    BuyTabard,
    BankDeposit,
    ArenaAccept
};

// Empty means the caller is already executing in the required context.
std::optional<bool> DispatchGuildTask(PlayerbotAI* ai, GuildTask task, Event& event);
void UpdateGuildTasks();
std::string GuildTaskStatistics();
}

#endif
