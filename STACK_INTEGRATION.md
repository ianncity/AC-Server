# AC-Server integrated stack

This checkout is the canonical server source for the matching [AC-Client repository](https://github.com/ianncity/AC-Client). The current `Playerbot` core branch is integrated with the updated `mod-playerbots` source and the custom modules listed below.

## Included features

| Feature | Included source |
| --- | --- |
| Dracthyr and expanded race choices | `mod-playerbots`, `mod-expanded-races` |
| Cultist and Necromancer classes | `mod-playerbots`, `mod-ascension-classes`, `mod-cultist`, `mod-necromancer` |
| Prestige | `mod-prestige` |
| XP falloff | `mod-expfalloff` |
| Town raids | `mod-town-raids` |
| Talent overranking and talent shards | `mod-talent-overrank` |
| New item, dungeon loot, PvP, shop, and transmog content | `mod-archetype-weapons`, `mod-dungeon-loot-revamp`, `mod-pvp-bloodforged`, `mod-qol-shop`, `mod-prestige`, `mod-transmog` |
| Updated playerbots and bot-specific class behavior | `mod-playerbots` |
| Other world content | `mod-cut-content`, `mod-zone-wardens` |

The client repository contains the matching custom addons and installer for the expanded character creation UI. Build the DBC patch from your local client DBC files; Dracthyr race rows 12 and 13 must have `ClientPrefix=Dh` and `ClientFilestring=Dracthyr` so GlueXML can resolve their icons and preview models. Extracted DBCs, installed game files, and generated MPQs are not stored in this source repository.

Dracthyr can choose Warrior, Hunter, Rogue, Priest, Death Knight, Mage, Warlock, Cultist, and Necromancer; Dracthyr Druids are excluded. Orcs cannot be Cultists. The world database updater synchronizes `playercreateinfo`, starting actions/items, and class skills with the client table.

Keep `PlayerStart.CustomSpells = 1` enabled. This loads the custom starting spells, racial abilities, and language spells from `playercreateinfo_spell_custom` for Dracthyr, Cultist, and Necromancer characters.

## Build

Configure the overranking module as static, then build the server from PowerShell:

```powershell
cmake -S . -B var/advai-msvc143 -DMODULE_MOD-TALENT-OVERRANK:STRING=static
cmake --build var/advai-msvc143 --config RelWithDebInfo --target worldserver --parallel 1
```

Use the same configured module set when running the database updater. Back up the intended realm databases before deploying module migrations. The Dracthyr player creation and minion AI bindings are installed through `modules/mod-playerbots/data/sql/world/updates`. The playerbots character migrations are in `modules/mod-playerbots/data/sql/characters/updates` and are handled by the matching updater.

For the complete ADVAI preset, merge `modules/mod-playerbots/conf/playerbots.advai.preset` into `playerbots.conf` after reviewing its population and gameplay settings. The preset enables the Dracthyr and Ascension class integrations. Install the matching DBC patch and class modules before starting the server. Do not apply the optional legacy trainer fallback migration while `Playerbots.AdvAI.AscensionClasses` is enabled.

## Local DBC and client patch files

The merged server DBCs belong in `data/dbc`; the client archive installs as `Data/patch-6.MPQ`, with a matching locale overlay at `Data/enUS/patch-enUS-6.MPQ` so the 3.3.5 client loads the corrected CharBaseInfo after `patch-5.MPQ`. Generate the DBC bundle from local client inputs using `modules/mod-playerbots/apps/advai/integrate_dbc_bundle.py`; the client installer validates the MPQ against its manifest before copying it into both archive locations.

The Ascension glyph SQL at `tools/ascension-glyphs-staged.sql` is a candidate, not a deployable database migration. Add it to `data/sql/updates/pending_db_world/` only after the matching glyph DBC rows are installed in both the client MPQs and server `data/dbc`; the local client MPQs currently lack custom glyph property IDs 1200–1211.

The 12-button class selector is in the local client's `Interface/GlueXML/CharacterCreate.lua` and `CharacterCreate.xml`. The client installer applies these changes and installs `AscensionClassSupport`, `CultistUI`, `NecromancerUI`, `OverrankTalents`, and `QoLShop` from AC-Client.
