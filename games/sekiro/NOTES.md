# Sekiro: Shadows Die Twice — co-op notes

Session 1: 2026-10-27. Owner switched from Spore (see games/spore/NOTES.md: stopped because the
owner's copy was not from a store). Phase 0 (triage) + phase 1 (research).

## Game identity (owner)

- Store: unknown
- Version: unknown (both known mods target 1.06, the latest Steam patch)
- SHA-256 of sekiro.exe: unknown
- Engine: FromSoftware proprietary, x64, Windows
- Wanted mode: online (friends on their own PCs)

## Prior work in this repo

- `data/directory.yaml` has two Sekiro entries (checked 2026-10-16): Sekiro Online, sekiro-coop.
- No `games/sekiro/`, no issues or PRs mentioning Sekiro (checked 2026-10-27).
- playbook/engine-families.md lists Sekiro under FromSoftware; Seamless Co-op internals not public.

## Observations (with sources)

1. **Sekiro Online** — https://www.nexusmods.com/sekiro/mods/577
   - v2.05, last updated 2021-06-13 (fix for a Steam client update of 2021-06-08).
   - Page: no longer developed; "a new version is being made as of 09/03/2025". Nothing newer found.
   - Souls-style: summon signs (server only for signs), P2P over Steam, Steam friends invites,
     1 host + 5 phantoms, invasions (cannot be disabled), level scaling.
   - Must finish the tutorial and reach the Dilapidated Temple first.
   - Known issues: crashes, stuck after a deathblow on a dead enemy, versions can't mix.
   - Required game version not stated on the page. Closed source.
2. **sekiro-coop (Rust)** — https://github.com/mstampfli/sekiro-coop
   - Seamless 2-player design for Sekiro 1.06 (Steam), injected via me3, MinHook.
   - README: not end-user playable; remote player is not rendered in the world (main blocker).
   - Has: 60 Hz player snapshots, reliable flag/item/XP/warp events (apply behind an env var),
     host enemy state at 5 Hz, desync hash every 60 frames. Rollback written but not live.
   - Networking: raw UDP direct IP, no NAT traversal; Steam P2P is an empty stub.
   - 6 commits, 0 stars, no releases. AGPL-3.0-or-later.
3. No Seamless Co-op (LukeYui) release for Sekiro found in searches.

## Derived

- Fastest route to playing with friends: Sekiro Online (works today per community reports,
  untested by us; needs Steam on every PC).
- Seamless story co-op: only sekiro-coop, a prototype. Path 1 (extend) work would be: render
  the remote player, wire Steam relay (Spacewar) into the empty Steam P2P stub, then QA.

## Unknown

- Owner's store/version/exe hash. Whether Sekiro Online 2.05 still works on the current Steam client.
