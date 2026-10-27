# Sekiro: Shadows Die Twice — co-op notes

Session 1: 2026-10-27. Owner switched from Spore (see games/spore/NOTES.md: stopped because the
owner's copy was not from a store). Phase 0 (triage) + phase 1 (research).

## Game identity (owner)

- Store: Steam (owner-reported; install has no emulator/uninstaller files)
- Version: 1.06 (sekiro.exe ProductVersion 1.6.0.0)
- SHA-256 of sekiro.exe: 637aca527538c0ec6e1f136c8ed66046e95dfbdbb1f51926e134d9916398b856 (68,005,144 bytes)
- SHA-256 of steam_api64.dll: fc20547408a7c34f0bd4946a34c21aab48a75e3b98dce9e55969f486d37b212f
- `pack.dat` (512 bytes, looks encrypted) in the game root: purpose unknown
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

Whether Sekiro Online 2.05 still works on the current Steam client.

## Session 1 (cont.): audit of sekiro-coop @ 1ef5fa8 (2026-06-15)

### Observations
- Test copy at `<test dir>` (copy of the Steam install; same sizes/mtimes).
- Owner exe matches the mod's target: PE TimeDateStamp 0x5fa3e066 (= HANDOFF.md "1.06 exe stamp"),
  SizeOfImage 70066176 (= HANDOFF.md byte count).
- Bug: `crates/sekiro-sdk-sys/src/version.rs` has `SIZE_V1_06 = 0x042D_8000` with comment
  "= 70066176"; 70066176 is 0x042D_2000. Only used as fallback when AOB version detection fails.
- The Steam exe on disk has a `.bind` section and an encrypted `.text` (entropy 8.0, zero
  `48 89 5C 24 08` prologues). All 42 AOB patterns from aob.rs miss on the file: static AOB
  validation needs the decrypted in-memory image (runtime scan or a dump from the running process).
- docs/HANDOFF.md (2026-04-19) says: data plane validated vs peer-simulator; remote player NOT
  rendered (raw ChrIns position writes are ignored by render); planned unblock = patch
  `common.emevd` (UXM unpack + Yabber DCX) so the game's own EMEVD VM calls
  RequestAnimationPlayback / CharacterWarpRequest on a hijacked NPC puppet. Not started.
- CharacterWarpRequest warps to predefined points only, not XYZ (HANDOFF §5) — a smooth puppet
  likely still needs the native warp/position path.
- Repo vendors me3 v0.11.0 binaries under tools/me3/bin. Per rule 8 we take me3 from its official
  release (github.com/garyttierney/me3) with SHA-256 instead.
- Build needs Windows + MSVC; this cloud workspace cannot reach static.rust-lang.org or crates.io.

### Derived
- Building, launching and every live check must run on the owner's PC.
