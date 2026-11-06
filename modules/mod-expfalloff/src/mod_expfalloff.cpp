#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Config.h"
#include <cmath>

using namespace Acore::ChatCommands;

class mod_expfalloff_playerScript : public PlayerScript
{
public:
    mod_expfalloff_playerScript() : PlayerScript("mod_expfalloff_playerScript") {}

    static float CalculateXPMultiplier(uint32 playerLevel, uint32 startLevel, float startSlowdown, float maxSlowdown, uint32 maxLevel)
    {
        if (playerLevel <= startLevel)
            return 1.0f;

        // Steps above startLevel: Level 71 is step 0, Level 79 is step 8 (for maxLevel 80)
        uint32 levelsAbove = playerLevel - startLevel;
        uint32 levelsUntilCap = maxLevel > startLevel ? maxLevel - startLevel : 0;
        uint32 totalRange = levelsUntilCap > 2 ? levelsUntilCap - 2 : 1;

        float progress = float(levelsAbove - 1) / float(totalRange);
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        // A non-positive configured slowdown would make the ratio below divide by zero or produce NaN,
        // and the NaN/inf multiplier then overflows the uint32 XP cast in OnPlayerGiveXP.
        if (!(startSlowdown > 0.0f) || !(maxSlowdown > 0.0f))
            return 1.0f;

        // Exponential slowdown between startSlowdown (2.0x slower = 0.5x) and maxSlowdown (4.0x slower = 0.25x)
        float currentSlowdown = startSlowdown * std::pow(maxSlowdown / startSlowdown, progress);
        if (!(currentSlowdown > 0.0f))
            return 1.0f;

        return 1.0f / currentSlowdown;
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (!sConfigMgr->GetOption<bool>("ExpFalloff.Enable", true) || !player || amount == 0)
            return;

        uint32 startLevel = sConfigMgr->GetOption<uint32>("ExpFalloff.StartLevel", 70);
        uint32 playerLevel = player->GetLevel();

        if (playerLevel <= startLevel)
            return;

        float startSlowdown = sConfigMgr->GetOption<float>("ExpFalloff.StartSlowdown", 2.0f);
        float maxSlowdown   = sConfigMgr->GetOption<float>("ExpFalloff.MaxSlowdown", 4.0f);
        uint32 maxLevel     = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

        float multiplier = CalculateXPMultiplier(playerLevel, startLevel, startSlowdown, maxSlowdown, maxLevel);

        uint32 newAmount = static_cast<uint32>(std::round(amount * multiplier));
        if (newAmount < 1 && amount > 0)
            newAmount = 1;

        amount = newAmount;
    }
};

class expfalloff_commandscript : public CommandScript
{
public:
    expfalloff_commandscript() : CommandScript("expfalloff_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable expCommandTable =
        {
            { "rate", HandleExpRateCommand, SEC_PLAYER, Console::No },
            { "",     HandleExpRateCommand, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "exprate", expCommandTable },
            { "expfalloff", expCommandTable }
        };

        return commandTable;
    }

    static bool HandleExpRateCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("ExpFalloff.Enable", true))
        {
            handler->SendSysMessage("EXP falloff is disabled; you receive the server's configured XP rates.");
            return true;
        }

        uint32 startLevel   = sConfigMgr->GetOption<uint32>("ExpFalloff.StartLevel", 70);
        uint32 playerLevel  = player->GetLevel();
        uint32 maxLevel     = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        float startSlowdown = sConfigMgr->GetOption<float>("ExpFalloff.StartSlowdown", 2.0f);
        float maxSlowdown   = sConfigMgr->GetOption<float>("ExpFalloff.MaxSlowdown", 4.0f);

        float mult = mod_expfalloff_playerScript::CalculateXPMultiplier(playerLevel, startLevel, startSlowdown, maxSlowdown, maxLevel);

        handler->PSendSysMessage("|cff00ffff=== EXP Falloff Progression ===|r");
        handler->PSendSysMessage("Levels 1-{}: |cff00ff00100%|r of server configured XP rate.", startLevel);
        if (maxLevel > startLevel && maxLevel - startLevel > 2)
            handler->PSendSysMessage("Levels {}-{}: XP progresses from {:.2f}x slower to {:.2f}x slower.",
                startLevel + 1, maxLevel - 1, startSlowdown, maxSlowdown);
        else if (maxLevel > startLevel && maxLevel - startLevel == 2)
            handler->PSendSysMessage("Level {}: {:.2f}x slower.", startLevel + 1, startSlowdown);
        else
            handler->SendSysMessage("There are no earning levels above the configured falloff start.");
        handler->PSendSysMessage("Your Level ({}) current rate: |cff00ff00{:.1f}%|r ({:.2f}x slower).",
            playerLevel, mult * 100.0f, (mult > 0.0f ? 1.0f / mult : 1.0f));

        return true;
    }
};

void AddSC_mod_expfalloff()
{
    new mod_expfalloff_playerScript();
    new expfalloff_commandscript();
}

void Addmod_expfalloffScripts()
{
    AddSC_mod_expfalloff();
}
