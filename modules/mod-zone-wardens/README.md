# Zone Wardens

Zone Wardens adds 147 custom rare creatures to 71 open-world questing areas across Eastern Kingdoms, Kalimdor, Outland, and Northrend. The roster includes all eight playable starter pockets. Cities, dungeons, raids, and non-questing interior areas are excluded.

## Encounter design

- Every area has one alpha rare. The normal level is the area's recommended level-range midpoint, rounded to the nearest integer with halves rounded upward. Each alpha is exactly ten levels above that midpoint; the level 80 zone therefore gets a level 90 alpha.
- Alpha rares are the roaming encounters by default. Each uses an existing native waypoint circuit in its zone, enlarged display scale, the 3.3.5 Gigantic visibility distance, a zone-specific aggro yell, rare-elite rank, and three SmartAI abilities. Its third ability triggers below 35% health.
- Fourteen alphas use localized patrols: the eight starter pockets stay inside their level-appropriate enclave, and Deadwind Pass, Moonglade, Zangarmarsh, Netherstorm, Isle of Quel'Danas, and Hrothgar's Landing each use a route suited to their terrain. The roster records each reason and route.
- Normal rares vary by zone. They use stationary, short-wander, or localized-patrol movement, with distinct names, level-matched zone-theme creature models, spell profiles, timers, and rare rank. Every appearance records its actual 3.3.5 model file in the roster, and no model file repeats within the same zone.
- Respawn timers vary with zone level and creature type. Loot references the midpoint's existing level-appropriate world loot and can include one material already dropped by a creature in that zone. No items or client display assets are added.

The roster uses 147 distinct creature model files across 147 rares: 10 giant models (8 alphas), 4 ogre models (3 alphas), and 18 dragon-family models (10 alphas). Every rare has a different mesh across all zones.

The complete roster, recommended ranges, rounded levels, existing model sources, displays, spell IDs, route IDs, spawn positions, respawn timers, rewards, and localized-route rationales are in [roster.json](roster.json).

## 3.3.5a compatibility

The server and patched client use the 3.3.5a data set. Every display ID was checked against the installed client's `CreatureDisplayInfo.dbc`; SmartAI spell IDs were checked against the server's 3.3.5 `Spell.dbc`; direct item rewards and shared loot references were checked against the checked-in world database. The module adds no client files, patches, or core API changes.

AzerothCore's checked-in creature stats have missing Wrath base health for levels 84 through 90. To keep the uncapped level 84-90 alpha rares functional, the module fills only missing `creature_classlevelstats` Wrath values for classes 1, 2, 4, and 8. The optional uninstall SQL restores the original checked-in values when those module values are still present.

## Install

1. Place this folder at `modules/mod-zone-wardens` in the AzerothCore source checkout.
2. Configure AzerothCore with `-DMODULES=static` (or the desired supported module linkage), then build `worldserver` using the server's normal build steps. The module exports the loader symbol through `src/mod_zone_wardens.cpp`; its encounter logic is database-driven.
3. Run the standard AzerothCore world database updater with this module enabled. It discovers `data/sql/db-world/20261016_00_mod_zone_wardens.sql` from the active module.
4. Restart `worldserver`. No client-side installation step is required.

## Remove

Disable the module in the build configuration and run [data/sql/uninstall/world.sql](data/sql/uninstall/world.sql) against the world database before removing this folder. It removes only the reserved creature, spawn, waypoint, SmartAI, text, model, and loot ranges, then conditionally restores the high-level stats modified by the module.
