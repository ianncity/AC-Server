# Debian deployment notes (bots / classes pass, 2026-11-05)

Status: **module code compiles on Linux** (Ubuntu 24.04, GCC 13.3, Boost 1.83, Ninja, Release): all 931 module translation units,
zero errors, after one fix (AdvaiLlm.cpp needs `<boost/bind/placeholders.hpp>` on Boost < 1.85). The core `worldserver` link and a run were NOT done
on Linux, and Debian 12 (GCC 12, Boost 1.74) was not tried. Build the full worldserver on the Debian host before relying on this.

## Configuration (the part that bit us on Windows)
- The module config is only read from `<config dir>/modules/playerbots.conf` (or `.conf.dist` is NOT used as a
  fallback for values). On the Windows stack the file the server loads is
  `bin/RelWithDebInfo/configs/modules/playerbots.conf`; the copy under `advai-gameplay/configs/modules/` was never read.
  On Linux put it in `etc/modules/playerbots.conf` next to `worldserver.conf`, or pass `-c` and keep `modules/` beside it.
- `PlayerbotsDatabaseInfo` is defined in worldserver.conf on this stack; if playerbots.conf also defines it, the module
  file wins. Keep exactly one definition.
- Required AdvAI keys: `Playerbots.AdvAI.Enabled=1`, `Dracthyr=1`, `AscensionClasses=1`, plus the preset
  `conf/playerbots.advai.preset`. `Playerbots.AdvAI.MemoryLimitMB` now accepts up to 28000 (was 10000).
  On Linux the limit is enforced differently from the Windows job object; check `Metrics::LimitMemory`.

## Sizing (measured on i7-12700K, 20 threads)
| Bots | Map threads | Mean diff | p95 diff | CPU cores | Private RAM |
|---|---|---|---|---|---|
| 500 | 1 | 34.7 ms | 105 ms | 1.0 | 5.9 GB |
| 500 | 6 | 8.4 ms (3 ms after warm-up) | 23 ms | 2.5 | 5.5 GB |
| 2000 | 6 | 16 ms (10-11 ms first 9 min) | 39 ms | 3.2 | 7.2 GB |

- `MapUpdate.Threads = 6` is required; with 1 thread the world loop is the bottleneck at ~500 bots.
- Plan for ~4 GB base + ~2 MB per online bot, 4+ cores. A 1500-2000 bot realm fits in 16 GB.
- Login storm: all bots logging in at once produces multi-second diffs for ~30 s. Keep `RandomBotsPerInterval` modest.

## Known data/db issues to watch
- Character DB deadlocks (error 1213) on pet/spell saves under heavy bot churn (~13 per 20 min at 500 bots).
- `Logger.network=5` writes every packet to Server.log; set to 2 on production.
- SpellShapeshiftForm.dbc lacks forms 50, 53, 56, 59 used by spells 504691, 803183, 524888/804216, 704471. The server
  now converts those auras to dummy markers at startup (mod-necromancer ApplyContracts). The client DBC still lacks them.

## Test harness (GM console / RA)
`.ascbench metrics on|off|reset|dump`, `.ascbench login <level> [perClass] [classId]`,
`.ascbench spells <class> [catalog] [level=N] [window=ms]`, `.ascbench moves [reset]`.
Spell-probe damage numbers are NOT reliable yet (cast OK, damage always 0); cast failures, summons and auras are.

## Changes in this pass (all in module code; nothing in core or live SQL)
- Hook exception guards (`modules/AscensionHookGuard.h`, `mod-playerbots/src/Script/PlayerbotsHookGuard.h`).
- SHAPESHIFT auras with a form missing from SpellShapeshiftForm.dbc become dummy auras at startup (mod-necromancer ApplyContracts).
- AdvAI: corrupt stored profile is quarantined instead of aborting boot; profiles load in compact form (startup RAM peak 5.2 GB -> 3.0 GB);
  memory-limit ceiling 10000 -> 28000 MB; per-bot class-action cache; summons dismount/stop first (they failed 100% while mounted).
- Cultist/Necromancer bot casting: filter no longer drops trainer attacks/heals; target fallback in combat.
- Necromancer minions: weapon floor x1.6 (80) .. x4.0 (0) and attack-power coefficient x1.0 (0) .. x2.25 (80). Measured before: 15-40% of stock median.
- Humanlike movement: per-persona habit + drifting mood, overshoot/undershoot, arc starts, wider corner smoothing, idle glances.
- `.ascbench` GM/RA command for metrics, login by level, spell probe, movement variety, bot casting funnel.
