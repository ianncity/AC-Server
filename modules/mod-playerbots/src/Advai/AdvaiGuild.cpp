/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiGuild.h"

#include <ctime>
#include <chrono>
#include <algorithm>
#include <map>
#include <mutex>
#include <set>

#include "AdvaiMgr.h"
#include "ArenaTeamActions.h"
#include "Event.h"
#include "GuildCreateActions.h"
#include "GuildAcceptAction.h"
#include "GuildBankAction.h"
#include "ObjectAccessor.h"
#include "PetitionSignAction.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PlayerbotWorldThreadProcessor.h"

namespace Advai
{
namespace
{
thread_local bool executingGuildTask = false;
std::mutex pendingMutex;
std::set<std::pair<BotId, GuildTask>> pending;
std::mutex delayedMutex;
std::multimap<std::chrono::steady_clock::time_point, std::unique_ptr<PlayerbotOperation>> delayed;

class GuildOperation final : public PlayerbotOperation
{
public:
    GuildOperation(ObjectGuid bot, GuildTask task, Event& event)
        : _bot(bot), _task(task), _source(event.GetSource()), _param(event.getParam()), _packet(event.getPacket()),
          _expires(static_cast<Timestamp>(time(nullptr)) + 60) {}

    ~GuildOperation() override
    {
        std::lock_guard lock(pendingMutex);
        pending.erase({_bot.GetCounter(), _task});
    }

    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI native guild charter operation"; }
    bool Expired() const { return static_cast<Timestamp>(time(nullptr)) >= _expires; }

    bool Execute() override
    {
        auto& manager = Manager::Instance();
        if (static_cast<Timestamp>(time(nullptr)) >= _expires)
        {
            manager.Report(_bot.GetCounter(), {Goal::Socialize, Outcome::Blocked, "queued guild charter task expired"});
            return false;
        }
        auto* bot = ObjectAccessor::FindPlayer(_bot);
        if (!bot || !bot->IsInWorld() || !bot->IsAlive() || !manager.Applies(bot) ||
            !manager.Active(_bot.GetCounter()) || bot->IsInCombat() || bot->GetTradeData() ||
            bot->IsBeingTeleported() || bot->IsInFlight())
            return false;
        auto* ai = GET_PLAYERBOT_AI(bot);
        if (!ai)
            return false;
        struct Context
        {
            Context() { executingGuildTask = true; }
            ~Context() { executingGuildTask = false; }
        } context;
        Event event = _packet.empty() ? Event(_source, _param) : Event(_source, _packet);
        bool result = false;
        switch (_task)
        {
            case GuildTask::BuyPetition: result = BuyPetitionAction(ai).Execute(event); break;
            case GuildTask::OfferPetition: result = PetitionOfferAction(ai).Execute(event); break;
            case GuildTask::OfferNearby: result = PetitionOfferNearbyAction(ai).Execute(event); break;
            case GuildTask::TurnIn: result = PetitionTurnInAction(ai).Execute(event); break;
            case GuildTask::Sign: result = PetitionSignAction(ai).Execute(event); break;
            case GuildTask::AcceptInvite: result = GuildAcceptAction(ai).Execute(event); break;
            case GuildTask::BuyTabard: result = BuyTabardAction(ai).Execute(event); break;
            case GuildTask::BankDeposit: result = GuildBankAction(ai).Execute(event); break;
            case GuildTask::ArenaAccept: result = ArenaTeamAcceptAction(ai).Execute(event); break;
        }
        if (!result)
            manager.Report(_bot.GetCounter(), {Goal::Socialize, Outcome::Blocked,
                                               "native guild charter action did not complete its eligibility or interaction checks"});
        return result;
    }

private:
    ObjectGuid _bot;
    GuildTask _task;
    std::string _source;
    std::string _param;
    WorldPacket _packet;
    Timestamp _expires;
};
}

std::optional<bool> DispatchGuildTask(PlayerbotAI* ai, GuildTask task, Event& event)
{
    auto& manager = Manager::Instance();
    auto* bot = ai->GetBot();
    if (!manager.Applies(bot) || executingGuildTask)
        return std::nullopt;
    {
        std::lock_guard lock(pendingMutex);
        if (!pending.emplace(bot->GetGUID().GetCounter(), task).second)
            return false;
    }
    auto operation = std::make_unique<GuildOperation>(bot->GetGUID(), task, event);
    bool reactive = task == GuildTask::Sign || task == GuildTask::AcceptInvite || task == GuildTask::ArenaAccept;
    bool queued = false;
    if (reactive)
    {
        auto persona = manager.Persona(bot->GetGUID().GetCounter());
        auto context = static_cast<std::uint32_t>(task) ^ static_cast<std::uint32_t>(event.getPacket().size()) ^
                       static_cast<std::uint32_t>(time(nullptr));
        auto milliseconds = std::clamp<std::uint32_t>(
            persona.ReactionMilliseconds(context) + (100 - persona.Skill) * 8 +
            static_cast<std::uint32_t>(event.getPacket().size()) * 8, 800, 8000);
        std::lock_guard lock(delayedMutex);
        if (delayed.size() < 4096)
        {
            delayed.emplace(std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds),
                            std::move(operation));
            queued = true;
        }
    }
    else
        queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(std::move(operation));
    if (!queued)
        manager.Report(bot->GetGUID().GetCounter(), {Goal::Socialize, Outcome::Blocked,
                                                    "guild charter operation queue is full"});
    return queued;
}

void UpdateGuildTasks()
{
    std::vector<std::unique_ptr<PlayerbotOperation>> ready;
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(delayedMutex);
        while (!delayed.empty() && delayed.begin()->first <= now && ready.size() < 32)
        {
            auto next = delayed.begin();
            ready.push_back(std::move(next->second));
            delayed.erase(next);
        }
    }
    for (auto& operation : ready)
    {
        auto id = operation->GetBotGuid().GetCounter();
        auto* request = static_cast<GuildOperation*>(operation.get());
        auto* bot = ObjectAccessor::FindPlayer(operation->GetBotGuid());
        if (bot && !request->Expired() && (bot->IsInCombat() || bot->IsNonMeleeSpellCast(false) ||
                                          bot->IsInFlight() || bot->IsBeingTeleported()))
        {
            bool rescheduled = false;
            {
                std::lock_guard lock(delayedMutex);
                if (delayed.size() < 4096)
                {
                    delayed.emplace(now + std::chrono::seconds(2), std::move(operation));
                    rescheduled = true;
                }
            }
            if (!rescheduled)
                Manager::Instance().Report(id, {Goal::Socialize, Outcome::Blocked,
                                                "guild or arena response could not be delayed during an activity"});
            continue;
        }
        if (!PlayerbotWorldThreadProcessor::instance().QueueOperation(std::move(operation)))
            Manager::Instance().Report(id, {Goal::Socialize, Outcome::Blocked,
                                            "delayed guild or arena response could not enter the world operation queue"});
    }
}

std::string GuildTaskStatistics()
{
    std::lock_guard lock(delayedMutex);
    return " delayed_membership_responses=" + std::to_string(delayed.size());
}
}
