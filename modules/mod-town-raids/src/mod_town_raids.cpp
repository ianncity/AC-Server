#include "ScriptMgr.h"
#include "../../AscensionHookGuard.h"
#include "ScriptedCreature.h"
#include "WorldScript.h"
#include "Chat.h"
#include "Config.h"
#include "Random.h"
#include "TownRaidMgr.h"
#include <charconv>
#include <sstream>
#include <string>

static bool ParseTownRaidId(std::string const& token, uint32& townId)
{
    uint32 parsed = 0;
    auto const result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size())
        return false;

    townId = parsed;
    return true;
}

static bool ParseTownRaidTheme(std::string const& token, int32& themeId)
{
    int32 parsed = 0;
    auto const result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
        parsed < 0 || parsed >= static_cast<int32>(RaidTheme::MaxThemes))
        return false;

    themeId = parsed;
    return true;
}

using namespace Acore::ChatCommands;

static bool IsTownRaidsEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("TownRaids.Enabled", true);
    return enabled;
}

class town_raid_invader : public CreatureScript
{
public:
    town_raid_invader() : CreatureScript("town_raid_invader") {}

    // These are the spells used by equivalent WotLK (VerifiedBuild 12340)
    // NPCs in the local world database: Felhound Tracker 8668, Firebrand
    // Pyromancer 9264, Skeletal Frostweaver 7341, Dark Strand Cultist 3725,
    // Southsea Cannoneer 3382, and Hive'Regal Spitfire 11732.
    static uint32 GetRaidSpell(uint32 entry)
    {
        switch (entry)
        {
            case 97002: return 2691;  // Mana Burn
            case 97012: return 15096; // Flame Shock
            case 97021: return 9672;  // Frostbolt
            case 97031: return 9613;  // Shadow Bolt
            case 97041: return 6660;  // Shoot
            case 97051: return 21047; // Corrosive Acid Spit
            default:    return 0;
        }
    }

    struct town_raid_invaderAI : public ScriptedAI
    {
        explicit town_raid_invaderAI(Creature* creature) : ScriptedAI(creature), _spellTimer(2500) {}

        void JustDied(Unit* /*killer*/) override
        {
            sTownRaidMgr->OnInvaderDeath(me->GetGUID());
        }

        void MoveInLineOfSight(Unit* who) override
        {
            // Invaders march toward the town center until they encounter a
            // hostile defender. Let normal faction/LOS checks pick targets.
            if (!me->GetVictim() && me->CanCreatureAttack(who))
            {
                if (GetRaidSpell(me->GetEntry()))
                    AttackStartCaster(who, 15.0f);
                else
                    AttackStart(who);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            uint32 spellId = GetRaidSpell(me->GetEntry());
            if (spellId)
            {
                if (_spellTimer <= diff)
                {
                    if (DoCastVictim(spellId) == SPELL_CAST_OK)
                        _spellTimer = urand(5000, 8000);
                    else
                        _spellTimer = 500;
                }
                else
                    _spellTimer -= diff;
            }

            DoMeleeAttackIfReady();
        }

    private:
        uint32 _spellTimer;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new town_raid_invaderAI(creature);
    }
};

class TownRaidWorldScript : public WorldScript
{
public:
    TownRaidWorldScript() : WorldScript("TownRaidWorldScript") {}

    void OnStartup() override
    {
        if (!IsTownRaidsEnabled())
            return;

        sTownRaidMgr->Initialize();
    }

    void OnUpdate(uint32 diff) override
    {
        ASCENSION_GUARD("town_raids::OnUpdate", [&] { UpdateImpl(diff); });
    }

    void UpdateImpl(uint32 diff)
    {
        if (!IsTownRaidsEnabled())
            return;

        sTownRaidMgr->Update(diff);
    }
};

class TownRaidCommandScript : public CommandScript
{
public:
    TownRaidCommandScript() : CommandScript("TownRaidCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable townRaidSubCommands =
        {
            { "start", HandleTownRaidStart, SEC_GAMEMASTER, Console::No },
            { "stop",  HandleTownRaidStop,  SEC_GAMEMASTER, Console::No },
            { "list",  HandleTownRaidList,  SEC_GAMEMASTER, Console::No },
            { "",      HandleTownRaidList,  SEC_GAMEMASTER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "townraid", townRaidSubCommands }
        };

        return commandTable;
    }

    static bool HandleTownRaidStart(ChatHandler* handler, const char* args)
    {
        if (!IsTownRaidsEnabled())
        {
            handler->SendSysMessage("Town raids are disabled.");
            return true;
        }
        if (!*args)
        {
            handler->SendSysMessage("Syntax: .townraid start <townId> [themeId 0=Demons, 1=Bandits, 2=Scourge, 3=Twilight, 4=Pirates, 5=Silithid]");
            return true;
        }

        std::string townIdToken;
        std::string themeIdToken;
        std::string extraToken;
        std::istringstream arguments(args);
        uint32 townId = 0;
        int32 themeId = -1;

        if (!(arguments >> townIdToken) || !ParseTownRaidId(townIdToken, townId))
        {
            handler->SendSysMessage("Invalid town ID. Use: .townraid start <townId> [themeId 0-5]");
            return true;
        }

        if (arguments >> themeIdToken)
        {
            if (!ParseTownRaidTheme(themeIdToken, themeId) || (arguments >> extraToken))
            {
                handler->SendSysMessage("Invalid raid theme or extra arguments. Theme IDs are 0-5.");
                return true;
            }
        }

        if (sTownRaidMgr->StartRaid(townId, themeId))
        {
            handler->PSendSysMessage("|cff00ff00[Town Raids]|r Raid successfully initiated for town {}!", townId);
        }
        else
        {
            handler->PSendSysMessage("|cffff0000[Town Raids]|r Failed to start raid (already active or invalid town ID).");
        }
        return true;
    }

    static bool HandleTownRaidStop(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("Syntax: .townraid stop <townId>");
            return true;
        }

        std::string townIdToken;
        std::string extraToken;
        std::istringstream arguments(args);
        uint32 townId = 0;
        if (!(arguments >> townIdToken) || !ParseTownRaidId(townIdToken, townId) || (arguments >> extraToken))
        {
            handler->SendSysMessage("Invalid town ID. Use: .townraid stop <townId>");
            return true;
        }
        if (sTownRaidMgr->StopRaid(townId))
        {
            handler->PSendSysMessage("|cff00ff00[Town Raids]|r Active raid on town {} has been concluded.", townId);
        }
        else
        {
            handler->PSendSysMessage("|cffff0000[Town Raids]|r No active raid found for town {}.", townId);
        }
        return true;
    }

    static bool HandleTownRaidList(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("=== Town Raids: Active Hubs & Status ===");
        for (TownDefinition const& town : sTownRaidMgr->GetTowns())
        {
            bool active = sTownRaidMgr->IsRaidActive(town.Id);
            uint32 timerSec = sTownRaidMgr->GetTownTimer(town.Id);
            uint32 hours = timerSec / 3600;
            uint32 mins = (timerSec % 3600) / 60;

            const char* scaleStr = (town.Scale == TownScale::Capital ? "Capital" : (town.Scale == TownScale::Medium ? "Medium" : "Small"));

            if (active)
            {
                handler->PSendSysMessage("[{}] {} ({}) - |cffff2020UNDER RAID INVASION!|r", town.Id, town.Name, scaleStr);
            }
            else
            {
                handler->PSendSysMessage("[{}] {} ({}) - Next Raid in: {:02}h {:02}m", town.Id, town.Name, scaleStr, hours, mins);
            }
        }
        return true;
    }
};

void AddSC_mod_town_raids()
{
    new town_raid_invader();
    new TownRaidWorldScript();
    new TownRaidCommandScript();
}

void Addmod_town_raidsScripts()
{
    AddSC_mod_town_raids();
}
