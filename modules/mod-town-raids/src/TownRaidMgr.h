#ifndef TOWN_RAID_MGR_H
#define TOWN_RAID_MGR_H

#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>

enum class TownScale : uint8
{
    Small = 0,   // ~5-8 units (e.g., Goldshire, Razor Hill, Brill, Sentinel Hill)
    Medium,      // ~10-15 units (e.g., Crossroads, Darkshire, Southshore, Tarren Mill, Gadgetzan)
    Capital      // ~24-30 units + Alpha Boss (e.g., Stormwind, Orgrimmar, Ironforge, Undercity)
};

enum class RaidTheme : uint8
{
    Demons = 0,
    BanditsDefias,
    Scourge,
    TwilightHammer,
    Pirates,
    SilithidSwarm,
    MaxThemes
};

struct TownDefinition
{
    uint32 Id;
    std::string Name;
    uint32 MapId;
    uint32 ZoneId;
    TownScale Scale;
    uint8 MinLevel;
    uint8 MaxLevel;
    Position RoadSpawn;
    Position TownCenter;
};

struct ActiveRaidInstance
{
    uint32 TownId;
    RaidTheme Theme;
    uint32 ElapsedMs;
    uint32 MaxDurationMs;
    std::list<ObjectGuid> SpawnedInvaders;
    bool IsRetreating;
};

class TownRaidMgr
{
public:
    static TownRaidMgr* instance();

    void Initialize();
    void Update(uint32 diff);
    void OnInvaderDeath(ObjectGuid const& invaderGuid);
    bool StartRaid(uint32 townId, int32 forceTheme = -1);
    bool StopRaid(uint32 townId);
    
    std::vector<TownDefinition> const& GetTowns() const { return _towns; }
    uint32 GetTownTimer(uint32 townId) const;
    bool IsRaidActive(uint32 townId) const;
    std::string GetThemeName(RaidTheme theme) const;

private:
    TownRaidMgr() : _checkTimer(0) {}

    void LoadTownDefinitions();
    void ResetTownTimer(uint32 townId);
    void SpawnInvadersForTown(TownDefinition const& town, RaidTheme theme, ActiveRaidInstance& raid);
    void CleanupRaid(uint32 townId, bool defeatedByDefenders);
    void AnnounceRaid(TownDefinition const& town, RaidTheme theme, bool started, bool defeated);

    std::vector<TownDefinition> _towns;
    std::unordered_map<uint32, uint32> _townTimers; // TownId -> remaining seconds
    std::unordered_map<uint32, ActiveRaidInstance> _activeRaids; // TownId -> ActiveRaid
    std::mutex _deathMutex;
    std::vector<ObjectGuid> _pendingDeaths;
    uint32 _checkTimer;
};

#define sTownRaidMgr TownRaidMgr::instance()

#endif // TOWN_RAID_MGR_H
