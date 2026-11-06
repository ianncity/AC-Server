#include <algorithm>
#include <charconv>
#include <exception>
#include <string>
#include <string_view>

#include "AscensionProgression.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

using namespace Acore::ChatCommands;
using namespace Advai::Ascension;
using namespace Advai::Ascension::Progression;

namespace
{
constexpr uint32 ClassId = CultistClass;

Player* GetPlayer(ChatHandler* handler)
{
    return handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
}

// Progression validators throw on inconsistent character data. An escaped exception from a
// login hook or chat command terminates the worldserver, so contain it to the one character.
void ReportProgressionFailure(Player* player, char const* context, std::exception const& error)
{
    LOG_ERROR("module", "Cultist progression {} failed for {} (GUID {}): {}", context,
        player ? player->GetName() : std::string("<none>"), player ? player->GetGUID().GetCounter() : 0, error.what());
    if (player && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage("Cultist progression data for this character needs repair. Please report it to a GM.");
}

template <auto Handler, typename... Args>
bool Guarded(ChatHandler* handler, Args... args)
{
    try
    {
        return Handler(handler, args...);
    }
    catch (std::exception const& error)
    {
        ReportProgressionFailure(GetPlayer(handler), "command", error);
        return true;
    }
}

uint32 ParseSpec(std::string_view value)
{
    struct SpecName
    {
        std::string_view Name;
        uint32 Id;
    };
    constexpr std::array<SpecName, 4> names = {{ { "influence", 40 }, { "corruption", 41 }, { "godblade", 42 }, { "bulwark", 96 } }};
    for (SpecName const& name : names)
    {
        if (value == name.Name)
        {
            return name.Id;
        }
    }
    uint32 id = 0;
    auto result = std::from_chars(value.data(), value.data() + value.size(), id);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? id : 0;
}

void SendResult(ChatHandler* handler, Result result)
{
    switch (result)
    {
        case Result::Success: handler->SendSysMessage("Your Cultist advancement was recorded."); break;
        case Result::WrongClass: handler->SendSysMessage("Cultist commands require a Cultist character."); break;
        case Result::AlreadyInitialized: handler->SendSysMessage("This character is already initialized as a Cultist."); break;
        case Result::NotInitialized: handler->SendSysMessage("Use .cultist init first on a Cultist character."); break;
        case Result::SelectSpecializationFirst: handler->SendSysMessage("Reach level 10 and choose a Cultist specialization first."); break;
        case Result::InvalidSpecialization: handler->SendSysMessage("Choose influence, corruption, godblade, bulwark, or a listed spec ID."); break;
        case Result::SpecializationLocked: handler->SendSysMessage("Cultist specializations unlock at level 10."); break;
        case Result::SpecializationChangeLocked: handler->SendSysMessage("Use .cultist reset before changing specialization."); break;
        case Result::SpecializationAlreadyChosen: handler->SendSysMessage("You already chose that Cultist specialization."); break;
        case Result::InvalidNode: handler->SendSysMessage("That advancement node does not exist."); break;
        case Result::WrongClassNode: handler->SendSysMessage("That node belongs to another class."); break;
        case Result::WrongSpecialization: handler->SendSysMessage("That node belongs to another Cultist specialization."); break;
        case Result::LevelRequired: handler->SendSysMessage("Your level is too low for that node."); break;
        case Result::PrerequisiteRequired: handler->SendSysMessage("Learn the node's prerequisites first."); break;
        case Result::ChoiceLocked: handler->SendSysMessage("That mutually exclusive choice is already learned."); break;
        case Result::NoAdvancementPoints: handler->SendSysMessage("You do not have enough AE or TE at your current level."); break;
        case Result::RankComplete: handler->SendSysMessage("That node has no further ranks."); break;
        case Result::MissingSpell: handler->SendSysMessage("The server spell data is missing; check the Ascension client DBC port."); break;
        case Result::InitializationFailed: handler->SendSysMessage("The Cultist starter kit could not be learned or placed on the action bar."); break;
    }
}

class cultist_progression_commands final : public CommandScript
{
public:
    cultist_progression_commands() : CommandScript("cultist_progression_commands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commands =
        {
            { "init", Guarded<&HandleInit>, 0, Console::No },
            { "spec", Guarded<&HandleSpec, std::string_view>, 0, Console::No },
            { "status", Guarded<&HandleStatus>, 0, Console::No },
            { "list", Guarded<&HandleList, Optional<uint32>>, 0, Console::No },
            { "learn", Guarded<&HandleLearn, uint32>, 0, Console::No },
            { "reset", Guarded<&HandleReset>, 0, Console::No },
            { "", HandleHelp, 0, Console::No }
        };
        static ChatCommandTable root = { { "cultist", commands } };
        return root;
    }

    static bool HandleInit(ChatHandler* handler)
    {
        SendResult(handler, Initialize(GetPlayer(handler), ClassId));
        return true;
    }

    static bool HandleSpec(ChatHandler* handler, std::string_view value)
    {
        uint32 specId = ParseSpec(value);
        Result result = specId ? SelectSpecialization(GetPlayer(handler), ClassId, specId) : Result::InvalidSpecialization;
        SendResult(handler, result);
        if (result == Result::Success)
        {
            LearnFreePassives(GetPlayer(handler), ClassId);
            handler->SendSysMessage("Use .cultist status and .cultist list to spend your level-based advancement points.");
        }
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player = GetPlayer(handler);
        if (!player || Class(player) != ClassId)
        {
            handler->SendSysMessage("Use .cultist init first on a Cultist character.");
            return true;
        }
        uint32 spec = Specialization(player);
        if (!spec)
        {
            handler->SendSysMessage(player->GetLevel() < 10 ? "Choose a Cultist specialization at level 10." :
                "Choose a specialization: influence (40), corruption (41), godblade (42), or bulwark (96).");
            return true;
        }
        Budget budget = EarnedBudget(player);
        Spent spent = PointsSpent(player);
        handler->SendSysMessage("Cultist level " + std::to_string(player->GetLevel()) + ", specialization " + std::to_string(spec) +
            ". AE available: " + std::to_string(budget.AE - spent.AE) + ". TE available: " + std::to_string(budget.TE - spent.TE) + ".");
        return true;
    }

    static bool HandleList(ChatHandler* handler, Optional<uint32> requestedPage)
    {
        Player* player = GetPlayer(handler);
        if (!player || Class(player) != ClassId)
        {
            handler->SendSysMessage("Use .cultist init first on a Cultist character.");
            return true;
        }
        uint32 spec = Specialization(player);
        if (!spec)
        {
            handler->SendSysMessage("Choose a specialization at level 10 before browsing advancement nodes.");
            return true;
        }
        Budget budget = EarnedBudget(player);
        Spent spent = PointsSpent(player);
        uint32 page = requestedPage ? std::max(1u, requestedPage.value()) : 1;
        auto canList = [player, spec, budget, spent](Data::Node const& node)
        {
            if (node.Class != ClassId || IsIdentityNode(node.Id) || (node.Spec && node.Spec != spec) ||
                !Ready(player, node, spec) || !ChoiceAvailable(player, node))
            {
                return false;
            }
            uint32 rank = Rank(player, node);
            if (rank >= node.Ranks.size() || !node.Ranks[rank])
            {
                return false;
            }
            uint32 costAE = node.Spec ? 0 : node.AE;
            uint32 costTE = node.Spec ? node.TE : 0;
            return spent.AE + costAE <= budget.AE && spent.TE + costTE <= budget.TE;
        };
        uint32 total = uint32(std::count_if(Data::Nodes.begin(), Data::Nodes.end(), canList));
        uint32 pageCount = std::max(1u, (total + 29) / 30);
        if (page > pageCount)
        {
            handler->SendSysMessage("That page is outside the available list. Use .cultist list <page> (1-" +
                std::to_string(pageCount) + ").");
            return true;
        }
        uint32 first = (page - 1) * 30;
        uint32 index = 0;
        uint32 listed = 0;
        for (Data::Node const& node : Data::Nodes)
        {
            if (!canList(node))
            {
                continue;
            }
            if (index++ < first)
            {
                continue;
            }
            uint32 rank = Rank(player, node);
            uint32 costAE = node.Spec ? 0 : node.AE;
            uint32 costTE = node.Spec ? node.TE : 0;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(node.Ranks[rank]);
            std::string name = info && info->SpellName[0] ? info->SpellName[0] : "Ascension spell";
            handler->SendSysMessage("Node " + std::to_string(node.Id) + " rank " + std::to_string(rank + 1) + ": " + name +
                " (AE " + std::to_string(costAE) + ", TE " + std::to_string(costTE) + ")");
            if (++listed == 30)
            {
                break;
            }
        }
        if (!listed)
        {
            handler->SendSysMessage("No learnable nodes are available at your current level and point balance.");
        }
        handler->SendSysMessage("Page " + std::to_string(page) + " of " + std::to_string(pageCount) +
            ". Use .cultist list <page> to browse.");
        return true;
    }

    static bool HandleLearn(ChatHandler* handler, uint32 nodeId)
    {
        SendResult(handler, LearnNode(GetPlayer(handler), ClassId, nodeId));
        return true;
    }

    static bool HandleReset(ChatHandler* handler)
    {
        Reset(GetPlayer(handler), ClassId);
        handler->SendSysMessage("Cultist advancement ranks and specialization were reset. Your Cultist identity remains.");
        return true;
    }

    static bool HandleHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("Cultist: .cultist init | spec <name|id> | status | list [page] | learn <nodeId> | reset");
        handler->SendSysMessage("Specializations: influence (40), corruption (41), godblade (42), bulwark (96). Spec selection opens at level 10.");
        return true;
    }
};

class cultist_progression_player final : public PlayerScript
{
public:
    cultist_progression_player() : PlayerScript("cultist_progression_player") { }

    void OnPlayerLogin(Player* player) override
    {
        try
        {
            if (!player || Class(player) != ClassId)
            {
                return;
            }
            Result initialized = Initialize(player, ClassId);
            if (initialized != Result::Success)
            {
                ChatHandler(player->GetSession()).SendSysMessage("Cultist starter kit initialization failed. Check the server spell data and action bar state.");
                return;
            }
            LearnFreePassives(player, ClassId);
            ChatHandler(player->GetSession()).SendSysMessage("Cultist class active. Use .cultist status, .cultist list, and .cultist learn <nodeId>.");
        }
        catch (std::exception const& error)
        {
            ReportProgressionFailure(player, "login", error);
        }
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (player && Class(player) == ClassId)
        {
            try
            {
                LearnFreePassives(player, ClassId);
                ChatHandler(player->GetSession()).SendSysMessage("New Cultist advancement points are available. Use .cultist status and .cultist list.");
            }
            catch (std::exception const& error)
            {
                ReportProgressionFailure(player, "level-up", error);
            }
        }
    }
};
}

void AddCultistProgressionScripts()
{
    new cultist_progression_commands();
    new cultist_progression_player();
}
