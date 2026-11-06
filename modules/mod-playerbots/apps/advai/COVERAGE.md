# ADVAI implementation and acceptance evidence

Last updated: 2026-10-17. This is a development build. An implementation reference is not runtime acceptance evidence.
Every pending implementation or acceptance item below remains a release blocker. User scope update: English only; additional language packs are no longer required.

| Behavior | Implementation | Evidence / remaining work |
| --- | --- | --- |
| Persistent, stable identity and traits | `src/Advai/AdvaiModel.*`, `AdvaiMgr.*`, state migration | Every generated character receives a deterministic GUID-based identity at startup; initial writes are staggered at four per world update. Existing persisted seeds remain authoritative. Offline identity determinism covered by component tests; live backfill and restart acceptance pending. Missing loaded personalities fail explicitly. |
| Separate mood, needs, commitments and long-term ambitions | `AdvaiCognition.*`, snapshot and goal contracts, atomic cognition persistence | Temporary mood and stress settle independently of personality. Observed level, profession, savings and friendship milestones influence the planner without bypassing maintenance or commitments. Component persistence/clock checks passed; richer multi-stage ambitions and live restart scenarios pending. |
| Relationships from conversations, assistance, deaths and trades | `AdvaiMgr.cpp`, `AdvaiNegotiation.cpp`, `AdvaiCognition.*`, `Script/Playerbots.cpp` | Durable positive/negative experience summaries and 32 recent experiences implemented, including shared quest objectives, boss victories and failed agreed trades. Relationship working-set bounds and live event acceptance pending. |
| Goal priority, switching costs, failures and retries | `GoalPlanner`, `AdvaiLifeStrategy`, action execution observer | Priority, commitments and bounded backoff covered by component tests. Full action coverage pending. |
| Ordinary speech, slang, typos, negation, links and command isolation | `ChatRules` and both `PlayerbotAI::HandleCommand` entry points | Named mentions and auxiliary 'do' separated from commands; registered shorthand/link parameters retained. Up to three recognized private requests retain separate negation and question semantics. Corrections replace unsent replies and unexecuted invitations. Linked names cannot inject intents. Partner-scoped quest follow-ups now survive restart. Speech acts and confidence, specific-subject priority, omitted question punctuation and topic-change invalidation are implemented. Comprehensive grammar, ambiguous compounds and broader entity-specific context remain pending. |
| Knowledge grounded in server state | `Manager::Capture`, `ChatRules::Facts`, `AdvaiKnowledge.*`, trade and recipe operations; factual rendering at delivery | Quest links resolve to actual log status, counts, level requirements and client-visible map markers. Invalid linked quests clarify rather than returning an unrelated personal quest. Pinned catalog snapshots render localized facts from fresh gameplay state at delivery. PvP preparation is distinct from actual participation. Live possessions, commitments, quest and transaction assertions pending. |
| 36 distinct variants per supported outcome | `catalog/enUS.txt`, `catalog.py`, text manager validation | 50 English outcomes, 1,800 authored variants, including six observed-event and party-readiness outcomes. Duplicate and placeholder validation passed. Editorial review for meaningful variation pending. |
| Personality/context eligibility and weighted phrasing | `AdvaiDialogue.*`, cached speech index, stable styling and bounded recent-line exclusion | Stable content-based identities, length/slang/energy weighting and familiarity/audience guards implemented. Repeated-topic selection remains available after all variants have been used. Broader authored context metadata and live style assessment pending. |
| English-only content and strict catalog loading | Existing translation table, English catalog compiler and runtime validation | User scope is English only. Runtime speech uses English independently of the realm DBC locale; compiler and loader require English entries only. All 44 outcomes retain 36 variants and 71 factual templates. Other language packs are outside scope. English editorial quality and rendered-message edge cases remain part of acceptance. |
| Channels, faction language, visibility, membership, mute and ignore | `AllowedChannel`, actual chat delivery APIs | Implemented channel guards. Live permission tests and public-channel membership scenarios pending. |
| Bot-to-bot exchanges through delivered messages | `ReceiveChat`, `Deliver`, `Ambient`, shared thread ledger | Six-message and five-participant bounds, respondent claims and channel budgets implemented. A bounded, expiring eight-line sender ledger retains exact delivered text and canonical meaning through delayed packets and translation; both command entry points exclude verified speech. Legacy autonomous broadcasts and unowned command chatter conflict with the preset. Progress, group proposals, stock advertisements and small talk connected. Complete event-to-dialogue coverage and live storm/loop/interruption scenarios pending. |
| Proactive cooperation and invitations | Familiar-player quest proposals, group offers, queued native `InviteOperation`, packet-based acceptance | Nearby available familiar players receive private questing proposals; explicit conversational consent queues a normal invitation. Declines defer further proposals. Only delivered proposals enter conversation memory. Live repeat-party/consent scenarios and organized raid recruitment pending. |
| Negotiated real trades or cancellation | `AdvaiNegotiation.cpp`, `AdvaiTrade.cpp`, core trade handlers, actual-stock advertisements | Concrete terms, ordinary-speech prices, cart verification, expiration and completion observation implemented. Group supply gifts now use ordinary trade initiation/acceptance and verified contents instead of direct inventory transfers. Generosity affects vendor-floor-bounded asking prices. Live gift and adversarial trade scenarios pending. |
| Earned quest and level progression | Existing quest/RPG engine, grant guards, level observer | Artificial completion and repeated factory grants rejected for initialized ADVAI characters. Login adopts previously played characters before any initial boost; add-class bots cannot receive repeated setup. Factory inventory creation is guarded centrally. Full prerequisite/reward/trainer progression scenarios pending. |
| Maintenance: bags, repairs, supplies and finances | `AdvaiLifeStrategy`, `AdvaiSupplies.*`, cached needs, native actions | Supplier selection checks actual required stock. Purchases refresh inventory and learned class-spell requirements, honor glyph exemptions, ammunition/weapon compatibility, pet diet, normal vendor restrictions, funds and bag space. Basic mining/skinning/blacksmithing/engineering tools and fishing poles enter acquisition goals; compatible owned tools satisfy the need. Required tools/reagents/pet food are retained. Feed Pet consumes actual food; Drain Soul earns shards from eligible creatures; taming uses nearby creatures and the learned spell. Cash shortages prefer owned vendor goods and respect blocked income retries. Live class/pet/supply cases, advanced tools and mounts pending. |
| Legitimate travel and blocked-path replanning | Existing RPG/travel engine, stuck guard and service exclusion; native summons | Synthetic summons and graveyard relocation rejected, spirit-healer recovery uses normal handlers, arena pipe exits use jumps. Legacy RPG flights now require known routes and retain actual fare deductions for ADVAI. Cross-map service planning and complete travel audit pending. |
| Professions and useful crafting | `AdvaiProfession.*`, trainer and crafting integration | Chooses known recipes, checks reagents, retrieves owned bank ingredients through normal bank handlers, buys vendor/AH ingredients and observes products. Exact purchased-item receipts persist across restarts and block duplicate ordering while mail is pending; purchasing an ingredient does not complete the craft. Live bank/full-bag/delayed-mail/skill-up/tool scenarios, broader recipe learning and market demand remain pending. |
| Auctions, mail and banking | `AdvaiEconomy.*`, ingredient buyout operation, existing bank action | Auction, mailbox and bank handlers execute on the world thread. Bank transfers preserve quest-item accounting and verify inventory changes; the legacy always-false bank result was fixed. Full bags can still receive stackable mail through ordinary core checks. Full auction price model, mail sending/COD decisions, broader withdrawal planning and cross-database crash recovery remain pending. |
| Guild life and dissatisfaction | Social events, tenure extension, normal guild departure | Lifecycle rules covered by accelerated component clocks. Guild assistance, repeat activities and satisfaction playtesting pending. |
| PvE preparation, roles, travel, lockouts, pulls, loot and wipes | Existing class/instance engines, ADVAI goal gating and `DungeonJoinOperation` | Whole-party core eligibility/lockouts refreshed on the world thread; offline members and unavailable destinations block explicitly. Random categories are submitted alone; specific selections remain coherent. Normal join handler acceptance creates the commitment. End-to-end preparation, role, lockout and encounter acceptance/audit pending. |
| PvP objectives, regrouping and arena preparation | Existing BG/arena engine, ADVAI queue gating and world-thread arena invitations | Available teammates receive real invitations; no forced login, disbanding or team teleports. Objective/role and observable-information scenarios pending. |
| Stable skill and recoverable human variation | Persona reaction delay, preparation thresholds, existing role logic | Risk tolerance and skill influence preparation; competitiveness influences activity commitment. Stable reaction delay implemented. Positioning, target-choice variation and bounded recoverable mistakes pending. |
| Purposeful idle behavior | Registered `advai leisure` action, nearby inn routing, ordinary regeneration/owned supplies and short-session lifecycle | Rest follows actual health/mana separately from purchasing needs. Solo, uncommitted bots can walk to a friendly nearby inn, take bounded social pauses and greet visible, non-ignored players with occasional harmless gestures. Actual rest completion clears the goal. Runtime/playtesting acceptance pending. |
| Human-zone activation plus 100 distant bots | World-thread roster, immutable active set, group units, 30-second stagger | 2,000-member component workload and rotation fairness passed. Party-size fairness regression added. Live transition and stagger tests pending. |
| Safe suspension with no simulated rewards | Activity gating, committed unit activation, stop movement on suspension | Core integration implemented. Suspended/offline reward invariants require live assertions. |
| Short sessions and retained long breaks | `Lifecycle`, manager, random-bot roster | Accelerated tests cover short pauses, commitments, ordinary/guild tenure and 14ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“30 day returns. Restart and replacement scenarios pending. |
| Persistent deadlines, returns and replacements | State migration, eligibility index, fresh/return selection, reserve-aware account assignment | Preset provisions approximately 16 retained identities per online slot for default duty cycles and makes surplus generated accounts eligible for random login. Fresh/return preference advances after each accepted scheduling decision, preventing fresh-first starvation when only one slot opens. Returning characters are never reset. Extreme synchronized depletion/dynamic provisioning, isolated database recovery and replacement acceptance pending. |
| Thread safety, bounded cost and async persistence | State locks, GUID operations, immutable rosters and dialogue snapshots, async transactional state/cognition writes | Catalog publication validates a complete replacement before publishing; queued replies pin that snapshot. Revision-guarded atomic state/cognition transactions reject out-of-order stale saves, verified with reversed native SQL statements. Configuration reload preserves live cognition. Lock-path review, relationship/profile memory bounds and measured processing cost pending. |
| Strict preset and diagnostics | `conf/playerbots.advai.preset`, config validation, `.playerbots advai`, metrics collector | Mood, ambitions, experiences, partner/topic, pending replies, repetition, expired replies, failed commitments, pending identity writes and supply facts exposed. Initial factory inventory units, initial money increments and rejected factory grants counted; complete resource auditing and detailed fairness acceptance pending. Online/human-zone/mandatory/background counts, activation changes and the longest current background wait are now reported. World p95/p99 and forced activation implemented. Catalog/config conflicts fail before population generation. |
| Supported-core build and repository checks | Separate `PlayerbotCore` branch `Playerbot`, module junction | Before the English-only scope update, module, worldserver and authserver builds passed against supported core `8a4e76eac8c4` (Playerbot branch). Native logs: `PlayerbotCore/var/advai-build-integrated.log` and `advai-link-integrated.log`. The unrelated talent-overrank module is excluded only in the isolated build configuration. Last completed component run: 610 checks passed, including all 44 English outcomes, private friend proposals, compound requests and acquisition retries. C++ codestyle and registration checks previously passed; final batch checks pending. |
| Repeatable migrations | Isolated MySQL 8.4 database; dated state, cognition, content and cache migrations | All six migrations have each been applied twice in the isolated database; the revision and factual-template migrations were replayed with this batch. SQL confirmed 44 outcomes, 36 minimum variants and 1,584 total rows. State, cognition and cache tables confirmed InnoDB. The factual migration contains 71 rows with all nine locale entries populated. State format three retains quest references and exact pending auction receipts; portable checks load previous format-two profiles. Conversion explicitly replaces the legacy FIXED row format with DYNAMIC. |
| Live scenarios, 2,000 online load and forced activation | Isolated MySQL 13316, auth 13724, world 18085; `runtime.py`, `e2e/conversation_test.go` | Data and databases provisioned; 520 test characters created by the native factory. Previous executable reached destination-cache initialization, then was stopped for the updated build. No scenario acceptance recorded. Test generator disables the optional command server. Live behavior, updated-catalog boot and performance acceptance remain blocked/pending. |
| Structured player-likeness assessment | Acceptance specification below | Structured in-game review remains pending. The user removed the 24-hour soak requirement. No claim of indistinguishability or production readiness. |

## Acceptance runs

Record exact core/module revisions, configuration, machine details, database sizes, start/end timestamps,
logs and transcripts with each run. Keep smoke, reference-load, forced-activation and structured playtesting evidence separate.

1. Boot an isolated realm, apply migrations twice, and confirm the selected catalog is fully validated.
2. Exercise ordinary/ambiguous/negated conversation, real humans, all channels, ignore and mute, bot groups,
   delayed replies, interruptions, simultaneous speakers and bounded exchanges. Compare factual statements
   and commitments against authoritative state.
3. Exercise each gameplay family with successful and blocked paths: missing money/reagents, full bags,
   broken equipment, unavailable services/routes, prerequisites, abandonment, death and wipes.
4. Save/restart and accelerate lifecycle clocks in tests. Verify retained identities, deadlines, guild behavior,
   eligible returning characters and fair replacement. Suspended/offline bots must gain no rewards.
5. Measure 2,000 online bots with 200 human-zone bots plus approximately 100 distant active bots, then
   no humans, multiple occupied zones, rapid transitions and forced activation. Reference targets are
   world update p95 <100 ms, p99 <200 ms and combined server/database memory <24 GiB on the i7-12700K/32 GB host.
6. Run structured play sessions; the user removed the 24-hour soak requirement. Review transcripts and traces for repetition,
   contradictions, mechanical timing, stalled goals, implausible actions and missed commitments.

## Local tools

- `catalog.py --locale enUS` checks the English catalog. `catalog.py` requires the English speech and factual catalogs. `--facts-output <migration>` compiles English factual templates. Additional language packs are outside scope.
- `configure.py --source <distribution config> --output <new config>` merges the explicit ADVAI preset.
- `runtime.py --core <supported checkout> --output <new runtime>` generates isolated configs. It requires
  `--mysql-client <native mysql executable>`, `ADVAI_DB_PASSWORD` and extracted
  `var/advai-data/{dbc,maps,vmaps,mmaps}` under that core checkout.
- `CMakeLists.txt` builds the portable rule, planner, roster and accelerated lifecycle tests independently
  of the live core. These tests provide component evidence only.
- `e2e/` uses the pinned AzerothGhost harness against a live isolated realm. Set `E2E_AUTH_ADDR`,
  `E2E_AUTH_DSN`, `E2E_CHAR_DSN`, `E2E_WORLD_DSN` and `E2E_PLAYERBOTS_DSN`, then run
  `go test -tags=e2e -count=1 -v -parallel 1 -timeout 10m ./...` there. The initial scenario verifies
  a response against the character's actual class and checks that joining a group requires acceptance.
- `.playerbots advai active all` enables the forced-activation stress workload. `.playerbots advai active normal`
  restores the prior distant budget. `.playerbots advai` reports active population and recent world-update
  percentiles; the world-frame window contains at most 4,096 samples. Costs are ADVAI processing costs,
  not the complete server processing time. A one-minute diagnostic log is emitted automatically.

## Integrated implementation batch: 2026-10-17

These updates supersede older counts and implementation descriptions above.
Native acceptance of the current source is still pending.

| Behavior | Implementation | Current evidence / remaining work |
| --- | --- | --- |
| Crash-safe one-time setup | Sealed initial target/profile, character transaction and initialization receipt; format six | Component checks retain an unfinished level/profile and reject reseeding initialized characters. Native fresh-login, restart and grant acceptance pending. |
| Dracthyr generation | Dedicated names, playable appearance filtering, faction creation data, authored DBC bundle and racials | Eight DBC tables generated; MPQ members read back and hash matched. Optional world migration applied. Generated/native-login and visual acceptance pending. |
| Cultist/Necromancer behavior | Persistent profiles, advancement progression, mana-based spell variants, companion statistics, native summons, four paid trainers, core undead pet persistence | Earlier base class migration applied twice. Full advancement DBC/asset bundle and matching class world migration generated; MPQ member readback and hashes verified. Spell/trainer gates and combat actions are implemented. Actual casts, level-by-level training, and reconnect acceptance remain pending. |
| Durable contact working set | 128 resident contacts; indexed relationship table, async hydration, revision-guarded dirty rows and pending observations | Social-memory module batch compiled; current integrated build pending. Contact loss/reload, failure and restart runtime scenarios pending. |
| Persistence backpressure | One in-flight profile save per bot; newer changes coalesce; shutdown drains callbacks | Portable run: 634 checks passed. Database-stall and shutdown runtime acceptance pending. |
| Memory ceiling | OS limit of 10,000,000,000 bytes, conservatively including the world process; 90% login backpressure and diagnostics | Implemented. The complete 2,000-online workload must still fit and pass performance targets. A cap-induced allocation failure remains a release blocker. |
| Observed event conversations | Deaths, wipes, victories, quest objectives and party readiness use delivered speech, budgets and delayed-state checks | English catalog has 50 outcomes / 1,800 lines. Live transcript and editorial acceptance pending. |
| GM-mode human speech | GM chat packet routed through the existing validated packet parser | Warm-realm live class-question scenario failed and exposed the missing dispatch case; corrected source awaits live rerun. Graceful isolated shutdown succeeded through actual client packets. |
| Group preparation | Readiness holds, legitimate rest, visible regrouping, wipe counting and normal departure; bot-only native LFG proposals permitted | Implemented. Dungeon/raid eligibility, run completion, recovery and role behavior remain runtime blockers. |
| Travel safety and service flights | Ground routes reject another map; progress timeout runs before movement waiting; known-node route search and native paid flight operation; owned hearthstone service route | Implemented. Service arrival, costs, cooldowns, transport planning and broader cross-map travel acceptance pending. |

The full plan remains incomplete. In particular, comprehensive activity and
resource audits, richer entity-specific dialogue, dynamic reserve provisioning,
2,000-online reference/stress workloads and structured playtesting are open.
The retired 24-hour soak and additional languages are not release requirements.

## Native run update: 2026-10-17 08:43 UTC

The module, game library, worldserver and authserver built successfully. Logs:
`PlayerbotCore/var/advai-backport-final-build.log` and
`advai-backport-current-link.log`. The corrected Dracthyr bundle preserves vanilla
model IDs, allocates isolated displays 50000-50005/models 10000-10005, and rejects
duplicate indexed IDs. The native factory has created 35 Dracthyr characters.

The isolated realm booted with the OS cap active. At 22 online bots, observed
private process memory was approximately 3.9 GB and resident memory approximately
3.8 GB; these are small-run observations, not 2,000-online acceptance. The live
client batch failed during world authentication for both custom-content scenarios
and the conversation scenario. No custom-spell, trainer or conversation acceptance
is claimed. See `var/advai-gameplay/backport-client-scenarios.log`. World
authentication and slow database commits remain under investigation.

## Complete Ascension class data backport: 2026-10-17

The installed Ascension client is the authoritative source. Older addon/Content
JSON reuses spell and advancement IDs; its entries are retained for reference
and do not select gameplay content. The canonical catalog has 356 nodes (160
Necromancer, 196 Cultist), seven specializations, and all 160 level budgets.
The source snapshot contains 2,758 spell records including dependencies.
The installed client archive, `WOW/Client/Data/patch-ZZZZZ-AscensionClasses.MPQ`,
contains 23 WotLK DBC tables and 1,622 assets; all 2,446 active asset references
resolve and the MPQ was read back and SHA-256 verified. The complete `F:\Ascension`
backup scan recovered the model and audio sources, and all 71 MPQs found inside
backup ZIPs match their direct copies. No active sound references are muted or
substituted. The matching optional world migration contains the class dependency
subset and 341 rank associations. `SkillCardData.json` is not read or included.

Implemented, pending integrated runtime evidence:

- Sixteen class mechanics files ported with source attribution: abilities,
  auras, event handling, resources, minion armies, and metadata contracts.
- Persistent specialization and node ranks in native player settings, saved
  alongside learned spells. Earned level budgets, prerequisite checks, free
  choice exclusions, and bounded advancement batches prevent free respecs or
  unearned point spending.
- Dedicated class action selection and role reporting for Influence, Corruption,
  Godblade, Bulwark, Death, Animation, and Rime. Classic carrier spell strategies
  are excluded when the complete Ascension profile is active.
- Core charge recovery, replacement spell casts/action bars, conditional crits,
  stack modifiers, summon area auras, and generic class spell hooks are wired
  into native WotLK spell handling. Build and integrated runtime acceptance remain
  pending.
- A separate reference database compiler applies the pinned class SQL migrations
  before extracting class-owned world dependencies; gameplay schemas are never
  its destination.

Open class release blockers: the final module build; validation of effective
summon dependencies; carrier stat/proficiency/equipment and resource integration;
runtime evidence for suspended minion activity; comprehensive class rotations;
native class casts, progression, healing/tanking, resource costs, army behavior,
and client presentation.
The MySQL TLS and cleanup fixes compile but still need live authentication and
crash-regression evidence. No full class or full ADVAI acceptance is claimed.


## Playerbot behavior implementation batch: 2026-10-17 (compilation deferred)

The user assigned Ascension porting to another agent and explicitly stopped further
compilation during implementation. The following source changes have **not** been
accepted by a completed native build or live scenario. They are implementation
work, with integration and acceptance still required.

| Behavior | Implementation | Remaining evidence |
| --- | --- | --- |
| Actual item/spell subjects | `AdvaiKnowledge`, parsed spell/enchant links, `ItemKnown`/`SpellKnown`, state format ten | 36 authored English variants per new outcome, fresh inventory/bank/equipment/spell/cooldown facts at delivery, partner-scoped item/spell/profession references. Native transcripts and malformed/context scenarios remain pending. |
| Market demand and craft profitability | `AdvaiMarket`, native successful-auction callback, 24 hourly sale buckets, incremental 1,024-listing world batches | Real sales include immediate buyouts; current supply and material opportunity cost influence production. Sale windows are session-local. Native auction/craft cases remain pending. |
| Bounded auction exposure | Personal limits of eight listings and two stacks of one product; actual deposit and sale handlers | Live limits, price changes and rejected deposits remain pending. |
| Advanced profession tools and prerequisite recipes | Native tool category index, owned-bank tool preference, depth-eight craft dependency stack, normal casting and purchases | Retains advanced tools, reserves products/materials and persists the chosen recipe in the life goal. Native rods, tool substitution, cycles and interrupted crafting remain pending. |
| Earned mounts | Supply planning detects missing ground/flying speed tiers, purchases real vendor items and uses the owned item to learn | Riding eligibility, actual learned mount speed and Northrend Cold Weather Flying govern upgrades. No mount or riding spell is granted. Price, skill, faction, item-use and successful learning scenarios remain pending. |
| Observable PvP decisions | Enemy and flag-carrier visibility/detection guards; distant combat-state filtering | Native stealth, distance, objective and regrouping scenarios remain pending. |
| Stable PvP roles | Persona- and instance-derived roles through join and objective changes; opposing bots' private strategy no longer selects a counter-strategy | Live match distribution and class-role behavior remain pending. |
| Suspension at execution | Additional bank, auction, mail, ingredient, flight, gift and arena action guards | Queued-operation/suspension boundary scenarios remain pending. Existing agreed trades can finish through native handlers. |
| Retained profile memory | Staggered offline archive, compact acknowledged state/cognition and lifecycle index; on-demand hydration without synchronous map-thread database reads | Fresh reserve profiles and returning characters retain one-time setup state. Concurrent login, restart and memory workloads remain pending. |
| Relationship load bursts | Prepared batches of 64 contacts, up to 256 requests per world pass, bounded in-flight batches; 1,024-event durable pending backlog | Overflow becomes an explicit error instead of discarding observations. Failure/restart, fairness and burst acceptance remain pending. |
| Actual raid assembly | Stable eligible destination, native group invitations/conversion/difficulty, available guild/visible recruits, missing-role counts | Ordinary native consent/acceptance remains authoritative. Guild roster, lockout, role shortage and invitation expiry scenarios remain pending. |
| Raid entrance travel | Cached native portal index, roster/role signature invalidation, normal movement/native area-trigger entry | Cross-map journeys use the new ordinary travel network. Complete encounter traversal and native entry/readiness scenarios remain open. No synthetic instance teleport is added. |
| Ordinary cross-map journeys | `AdvaiTravel`, actual area-trigger and gameobject portals, known paid taxi routes, owned hearthstones and learned class spells | Boats use physical deck paths, actual passenger attachment and scheduled stops. Native route, cooldown, fare, access, interrupted transit and boarding geometry scenarios remain pending. |
| Grounded class/gear/profession/guild answers | Native specialization/role, equipped weapon, average item level, own durability, guild membership, and real skill ranks | Other players' visible gear requires proximity, phase and line of sight; private talents/skills are reported unknown. Authored English catalogs are updated; transcript acceptance remains pending. |
| Useful equipment enchanting | `AdvaiProfession` and `AdvaiLifeStrategy`, native stat-weight comparison and item-target casts | Uses learned recipes, existing tools, earned reagents and actual item GUIDs. Completion requires the expected permanent enchant on the target; existing enchants require a useful upgrade. Native cast/acquisition/interruption scenarios remain pending. |
| Requested guild invitations | `GuildInviteOperation`, direct affirmative requests, native guild invite handler | Rank permissions, faction, ignore and current membership remain authoritative; recipients choose acceptance. Native permission/rejection/consent scenarios remain pending. |
| Request and guild lifecycle safety | Expired requests removed before commitment checks with logs/counter; normal guild departure handler | Expired requests cannot retain population commitments indefinitely. Guild master/permission restrictions retain membership when departure is rejected. Native queued/expiry/restart scenarios remain pending. |
| Guild creation economy | `GuildCreateActions`, paid native emblem vendor handler | ADVAI bypasses the legacy money grant and free emblem setter; a formed guild can wait for money and a tabard designer. Native economy scenarios remain pending. |
| Fishing and earned experience guards | Autonomous shore search/LOS correction, truthful fishing cast result, synthetic XP rejection | Pole ownership and normal fishing skill/casts/loot remain required. No successful fishing or earned-XP runtime acceptance is claimed for this batch. |
| Complete-market ingredient discovery | Per-item native auction indexes, rotating pages of at most 256 IDs, current listing revalidation | Removes the fixed first-4,000-auction search limit. Same-account bids and expired listings are excluded; stale indexes and unfinished searches retain explicit pending state. Native large-market acceptance remains pending. |
| Guild charter execution and consent | `AdvaiGuild` dispatches creation, offers, signing and turn-in through world-thread operations, preserving GUIDs and packet copies | Native cached petitions/signatures replace synchronous map-thread reads. World-published counts drive triggers; pending tasks are deduplicated. Personality and native eligibility govern signing; actual signatures confirm success. ADVAI retains the one-signature-per-account rule. Native scenarios remain pending. |
| Persistent interrupted activities | State format nine, bounded retained goals, planner resumption, durable craft reservations | Repairs, restocking, training, mail, banking and trades retain the previous activity's destination/recipe, retry state and materials. Survival/recovery retains the current goal. Resume honors obligations and retry deadlines. Restart, mail collection and interrupted-craft scenarios remain pending. |
| Earned arena teams | `AdvaiArenaPreparationAction` and `ArenaCharterOperation`, paid native charter purchases, nearby signatures with remembered offers, normal turn-in | ADVAI bypasses direct factory assignment and artificial team ratings. Native costs, inventory, account uniqueness and actual signed charters create teams. Queue types match actual membership. Native purchase/signing/creation/match scenarios remain pending. |
| Guild and arena membership consent | World-thread guild/arena acceptance, stable personality and relationship preferences, actual membership verification | Human-controlled bot permissions remain authoritative. Native rejection and decline handling remains intact; ordinary acceptance avoids hardcoded universal-language chat. Native scenarios remain pending. |
| Verified guild bank deposits | World-thread dispatch, real bank proximity and native rank rights, observed removal from personal inventory | Rejected deposits return failure instead of reporting an item as deposited. Native permission, full-tab and item merge scenarios remain pending. |
| Required crafting stations | Read-only native spell-focus index, actual focus object checks, legitimate station journeys and bounded failed-station exclusions | Forges, anvils, laboratories and fires must actually be available before casting. Deactivated objects and failed routes cause explicit blocking/replanning. Native focus, phase and travel scenarios remain pending. |
| Practical preparation spending | Essential food, drink, ammunition, pet food, reagents and tools precede mount purchases; personality-based money reserve | Mount upgrades wait when buying would exhaust the preparation budget. New recipe selection excludes active cooldowns and restored recipes require actual active-spec knowledge. Native shortage/cooldown scenarios remain pending. |
| Natural membership response timing | Bounded delayed guild/arena acceptance and charter-signing queue, personality/skill/reading delay, 32 dispatches per world pass | Combat, casting, flights and teleport transitions delay responses within a fixed expiry instead of instant acceptance. Pending tasks are deduplicated, capacity failures are explicit, and the GM aggregate reports delayed responses. Native timing/interruption/load scenarios remain pending. |
| Equipment- and resource-based supply needs | Native mana capacity and equipped ranged weapon determine drink/ammunition needs; learned flying forms count as travel capability | Warriors with bows/guns can stock ammunition; wands do not require it. Flight forms avoid unnecessary flying mount purchases, and ordinary flying mounts remain eligible with native 60% or 150% data. Native class/form/equipment scenarios remain pending. |
| Conversation delivery permissions | Guild speak/listen rights, native faction/cross-guild language configuration, party subgroup and recipient-ignore filtering | Group replies no longer use an unfiltered packet broadcast. Actual delivery still feeds the shared speech ledger and conversation budgets. Native guild rank, subgroup and ignore scenarios remain pending. |
| Persistent guild identity and social interruptions | State format ten retains the current guild ID; accepted trades, gifts and group activities retain personal goals | Guild changes reset the prior guild's satisfaction. Legacy profiles adopt their first known identity without discarding existing opinion. Native membership/restart and interrupted-activity scenarios remain pending. |
| Gathering for actual crafting needs | Startup native loot/reference index, skill/phase-aware node selection, existing gathering and loot actions | Missing materials can be harvested in the current zone. The craft waits for actual carried inventory, with a three-minute/64-failed-node limit and ten-minute search cadence. No materials or skill credit are manufactured. Native gathering/crafting scenarios remain pending. |
| Owned bank supplies before purchases | Cached needed-stack GUID, native banker travel and world-thread bank transfer, refreshed carried totals | Food, drink, ammunition, reagents, tools and usable mounts already in the bank can satisfy preparation, including when the bot has no gold. One stack is withdrawn per pass and actual inventory change is required. Native bag-capacity, changed-needs and restart scenarios remain pending. |
| Crafting project conversation | Existing profession outcome and authored variants, live retained recipe and missing reagent/tool facts, dated English factual migration | Questions about current crafting and required materials resolve to the actual retained project. No project and changed requirements are stated explicitly. Native transcripts remain pending. |
| Safe rotation during native actions | Cached action-in-progress fact plus fresh world-thread cast/loot checks | Casting and open loot windows retain activity until a safe stopping point; goal switching and lifecycle use the same commitment. Native rotation scenarios remain pending. |
| Observable PvP target decisions | `AdvaiPvp`, `EnemyPlayerValue`, stable personality scoring | Favors flag carriers, visible healing casts, reachable low-health enemies, allied pressure and threats to vulnerable teammates. Current-target preference reduces switching. Damage-breaking crowd control and visible major immunities are excluded. Candidates and allies are bounded; hidden specializations/cooldowns are not read. Native match scenarios remain pending. |
| Allied tactical suggestions | Ordinary focus/peel phrases, same-group/team/map validation, 12-second target suggestion | Visible named enemies, a teammate's actual victim, observed healing casts or an actual attacker can influence the existing target selector. Suggestions expire, retain native attack checks and do not replace human-master command authority. Native consent, target-change and visibility scenarios remain pending. |
| Safer PvP regrouping | Registered `bg regroup` action in battleground and arena strategies | Hurt/outnumbered bots can move toward a visible teammate with greater separation from observed enemies. Flag carriers, human-controlled bots, vehicles and active casts retain their existing behavior. Native pathing, combat movement and objective scenarios remain pending. |
| Tactical conversations during combat | Focus, regroup and peel outcomes; 36 authored English variants each; live facts at delivery | Tactical event speech can precede ordinary queued conversation, expires within ten seconds, uses per-bot/group/channel budgets and suppresses automatic replies. Stale targets/pressure/positions are discarded. Human tactical replies use shorter personality-based delays. Native transcripts and storm scenarios remain pending. |
| Actual arena and battleground answers | Native team and personal ratings, requested bracket selection, actual battleground match state | Existing arena/battleground variants now include factual context. Multiple teams prompt bracket clarification; absent teams and unknown personal information are stated plainly. English catalog totals 55 outcomes / 1,980 lines. Source catalog/contract/SQL generation completed; native build and runtime acceptance remain deferred. |
| Expanded English conversations | 24 additional authored lines for each of the 55 outcomes; `2026_10_17_05_advai_expanded_english.sql` | Latest catalog totals 3,300 lines, exactly 60 variants per outcome. Covers social chat, factual answers, trade negotiations, groups, observed events and PvP. Catalog generation rejected duplicate lines before producing the complete repeatable migration. Native transcript and editorial acceptance remain pending. |
| Broader repetition exclusion and natural endings | 96 retained line identities, exclusion of recent eligible lines up to the history bound, declarative-answer preference | Questions and later conversation turns favor statements without disabling follow-up questions. Casual gamer vocabulary retains personality eligibility. Existing saved histories load within the expanded bound. Native personality/repetition/restart acceptance remains pending; no compilation or runtime tests were run for this expansion. |
| Occasional human travel mannerisms | `AdvaiMannerisms`, personality-dependent staggered opportunities, native physical jumps at normal running speed | Only clear, flat outdoor point routes are eligible. Short-path, terrain, water, LOS and corner checks protect the route; the original destination resumes after landing unless another activity supersedes it. Combat, groups, human control, mounts, transport, water, vehicles and precise approaches are excluded. Some personalities never choose the habit. Movement effects retain scheduler activity; diagnostics expose hop count and next opportunity. Source implementation only; native movement and visual acceptance remain pending. |
| Continuous movement and stable turning | `AdvaiMovement`, direct native splines over validated ground corridors, existing action-priority timing | Routes continue across their points without per-waypoint AI stops. Corner rounding uses bounded native raycast/ground/collision checks and persistent tighter/broader/restrained styles. Precise approaches and instance geometry retain their validated corners. Current routes are reused; native running/walking/backward speeds and speed changes are respected. Native movement, root/cast interruption and speed-change acceptance remain pending. |
| Route preference and bounded memory | Four familiar corridors per character, observed-threat costs, two detour candidates, eight temporary failed-position exclusions | Only visible nearby threats influence route costs. Cached corridors need a reachable forward join and current collision LOS; variation is committed for the route. Alternatives use native complete paths and bounded detour lengths. No road classification data is invented. Native obstacle, threat, memory and load scenarios remain pending. |
| Varied useful approaches | Stable per-character/object angles, six native reachable candidates, visible-player crowd costs; nearby life-service integration | NPC/world-object arrivals vary inside the requested approach radius while respecting terrain, interaction distance and LOS. Failed approaches report a blocked reason. Native vendor, trainer, bank, quest-object and crowded-arrival scenarios remain pending. |
| Purposeful PvP footwork | Registered `advai pvp footwork`, short verified paths, target-facing movement | Ranged bots make limited lateral adjustments or retreat under immediate pressure; melee bots approach rear positions incrementally. Stable timing/side preferences, maximum path length and terrain/LOS requirements apply. Casting, urgent movement, carriers, captures, vehicles and human control take priority. Unpressured healers retain their role actions. Native combat/visual acceptance remains pending. |
| Contextual pauses and legitimate recovery | Brief reading pauses for selected personalities on long direct questions; two bounded recovery attempts and earlier safe positions | Pauses resume only for the same goal and are interrupted by combat or activity changes. Route progress is measured along actual movement, allowing legitimate detours that initially move away from a destination. Stalled routes receive temporary exclusions and explicit failure/backoff after exhausted recovery. Travel hops resume through the same controller. Native interruption, stuck-route and performance acceptance remain pending. |

The stopped module compilation is not a successful build. The latest live
conversation attempt reached successful server authentication but timed out before
client authentication completed; see `PlayerbotCore/var/advai-gameplay/release-tls-conversation.log`.
No conversation or 2,000-online runtime acceptance is claimed for this batch.

## Surface pass: conversation, goals, economy, coordination and replacements

Source changes below are implemented together. Compilation, runtime scenarios and playtesting remain pending;
no builds or tests were run for this pass, as requested.

| Requested area | Source implementation | Scope and remaining acceptance |
| --- | --- | --- |
| Deeper conversation and bot exchanges | `AdvaiModel`, `AdvaiMgr`, `AdvaiDialogue`: recover recent informational subjects for short pronoun/quantity follow-ups; retain item/spell subjects; eligible sociable bots can ask a further authored question after introductions or small-talk answers | Questions still use delivered messages, personality eligibility, shared conversation limits and channel budgets. Context never replays an invitation or purchase command. Existing 60 variants per outcome remain. Native transcript review remains pending. |
| Long-term goals and relationships | `AdvaiCognition`: persistent equipment milestones pursue ordinary dungeon/raid/PvP activities; lower-level arena personalities pursue battlegrounds. `AdvaiSocial` and `AdvaiMgr`: known affinity ranks raid recruits and discourages repeated party/arena cooperation with disliked contacts | Milestone completion uses observed average item level, with no equipment grants. Existing persisted cognition fields carry the appended ambition kind. This adds a milestone family, not a full multi-stage life simulation. Restart and relationship scenarios remain pending. |
| Recipes, outgoing mail/COD and market memory | `AdvaiSupplies`, `AdvaiLifeStrategy`: preserve/use eligible owned profession recipes and opportunistically purchase one unfamiliar affordable vendor recipe during a supply visit. `SendCodOperation`: an existing agreed seller quote can use a real mailbox after an explicit `mail it cod` request. `AdvaiMarket`: hourly completed-sale units/revenue reload from the database; at most 64 accumulated sale buckets are submitted per world update | Native item use, vendor purchases, postage, faction restrictions and mail attachment checks apply. Dispatch does not create payment or award a completed-sale relationship event. Unknown incoming COD remains unaccepted; dedicated recipe-shopping itineraries and autonomous COD settlement are outside this surface pass. Apply `2026_10_17_06_advai_market_history.sql` before using this source; missing history schema fails startup. Sale history retains 24 hourly buckets, with hourly cleanup. Market restart and transaction scenarios remain pending. |
| Dungeon/raid coordination | `PartyUpdateOperation`: require tank/healer roles before another instance pull; mark the engaged tank's visible target when no skull mark already exists. Raid recruitment scores actual role eligibility, skill, guild membership and known affinity | Extends existing assembly, readiness, eligibility, travel and wipe/session budgets. Existing encounter-script marks take priority. This does not establish complete encounter-specific dungeon/raid coverage. Native role-loss, pull, recovery and encounter scenarios remain pending. |
| PvP objectives and arena cooperation | `AdvaiPvp`, `BGTactics`: select up to two preferred escorts from visible allies near a visible friendly flag carrier, favoring a healer; arena target scoring adds focus assistance using an observable damage teammate's actual victim | Existing native objective selection, pathing, CC/immunity exclusions and urgent role actions continue to execute gameplay. Escorts do not reveal unseen carriers or read other bots' plans. Native battleground and arena scenarios remain pending. |
| Dynamic replacement provisioning | `RandomPlayerbotMgr` requests replenishment when its eligible retained/fresh pool cannot fill requested slots; `Manager::UpdateWorld` pumps `RandomPlayerbotFactory::ProvisionAdvaiReplacement` | At most one new account/character per minute, one operation in flight, native character save acknowledgement polled across ticks, then normal identity registration and login selection. Existing characters are retained. Account/name/class/save failures are explicit; memory pressure prevents new requests. Existing startup reserve remains. Population exhaustion and restart-recovery scenarios remain pending. |

`advai stats` includes `provisioned_characters` for completed runtime provisioning in this process.

## Optional llama.cpp server inference

`AdvaiLlm` sends bounded asynchronous HTTP requests to an existing llama.cpp server;
it never starts an inference process or loads a model. `AdvaiLlmGame` supplies persistent
personality, current goals/mood, relationship facts, bounded recent exchanges and native
item/spell candidates. `Manager` connects structured decisions to live inventory answers,
native trade proposals, invitations, spare-consumable gifts and beneficial spell requests.
Server checks govern willingness, current inventory, learned spells, channel permissions
and native execution. Surplus consumable sales preserve ten carried units and craft
reservations. Explicit bot commands retain their original authority.

Human direct conversation has priority; bot exchanges have a separate smaller budget.
Replies share the delivered-message ledger, six-message exchange limit and channel budgets.
New messages, logout, changed location/activity and expiry invalidate pending inference.
Errors are operator-visible, and failed inference does not substitute an authored reply.
Configuration is opt-in through `conf/playerbots.advai.llm.preset`. See
[server integration and limits](LLAMA_SERVER.md). No compilation, inference request,
model launch or runtime test was performed for this implementation. Compatibility with
the actual model/server and factual quality of free-form chat remain acceptance work.

### Expanded chat, requests and autonomous playerbot interaction

| Surface addition | Implementation | Evidence/limits |
| --- | --- | --- |
| More conversation content | Broader LLM topic/response instructions and factual context; 12 additional small-talk prompts and 12 activity answers | Catalog generation produced 55 outcomes / 3,324 authored English variants and `2026_10_17_07_advai_social_conversation.sql`. Native transcripts remain pending. |
| More requests | 23 structured decisions including crafting, preparation, guild invitations, factual status and PvP coordination; broader agreement/cancellation phrases | Decisions connect to existing native handlers or real goals with need/recipe/consent/retry checks. Craft acceptance is not completion or delivery. No native/inference tests run. |
| Autonomous playerbot conversations | Ambient inference initiates an actual whisper to a visible eligible bot, ranked by relationship, shared experience, guild/group and level | Initiative has no fabricated incoming utterance and is chat-only. Real delivered messages enable recipient decisions and native cooperation. Same-peer cooldown, shared thread limits and bot inference budget apply. Other creature NPC scripts are outside this pass. |
| Cooperative outcomes | Existing invitations, guild acceptance, reciprocal gifts/trades, beneficial casts and newly accepted crafting/preparation goals | Both bots use the same action eligibility as human requests. Native transactions and subsequent gameplay must establish completion. Actual emergent sequences and load effects remain unverified. |
# Compact inference context (2026-10-17)

Conversation surface pass: 96 additional English variants (3420 total); expanded
ordinary grouping/location/activity phrasing, slang and explicit quiet requests;
scoped courtesy-negation handling; two-minute per-partner item/spell references;
deduplicated spell-rank candidates with explicit linked ranks retained. Inference
instructions clarify inventory, spell, craft and gift wording and prohibit unrelated
substitution. English catalog generation succeeded and produced repeatable migration
`2026_10_17_08_advai_plain_english.sql`. Native compilation, live inference and
in-game behavioral validation remain unperformed in this pass.

Supersedes the earlier 4096-token slot recommendation below. Entire user context
now uses compact fact records with a 1024-token maximum measured by the inference
server's tokenizer. Excess tail records are truncated; permitted item/spell IDs are
restricted to retained records. Current message/personality/needs and recent turns
take priority. Up to eight item/spell candidates each and three experiences fit
only as space permits. Full template preflight reserves 160 generated tokens and
32 boundary tokens within a 2048-token slot. Operator counters report context and
prompt token maxima and truncation count. Source-only implementation; no native
build or live inference acceptance run was performed.

# Native custom identities and inference slot budget (2026-10-17)

Source integration now includes native Cultist/Necromancer population selection,
account character caps, specialization roles and names, class-aware initial
professions and equipment scoring, Cultist plate/shields, and Necromancer caster
equipment. Initial class seeding precedes equipment selection; ported specializations
bypass the vanilla three-tree talent initializer. Dracthyr names use the ported race
entries. LLM facts include specialization, race, native class/race IDs, mana, actual
Cultist aura resources and living owned Necromancer summons. Friendly custom scripted
spell requests retain learned-spell, target, willingness and native casting checks.

Inference concurrency defaults to four and supports ten workers. Additional work
queues with FIFO order within human/bot priorities, a bounded backlog and configurable
120-second conversation deadline. Diagnostics expose slots and active HTTP requests.
The prompt and candidate/history bounds were reduced for a recommended 4096 tokens
per inference slot. These are source changes; no build, inference run or runtime
acceptance has been performed in this pass.

