# Playerbot ADVAI development integration

The implementation lives in this module. The supported integration checkout is
`E:\WowNewClient\PlayerbotCore`, branch `Playerbot`, with `modules/mod-playerbots`
linked to `E:\WowNewClient\playerbotadvai`. The complete behavior and acceptance
status is tracked in [COVERAGE.md](COVERAGE.md). Pending implementation and acceptance
items are release blockers.

## Configuration and retained population

Apply `conf/playerbots.advai.preset` with `configure.py` to a new configuration file.
The preset requests 2,000 online characters, activates human-occupied map/zone pairs,
and rotates a target of 100 distant characters every five minutes. Commitments can
temporarily exceed that target.

Long breaks require substantially more retained characters than online slots. With
the default ordinary tenure of 2-4 days, breaks of 14-30 days, and short sessions,
expected demand is approximately 12.1 identities per online slot. The factory
provisions a recommended reserve of 16 identities per slot; the default preset asks
for 3,250 generated accounts, including 50 accounts for the add-class pool. Accounts
outside the add-class pool are available for random login. Faction ratios and
Death Knight availability can increase the required account count. The reserve is
capacity planning, not a guarantee against every synchronized departure pattern;
dynamic provisioning and extreme-depletion acceptance remain open.

Every generated character, including offline reserves, receives a deterministic
GUID-derived personality. Initial identity persistence is staggered. Played
characters are adopted with their progress intact; initial resources are seeded
only for fresh characters. Reserve characters receive no simulated progression.

The isolated runtime generator computes this reserve automatically and rejects an
explicit account count below its minimum. It disables the optional bot command
server and uses separate database names and loopback ports. It never changes an
existing source configuration in place.

## Movement and peer conversation

With ADVAI enabled, eligible outdoor solo travel can use short, terrain-checked route drift,
rounded turns, occasional hops and a brief look behind while moving backwards. The movement
controller abandons a gesture when combat, a new route, or a changed activity takes priority.
`playerbots advai <bot-guid>` diagnostics include route drift, look-behind and hop counts. The
behaviors are deliberately restricted around transport, water, precise approaches and combat.

Bots can initiate bounded conversations with nearby bots using the rule catalog or the LLM
when bot LLM chat is enabled. Nearby pairs sometimes speak in /say; other exchanges use
whispers. Exchanges have a turn limit and a rising chance to end, while the per-zone channel
budget limits bot messages. Human whispers retain their existing direct-response behavior.
These timings and movement frequencies require in-game observation and calibration.

## Content gate

`python apps/advai/catalog.py` validates the required English speech and factual
catalogs. ADVAI uses English independently of the realm's DBC locale. Additional
languages are outside the user-approved scope. Missing English outcomes, variants,
placeholders or factual templates stop activation before population creation.

The catalog contains 44 English outcomes with 36 variants each, plus 71 factual
templates. `catalog.py --facts-output <migration>` compiles English factual text.
Queued replies retain their immutable catalog snapshot and render fresh gameplay
facts when delivered. The sender ledger binds canonical meaning to exact delivered
text for delayed bot-to-bot replies.

The preset disables legacy automatic broadcasts, invitation chatter and unowned
command chatter; conflicting settings are rejected before population generation.

## Gameplay connections in this batch

- Supply planning reads actual inventory, learned spells, glyph reagent exemptions,
  professions, hunter weapons and pet diets. It selects vendors that carry needed
  goods and purchases through normal vendor checks on the world thread.
- Cash shortages sell owned vendor goods before pursuing available quest or
  gathering income. Failed income goals retain their retry deadlines.
- Pet feeding casts Feed Pet with owned food. Soul shards come from Drain Soul;
  taming requires a nearby creature and the learned spell. Fishing tools are
  acquired through supply planning.
- Group supply gifts use normal trade initiation, acceptance, exact cart checks and
  completion observation. They record assistance only after server completion.
- Legacy RPG flights use discovered routes and actual fares. Core map transfers
  that continue a paid cross-map flight remain part of normal flight execution.
- Private human messages can contain up to three recognized requests with separate
  negation. Corrections remove unsent replies and pending social requests before
  replacement replies are prepared. Commands keep their existing authoritative path.
- Bank transfers and ingredient withdrawals use normal core handlers on the world
  thread. Crafting checks owned bank stock before purchasing new ingredients.
- Pending auction receipts retain the purchased item GUID across restarts. New
  orders wait for actual mailbox collection; overdue receipts remain visible.
- Rest uses actual mana independently of drink stock. Solo, uncommitted bots can
  visit nearby inns, take social pauses and greet visible players occasionally.
- Revision checks on atomic state/cognition saves prevent older asynchronous
  writes from replacing newer memory. State format six also retains quest
  references, party commitments, pending relationship observations, initialization
  phases and custom class profiles. Formats two through five remain readable.
- Dungeon selection refreshes whole-party eligibility and lockouts on the world
  thread, submits random categories separately from specific dungeons, and records
  a commitment only after the ordinary core handler accepts the selection.

## Diagnostics and evidence

`.playerbots advai <guid>` reports personality, goals, mood, ambitions, recent
experiences, lifecycle deadlines, pending conversation and supply facts.
`.playerbots advai` includes repetition, expired messages, failed commitments,
identity writes, factory inventory units/money increments, rejected factory grants,
processing cost, rolling world-update percentiles, activity population counts,
activation transitions and the longest current background selection wait. Factory counters are not a
complete audit of every core resource source.

Portable checks run from the separate `apps/advai/CMakeLists.txt` target. Native
builds use the supported core. The isolated `e2e/` harness uses real client packets;
component tests and a successful build do not substitute for live scenarios,
2,000-online load testing, forced-activation stress, structured playtesting. The user removed the 24-hour soak requirement.

## Custom content and memory

See [BACKPORTS.md](BACKPORTS.md) for the matching DBC/MPQ bundle, optional world
migrations, the supported-core pet change and the installed class definitions.

ADVAI enforces `Playerbots.AdvAI.MemoryLimitMB = 10000` using the operating
system. The limit uses decimal MB and conservatively charges the entire world
process, including core-owned bot objects. Windows limits private committed
memory; Linux limits process data allocations. New bot logins stop at 90% of
this cap. Existing commitments continue. Reaching the OS limit makes allocation
fail; it is not a successful load result. Diagnostics report the limit, private
usage, resident usage, peak usage and admission status. The reference workload
still must meet its population and performance targets within the cap.

Profile saves allow one in-flight transaction per character and coalesce newer
changes. Relationship caches retain 128 contacts per bot; indexed durable rows
retain evicted contacts. Pending events wait for asynchronous contact loading.
Shutdown drains outstanding profile and initialization saves before database
closure, with an explicit failure after 30 seconds.
