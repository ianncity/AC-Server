# Dracthyr, Cultist and Necromancer integration

The integrated PlayerbotCore stack uses Dracthyr race IDs 12 (Alliance) and 13
(Horde), plus standalone Cultist and Necromancer class IDs 12 and 13. The client
DBC patch, server class modules, and updated playerbots module share those IDs.
Bot class profiles are persisted with character identity, and their actions and
conversations use the matching class profile.

Both Dracthyr factions can choose Warrior, Hunter, Rogue, Priest, Death Knight,
Mage, Warlock, Cultist, or Necromancer; Dracthyr Druids are excluded. Orcs cannot
be Cultists. Cultist remains available to every other original race, while
Necromancer remains available to all original races. The integrated DBC
generator, character-creation SQL, and server startup validation keep these
restrictions aligned.

`conf/playerbots.advai.preset` enables Dracthyr and the Ascension class integration.
Returning characters retain their recorded class profile. When enabled, missing
backport spells, creation data, or required class content prevents startup.

## Build the content bundle

Run from the module checkout, substituting the paths for the installation:

```powershell
python apps/advai/backports.py --source ../WOW/Client/dracthyr_dbcs/DBFilesClient --base ../PlayerbotCore/var/advai-base-dbc --output ../PlayerbotCore/var/advai-backport/DBFilesClient
python apps/advai/integrate_dbc_bundle.py --server-dbc ../PlayerbotCore/var/advai-original-server-dbc --race-dbc ../PlayerbotCore/var/advai-backport/DBFilesClient --class-spell-source ../PlayerbotCore/var/advai-backport/DBFilesClient/Spell.dbc --output ../PlayerbotCore/var/advai-integrated-dbc
python apps/advai/backport_world.py
python apps/advai/backport_names.py
python apps/advai/backport_mpq.py --bundle ../PlayerbotCore/var/advai-backport/DBFilesClient --output ../PlayerbotCore/var/advai-backport/patch-ADVAI.MPQ --stormlib ../.advai-deps/stormlib-build/Release/StormLib.dll
```

The packaging tool uses the official Unicode StormLib build and reads each
archive member back to verify its SHA-256 hash. It refuses to overwrite an
existing archive. The bundle manifest identifies every generated DBC. Source
files are preserved. The supplied prototype's malformed CharBaseInfo string
block and reused appearance/model/display IDs are corrected during authoring.
Use an immutable vanilla DBC base, separate from the installed output. Vanilla
models are retained; Dracthyr displays use IDs 50000-50005 and isolated models
10000-10005. Both factions share the declared Draenei customization contract.
Duplicate indexed IDs fail bundle generation.

The integration merges the race rows and the 13 custom Cultist/Necromancer
spell records with the server's standalone class DBCs. It stages the complete
server/client DBC set, including Spell.dbc rows required by the registered
custom-class handlers. Stop the world process
before installing those tables on another server, and retain its original DBCs.
For database setup, use the migrations as follows:

- Characters: the module database updater applies
  `2026_10_17_00_advai_initialization_receipts.sql` and
  `2026_10_17_01_advai_dracthyr_names.sql`.
- World: apply `optional/2026_10_17_00_advai_dracthyr.sql` for Dracthyr
  character creation. The standalone class migrations are supplied by
  `mod-ascension-classes`. Use `optional/2026_10_17_01_advai_classes.sql` only
  for the fallback class path with `Playerbots.AdvAI.AscensionClasses = 0`.
- Class-owned Ascension content: `mod-ascension-classes` already supplies the
  same class world rows as
  `optional/2026_10_17_02_advai_ascension_class_content.sql`; apply one source.

These migrations target the supported Playerbot core's current schema. Do not
apply the older prototype SQL, which uses different tables and contains damaged
escape characters. The companion persistence change in
`PlayerbotCore/src/server/game/Entities/Pet/Pet.cpp` allows Warlock-based casters
to retain undead companions through the native pet system; carry that core
change when deploying to another checkout. The required core patch, including
permission-aware channel delivery, is in [core/README.md](core/README.md).

The integrated client patch installs as `WOW/Client/Data/patch-6.MPQ`.
Install the matching locale overlay as `Data/enUS/patch-enUS-6.MPQ`
as well, so locale archives cannot shadow the corrected `CharBaseInfo.dbc`. Retain
the earlier class and Dracthyr archives because they provide models, textures,
and other supporting assets. The patch is staged beside its manifest, and the
original DBC inputs remain backed up under `PlayerbotCore/var/`.

## Ascension Cultist and Necromancer content

The source is the installed Ascension client at `E:\Ascension\Ascension`.
Gameplay progression comes from `Content/CharacterAdvancementData.json`, the
source spell ranks, and the supporting class spell data. `SkillCardData.json` is
excluded. The catalog contains 356 advancement nodes (160 Necromancer and 196
Cultist), seven specializations, and 80 level budgets for each class. Those
budgets and their prerequisite, specialization, rank, and point costs are carried
through level 80. The source class spell closure contains 2,758 records; 2,754
new WotLK `Spell.dbc` rows and their required support rows are in the generated
bundle.

`WOW\Client\Data\patch-ZZZZZ-AscensionClasses.MPQ` contains 23 WotLK DBC tables
and 1,622 referenced assets, including 2,754 new `Spell.dbc` records. All 2,446
active client asset references resolve to packaged data. The complete `F:\Ascension`
backup scan found the missing model and audio files; no active sound references
are muted or substituted. The MPQ was read back and SHA-256 verified.

Both classes use standalone server class IDs 12 and 13, the 3.3.5 spell data
layout, and the existing client protocol. Spell rank chains, effect
coefficients, and level rules use the native WotLK spell system, with server
hooks in `src/Advai/Ascension/` for Ascension-only mechanics. Creature display
IDs are collision-safe, and the final DBC model paths match packaged `.m2` and
texture files. There are 356 advancement nodes and 389 distinct advancement
spells; all 31 multi-rank nodes have complete WotLK rank chains. Each class has
level budgets from 1 through 80 (36 AE and 35 TE at level 80). Characters earn
and spend those budgets on eligible nodes; the budget intentionally constrains
each build rather than granting every node at once.

The module's matching class content migration supplies the class-owned
creatures and spell dependencies, including rank associations and model IDs
used by the final client bundle. It excludes Ascension's full world database
and all Skill Card data.

## Gameplay contracts

Fresh characters receive an immutable initial level and class profile before
resources are seeded. A full character save and initialization receipt commit
together; a restart resumes only unfinished initialization. Existing played
characters are adopted without replacing their possessions or progress.

Cultists retain normal Shadow Priest behavior and gain temporary void servants.
Necromancers use mana-based versions of Frostbolt, Icy Touch, Death Coil, Bone
Shield and Corpse Explosion, plus skeletal companions and a Flesh Golem. Summons
use real spell casts, mana, cooldowns and native companion saves. The Flesh Golem
requires a Soul Shard. Companions have explicit level 1-80 statistics and actual
attacks. Grouped Necromancers disable companion taunt autocasting.

Configured Cultist and Necromancer profiles receive their starting kits once,
then the progression controller learns eligible advancement ranks as the
level 1-80 budgets grow. Native instructors train the standalone class IDs;
their copied Priest/Warlock spell catalogs are filtered by the class-specific
trainer checks.
Trainer state checks reject another profile's spells before taking payment.

Dracthyr gain Glide, Tail Swipe, Wing Buffet and gender-appropriate Visage.
Visage is cosmetic and breaks on damage or attacks. It does not grant the
prototype's unintended movement or regeneration bonuses. Glide does not require
a reagent. All combat racials respect native cast checks and cooldowns.

These are implementation contracts. Runtime acceptance and the complete plan's
remaining release blockers are tracked in [COVERAGE.md](COVERAGE.md).
