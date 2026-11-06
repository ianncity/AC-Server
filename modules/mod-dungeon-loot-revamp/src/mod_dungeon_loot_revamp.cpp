#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"

class mod_dungeon_loot_revamp_worldscript : public WorldScript
{
public:
    mod_dungeon_loot_revamp_worldscript() : WorldScript("mod_dungeon_loot_revamp_worldscript") {}

    void OnStartup() override
    {
        LOG_INFO("server.loading", ">> Loaded Module: Vanilla+ Dungeon Loot Revamp (Hybrid & Relic Itemization Across Classic Dungeons)");
    }
};

void Addmod_dungeon_loot_revampScripts()
{
    new mod_dungeon_loot_revamp_worldscript();
}
