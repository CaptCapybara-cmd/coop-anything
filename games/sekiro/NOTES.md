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

## Session 1 (cont.): first live run, 2026-10-27

Setup (owner PC, commands run by owner):
- sekiro-coop @ 1ef5fa8 from the GitHub archive; toolchain 1.78.0 (from rust-toolchain.toml), MSVC 2022.
- First `cargo build` hit a one-off rustc STATUS_ACCESS_VIOLATION on sekiro-sdk-bridge; an identical
  rebuild passed. (`-C linker-plugin-lto` in that command is cargo's own flag for `lto = "thin"`.)
- MinHook: vendored `vendor/minhook/{src,include}` is byte-identical to official TsudaKageyu/minhook
  @ d94c64d. Rebuilt `MinHook.x64.lib` from it with cl/lib (/MD /O2) via vcvars64.bat and linked with
  MINHOOK_LIB_DIR instead of the prebuilt `vendor/lib/MinHook.x64.lib`.
- me3 v0.11.0 `me3-windows-amd64.zip` from the official release: SHA-256
  1ddc6c3d9468aa6c7f99bd6b080aa94977e0d6fab771368ca89387a1c8cda4b7 = GitHub asset digest (and = the
  zip vendored in sekiro-coop). me3.exe / me3-launcher.exe Authenticode Valid, signer Gary Tierney.
- Launch: `me3 launch -g sekiro -e <test dir>\sekiro.exe --disable-arxan true --online false
  --savefile coop-test.sl2 -p sekiro-coop.me3`. Owner saves backed up first.

### Observations (log, solo, no session)
- me3: "Sekiro: Shadows Die Twice 1.06.0.0 Worldwide"; dearxan reports arxan detected: false;
  savefile override to coop-test.sl2 applied.
- Mod: `module loaded version=V1_06 size=70066176 base=0x140000000 has_base_addrs=true`
  (AOB-based detection, so the SIZE_V1_06 typo did not matter).
- `hook installation finished installed=6 total=6`: SetFlag 0x1406c4520, ApplyEffect 0x1409f54a0,
  DeleteEffect 0x140bfb6d0, GiveItem 0x14091c970, AddExperience 0x1407c0ef0, WarpBonfire 0x14068e870.
- Player state readable (hp 320/320, posture 120/120, anim, pos, IGT). SetFlag edge events and
  ApplyEffect/DeleteEffect calls logged. Param tables found: AtkParam_Pc 443, AtkParam_Npc 2396,
  SpEffectParam 3096, BehaviorParam 3128, NpcParam 1279, ItemLotParam 1926.
- chrins registry count=2 shortly after load. No crash during the new-game opening.

### Not yet checked
- UDP session (two instances or peer-simulator), remote apply, enemy-state broadcast.

## Session 1 (cont.): data plane vs peer-simulator, 2026-10-27

Setup: peer-simulator (built from the same tree) `--bind 127.0.0.1:28001 --peer 127.0.0.1:28000
--as client --fake-events`; game as host with SEKIRO_COOP_BIND=127.0.0.1:28000,
SEKIRO_COOP_PEER_ADDR=127.0.0.1:28001. Loopback only.

### Observations
- `session established seed=MatchSeed(...)` within seconds of load.
- Remote snapshots arrive (`remote=peer=Client hp=420/420 pos=(..) anim=790010/790020`).
- Remote BridgeEvents arrive every 300 frames (EventFlagSet 11100100, SpEffectApplied 4800).
- Host EnemyStates broadcast; echo resolved: `matched=2 unmatched_count=0`, `applied=0`
  (SEKIRO_COOP_APPLY_REMOTE not set), divergences = the simulator's HP-1.
- peer-sim log: snapshots sent every second, EnemyStates echoed with HP-1.
- No desync kill, no crash. Earlier run without a listening peer: UDP `os error 10054` every 2 s,
  retry loop works, game unaffected.
- => Reproduces the author's data-plane claims on this machine (QA level: one game client +
  simulator; not two real clients).

## Session 1 (cont.): puppet probe v2 — remote-player rendering blocker, 2026-10-27

Change (local branch `coop/puppet-probe` on top of 1ef5fa8, not published): new
`crates/sekiro-coop-dll/src/puppet.rs`, enabled by SEKIRO_COOP_PUPPET_PROBE=1. Pins the nearest
team-6 ChrIns within 12 m and writes Hero+(3,0,0) to its physics-module position
(`ChrIns+0x1ff8 → +0x68 → +0x80`) from inside the SetFlag detour; every hop VirtualQuery-checked,
handle at ChrIns+0x08 re-checked before each write.

### Observations (log + owner watching the screen)
- Pinned `char_id=10201000 handle=268484639 dist=11.9`.
- Writes ran on ThreadId(3) (the SetFlag caller); the fallback ticker is ThreadId(2).
- ~200 writes/s; the game changed the value between two of our writes ~8% of the time
  (180 of 2273). Read-back position tracked Hero+3 m.
- Owner: the enemy visibly teleported next to (behind) him and killed him. => Writing the physics
  position from the game thread DOES move a character on screen.
- After death/reload the old ChrIns pointer no longer held the handle; probe logged stale=1, dropped
  the target, no crash. The same handle later reappeared in the registry with a new pointer.
  No re-pin happened: registry (fed only by ApplyEffect) had no team-6 entry within 12 m.

### Derived
- docs/HANDOFF.md §3 conclusion "render path ignores physics-module writes" does not hold. Its
  experiment wrote from the 60 Hz ticker thread and read back immediately. Position for a puppet
  can be driven from a game-thread hook without the EMEVD/UXM pipeline.
- Still open for a usable puppet: stop its AI/hostility, play the remote player's animations,
  use a character that looks like the player, keep a target across reloads.
