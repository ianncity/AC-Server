# Current implementation ownership

The user assigned the main chat the remaining Playerbot ADVAI behavior plan and is
assigning Ascension porting to another agent. Both workstreams share this checkout.

The main chat is editing the ordinary ADVAI model, planner, conversation, economy,
profession, lifecycle and population code; the English catalogs; and acceptance tools.
The current batch adds `AdvaiMarket.*`, `AdvaiTravel.*`, grounded item/spell/profession
conversation, and state format ten. It adds market script registration in
`src/Script/Playerbots.cpp`, ordinary group/raid/guild operations, native enchanting,
earned mount upgrades, request expiry, and compact offline profiles. Shared factory
edits batch initial character saves; shared `PlayerbotAI` edits guard physical
transport attachment and add bounded solo hesitation. Preserve these shared edits.
`AdvaiGuild.*` now dispatches guild charter actions to the world thread; shared
`Playerbots.cpp` also preserves native account signature restrictions for ADVAI.
State format ten retains interrupted activities and craft material reservations
across maintenance and restarts, and associates satisfaction with the current guild.
It reads formats two through nine as well. Negotiated trades, gifts and accepted
group commitments also retain the previous personal activity. Required spell-focus
stations use legitimate travel, and membership responses use bounded delayed queues.
Crafting can acquire missing materials through native gathering in the current zone;
maintenance retrieves needed owned bank supplies before purchasing replacements.
Arena preparation now uses paid native charters and signatures. The main chat's
factory guard removes ADVAI from direct arena assignment and rating manufacture;
the arena acceptance and guild-bank actions also dispatch through `AdvaiGuild`.
The latest chat/PvP batch adds `AdvaiPvp.*`, observable target scoring, brief allied
target suggestions, hurt/outnumbered regrouping, and tactical speech during combat.
`EnemyPlayerValue`, `AttackAction`, `BattleGroundTactics`, `BattlegroundStrategy`
and `ActionContext` contain its integration; preserve these shared edits. Three new
outcomes were introduced with 36 English lines each. The subsequent expansion adds
24 lines to every outcome: 55 outcomes / 60 variants each / 3,300 authored lines.
Arena and battleground replies now include actual team/rating and match-state facts.
The PvP catalog migration is `2026_10_17_04_advai_pvp_conversation.sql`; the complete
expanded catalog is `2026_10_17_05_advai_expanded_english.sql`. Phrase selection now
retains 96 recent line identities and excludes recent eligible lines up to that
bound, rather than limiting exclusion to 12 per outcome. Replies to questions and
later conversation turns favor declarative variants. Gamer slang is weighted with
the same stable speech-style eligibility as the existing casual vocabulary.
This batch has not been compiled or exercised in the running realm.
`AdvaiMannerisms.cpp` now adds occasional personality-dependent outdoor travel hops
using native physical movement. It validates flat terrain, water, LOS and the short
path, retains the original ground destination and resumes it after landing. Native
movement effects retain scheduler activity. Human control, groups, combat, vehicles,
mounts, water and precise approaches are excluded. GM diagnostics report travel hop
count and the next gesture opportunity. This source also awaits native acceptance.
The movement implementation now adds `AdvaiMovement.*`: continuous native splines
over validated corridors, stable cornering styles, route retention, four familiar
route records, visible-threat costs, two native detour candidates, movement-speed
updates, brief contextual reading pauses and bounded local recovery. It integrates
through `MovementActions`, `NewRpgBaseAction`, the manager and `AdvaiMannerisms`.
Stationary world-object approaches use stable angles and visible crowd spacing;
nearby vendor/trainer/bank trips use that approach path. `advai pvp footwork` is
registered for battleground and arena movement with real paths, target facing,
short ranged sidesteps/retreats and incremental melee positioning. Native role,
casting, flag and human-control checks remain authoritative. All of these movement
changes are source-only pending the deferred native build and visual acceptance.

The porting workstream owns `src/Advai/Ascension/`, Ascension compiler/import tools,
Ascension catalog/header/SQL, matching client/server bundles, and the associated
PlayerbotCore spell/class mechanics. It should finish carrier resource/stat/equipment
integration, complete class rotations, explicit conversion of existing profiles, and
native class acceptance described in `COVERAGE.md`.

The supplied client path is `E:/Ascension/Ascension`; the user supplied that path.
The actual data root is `resources/ascension-live` beneath it. Existing extracted
source and staging artifacts are under `PlayerbotCore/var/advai-ascension-*`.

The user stopped compilation and requested continued implementation. The module
build was stopped; continue implementation before compilation or runtime validation.

Do not overwrite or reset shared changes. Coordinate runtime restarts and installation
of client/server bundles before replacing live data. The existing isolated realm
uses world port 18085, auth 13724, and MySQL 13316.
Full Ascension classes are currently disabled in that realm. Builds must use Release;
`start.py` records executable paths and hashes in the process manifest.
