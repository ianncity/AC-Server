# Supported core changes

The integration checkout is the `Playerbot` branch of
`https://github.com/mod-playerbots/azerothcore-wotlk`, based on commit
`8a4e76eac8c4`. The adjacent patch contains the core changes required by ADVAI:

- Retain undead companions owned by a Warlock-based Necromancer through the
  normal permanent-pet path, including their client action bar and saved state.
- Expose `Channel::TrySay`, which reports rejection for missing membership or a
  muted membership. This lets ADVAI record only delivered channel speech without
  reading private channel internals or bypassing their permission checks.
- Supply native Ascension spell and aura state, metadata and combat hooks,
  sequential durable charge recovery, summon-only area auras, resolved damage
  accounting, conditional critical modifiers, maximum aura stack modifiers, and
  temporary spell replacements with cast routing and durable action-bar origins.
- Initialize MySQL client TLS in every database-using thread and repair prepared
  result cleanup paths that could free metadata or bindings twice after errors.

The Ascension implementation is adapted from
[`jealous-sound/azerothcore-wotlk-coa`](https://github.com/jealous-sound/azerothcore-wotlk-coa)
at `0487265f863b9c50e7757e1f113169b80505ba8b`. Its GNU AGPL v3 notices are retained;
source paths and hashes are recorded in `ascension-upstream.json`.

Native game/database/scripts compilation has passed for the core mechanics.
Class content packaging, the final module build, and runtime class scenarios
remain release blockers. Core ABI changes require rebuilding the game, module,
and all script libraries before linking a realm executable.

Apply it to a compatible clean supported-core checkout:

```powershell
git apply ../playerbotadvai/apps/advai/core/2026_10_17_advai_backports.patch
```

The working integration checkout already contains these changes. Reapplying
the patch there is unnecessary. The optional world/DBC content is described in
[BACKPORTS.md](../BACKPORTS.md).
