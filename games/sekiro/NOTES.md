# Sekiro: Shadows Die Twice — co-op notes

Session 1: 2026-10-27. Phase 0 (triage) through a first round of live probes.

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

### Run 3 (2026-10-27 16:30): stale build, FPS
- The new puppet.rs (team write / re-pin / heading) did not reach the owner's disk: the file transfer
  delivered the previous version (9175 bytes instead of 12490). Log confirms: stats lines had no
  `repins`/`team_*` fields. Owner saw the same behaviour as run 2.
- Owner: FPS fell to ~15 while the puppet was held. Log: game moved the puppet's height by
  ~0.3-0.4 m against every write (Hero height on a slope = inside terrain), ~300 writes/s
  (several per frame). Fix: keep the game-resolved height, max one write per 8 ms.
- Re-sent with a read-back SHA-256 check of the file on the owner's disk.

### Run 4 (2026-10-27 16:35): team write + re-pin + heading, game-resolved height
- FPS normal (owner). Pin and re-pin worked: `re-pinned (same handle)` after death, repins=1.
- Team byte ChrIns+0x74: 6 → 2 written once per pin (team_before=6, team_now=2). Whether the puppet
  stopped attacking: not observed clearly yet. Other enemies (team 6) attacked as usual.
- Keeping the game-resolved height made the puppet fall through the world: y −43 → −66 → −98 →
  … → −265 m, anim 10000/10001 near the end, then stale/despawn. Owner saw it "disappear".
- Fix: keep the game's height only within [Hero−1 m, Hero+2 m], else Hero+0.2 m.

### Run 5 (2026-10-27 16:43): height reset
- Puppet held ~3 m ahead of the Hero for the whole session (2467 writes, 0 stale, 5 height resets,
  FPS normal). Owner: first frame behind (heading starts as +X), then in front after moving.
- Team byte stayed 2 (from 6), but the puppet kept attacking (anims 3000/3002) and missed because it
  is re-placed every 8 ms. => Team 2 alone does not stop its AI.
- Next: global debug flag all_no_update_ai (debug_flags+13) as a test aid; per-character AI
  disable still to find.

### Run 6 (2026-10-27 16:47): global AI freeze (debug_flags+13: 0 → 1)
- Owner: puppet no longer attacks; it has no animation; it follows only when the owner circles near
  it, and stays behind when he walks straight ahead.
- Log: even with all AI frozen the game reverted ~80% of our writes (2530/3147). Distance to the
  Hero mostly 3.0 m, sometimes 6-9 m while walking straight.
- Derived: the reverting copy is not the AI; most likely the physics engine's own copy (character
  proxy) syncing back into the physics module each frame. Writes stick only when they land at the
  right point in the frame; big jumps are visible, small per-frame steps are lost.
- Next: read-only mirror scan from the ChrIns (BFS through pointer fields, depth 3, ≤4000 objects,
  VirtualQuery-checked) for every float triple equal to the position, twice 10 s apart, with writes
  paused 1 s before each scan.

### Run 7 (2026-10-27 16:53): mirror scan from the ChrIns
- Scan A (puppet had reverted to its spawn point during the pause): 8 hits, mostly spawn/home-position
  copies: chr+0xe0, chr+0x1050, chr+0x1060, chr+0x50>+0x1a0, chr+0x58>+0x5a0, chr+0x18>+0x28>+0x1a0,
  chr+0x50>+0x18>+0x58>+0x138, chr+0x58>+0x340>+0x0>+0x258.
- Scan B (puppet away from spawn): 3 hits: chr+0x1050, chr+0x1060, chr+0x18>+0x28>+0x80.
- Both scans hit the 4000-object cap; the known path chr+0x1ff8>+0x68>+0x80 was not reached.
- Derived (guess): chr+0x1050/+0x1060 = live transform copies inside ChrIns;
  chr+0x18>+0x28 likely another path to the physics module (same +0x80).
- Next: write position to chr+0x1050/+0x1060 as well, count per-copy reverts; scan rooted at the
  physics module object.

### Run 8 (2026-10-27 ~16:57): write chr+0x1050/+0x1060 too; scan from physics module
- Owner: FPS ~20 again; puppet "hard to attach", follows a few metres when walking straight, then
  drops behind.
- Per-copy reverts: chr+0x1050 reverted 2 times in 7056 writes (not maintained by the game, so
  probably not a live copy); chr+0x1060 reverted together with the physics position every time
  (1851 vs 1856). Overall revert rate ~26%, height resets 57.
- During each 1 s write pause the puppet snapped back exactly to its spawn point
  ([-198.86, -40.00, 243.24], same as run 6/7 scan A). Both scans therefore saw the home position.
- Scan P (from physics module object, 3434 objects): phys+0x90, phys+0x1a0, phys+0x8>+0xe0,
  phys+0x8>+0x50>+0x1a0, phys+0x8>+0x58>+0x5a0. phys+0x8 is a back-pointer to the ChrIns
  (same suffixes as scan C).
- Derived: something re-applies a stored home/target position to the physics module every frame.
  Position-poking fights that system; the native move/warp path (or the game's remote-character
  sync) is what needs finding. Per-launch trial and error through the owner is too slow for that.
- Proposed next: dump the decrypted sekiro.exe image from memory (read-only, kept in local/,
  never committed) and analyse it offline (disassembler) to find warp / animation / per-character
  AI-disable natives and how DS3-lineage player slots are spawned.

## Session 1 (cont.): offline analysis of a 1.06 memory image, 2026-10-27

Image: `sekiro.exe` module read from the running game with ReadProcessMemory (70,066,176 bytes,
SHA-256 33533c5a…; kept in `local/`, never committed). `.text` decrypted (entropy 6.65, 27,572
`48 89 5C 24 08` prologues); 32 of sekiro-coop's 42 AOBs hit uniquely (DeleteEffect misses because
the dump was taken with the mod's detours installed). Tools: GNU objdump on the raw image, a small
MSVC-RTTI walker (type descriptor → complete object locator → vftable).

### Observations (RVAs for Steam 1.06, module base 0x140000000)
- Script commands are local classes in `NS_SPRJ::EventEntityCommandUtil::ChrCmd_*` (RTTI), each with
  one method that forwards to an `EventChrEntity` (vftable 0x29ac0b8, ChrIns pointer at +0x10):
  - `ChrCmd_EnableLogic(int id, bool)` → clears/sets bit 0 of `[ChrIns+0x50]+0x190` (ChrCtrl);
    also zeroes 16 bytes at +0x80 of another per-character object. RVA 0x9ede00.
  - `ChrCmd_SetDisableGravity(int, bool)` → `[[ChrIns+0x1ff8]+0x68]+0x92b = b` (0x9f5450).
  - `ChrCmd_SetDisableMapHit(int, bool)` → bit 0x10 of `[ChrIns+0x50]+0x194` (0x7de220).
  - `ChrCmd_Warp(int point, bool)` → resolves the point to pos/rot, then
    `ChrIns->vt[+0x6A0](chr, const float4* pos, const float4* rot, 1, 0)` (0x9f81f0, same slot for
    ChrIns/EnemyIns/PlayerIns), then `ChrIns+0x1a10 |= 2`.
  - vt[+0x6A0] → vt[+0x698] (0x9f8100): sets `ChrIns+0x1af5`, calls `ChrCtrl->vt[+0x70](pos, rot, a, b)`,
    then `vt[+0x680]()`, `vt[+0x678](a)`.
  - Rotation quaternion: physics module +0x70.
  - Other command classes present: SetTeamType, WarpDmy, ChangeThink, ResetThink, EnableInvincible,
    SetNetworkUpdateValue, ChrResetAnimation.
- RTTI also has ChrManipulator / ComManipulator (vftables 0x29d2ae0 / 0x29d2fb8).

### Derived
- Puppet v3 uses ChrIns::Warp (vt+0x6A0) on the game thread (guarded by checking the slot equals
  base+0x9f81f0) and per-character EnableLogic off instead of the global debug flag.

### Run 9 (2026-10-27 ~21:05): puppet v3 (native warp every 33 ms + per-character logic off)
- Owner: puppet does not attack (per-character EnableLogic off works without the global AI freeze),
  FPS normal, follows the player, but flickers. Log of that run was lost (next launch cleared it).
- Guess: vt[+0x698] resets per-warp state (`ChrIns+0x1b94 = 1.0f` when 0xa4a650 is true; render
  interpolation), so warping 30×/s flickers.
- Next: hybrid — native warp every 500 ms or when >2 m off, raw physics steps every 8 ms between.

### Run 10 (2026-10-27 21:15): hybrid move
- Native warp every 500 ms or when >2 m off (108 warps in ~40 s), raw physics steps every 8 ms
  between (2742). Distance to the Hero held at 3.0-3.3 m, stale=0, logic bit stayed cleared
  (`[ChrIns+0x50]+0x190 = 2`), no attacks. Owner: moves smoothly, flicker "practically gone", FPS normal.
- Between warps the game still moves it ~60% of the time (overwritten 1689/2850), but small enough
  not to show. The puppet keeps playing its own animation (405010, occasionally 7020).
- Puppet state: position solved. Open: drive it from the network snapshot of the remote player,
  play the remote player's animations, look like the player.

### Run 11 (2026-10-27 21:27): puppet driven by the network (SOURCE=net) + peer-simulator --mirror
- Failed: remote_fresh=0, remote_stale=1380. The simulator handshook, then sent nothing while waiting
  for the DLL's first snapshots (sent only once in game); the DLL's grace expired after 9 s
  ("peer grace expired" every frame, 3,700 log lines) and no snapshot ever arrived.
- With no remote position the puppet step returned before logic-off/team write, so the pinned enemy
  stayed hostile (team 6, logic on) and killed the owner. Then the game closed without an error
  dialog after the death/reload; cause not in the log.
- Fixes: simulator sends heartbeats while waiting; puppet is neutralised (logic off, team 2) before
  any early return; grace log rate-limited.
- Crash cause (Windows Application Error log): AV 0xc0000005 in sekiro_coop.dll at RVA 0xac37c
  (same DLL, PE stamp 0x6ae0ed0a). Disassembly: sekiro-coop's `live::chrins_snapshot` reading
  `[[ChrIns+0x1ff8]+0x18]` on a freed ChrIns. Called from the host EnemyStates broadcast, which only
  runs with a session (earlier solo runs never hit it). Registry entries outlive death/reload.
- Fix: prune the registry every frame (handle at +0x08 must match, every hop VirtualQuery-mapped)
  and re-check right before each snapshot/HP write.
- Side note: the owner's event log also shows AVs in cargo.exe (21:11) and powershell.exe (20:55)
  in unknown modules, plus the earlier rustc AV during the first build. Not explained; worth
  keeping in mind if unrelated crashes appear.

### Run 12 (2026-10-27 21:35): network-driven puppet, second try
- Registry fix held (no crash). Puppet neutralised (no attacks) but "just runs straight": no
  session, so no remote position; with no position write its last animation carried it away.
- No session: the simulator's single Handshake went out 0.03 s before the DLL bound its socket
  (os error 10054) and was lost; the simulator then received the DLL's handshake and declared the
  session up, while the DLL kept timing out and re-sending. sekiro-coop's handshake sends once and
  waits, so whichever side starts first can lose its packet.
- Fixes: simulator answers every Handshake retry; puppet holds its last position without data.

### Run 13 (2026-10-27 21:43): network-driven puppet works
- Launch via a .cmd script (a pasted command had been cut, launching vanilla without the profile or
  the save override; the main save was written but no mod was loaded, so nothing modded touched it).
- Session up (the simulator answered one handshake retry); remote_fresh=2383, remote_stale=0; ~2
  warps/s. Owner: the puppet follows his path as intended, but jitters and does not turn.
- The game moved it between our steps ~80% of the time (1820/2383): its own run animation's root
  motion while logic is off.
- Offline: `ChrCmd_ChrResetAnimation(false)` ends in `0x9f4c60(chr)` = reset of the animation module
  `[ChrIns+0x1ff8]+0x28` via 0xb442c0 (the `true` variant 0x9f4c80 also sets
  `[[ChrIns+0x50]+0x20]+0x1334 = 1`).
- Next (puppet v4): reset animation every 2 s (byte-checked call), yaw quaternion (0, sin, 0, cos)
  from the target's movement written to physics +0x70 and passed to the warp, eased target.
  Logs hero rotation and heading to calibrate the yaw convention.

### Run 14 (2026-10-27 21:50): puppet v4 (animation reset, yaw, easing)
- Animation reset ran every 2 s (29 resets, 0 refused by the byte check), no crash.
- Owner: puppet faces "inverted" relative to the player, jitters when turning.
- Log: the Hero's physics +0x70 reads `[0, θ, 0, 0]` with θ in radians (±π), not a quaternion; we
  had written (0, sin, 0, cos). Calibration over 8 samples of the Hero's movement direction
  (dx, dz): θ = atan2(dx, dz) + π = atan2(-dx, -dz), all within ~0.15 rad.
- Fix: write `[0, yaw, 0, 0]`; recompute the yaw goal only after 0.15 m of movement and ease toward
  it along the shorter arc.

### Run 15 (2026-10-27 21:53): yaw fixed
- Owner: the puppet now faces the way it moves. It does not walk: it slides/teleports. It is in the
  reset (idle) pose; between native warps (every 500 ms) the game still pulls it back, so it jumps
  on each warp.
- Next: (a) quick check with a shorter warp interval now that its animation is reset (flicker in
  run 9 may have come from fighting its run cycle); (b) real walking needs the puppet's own
  locomotion driven by a movement input (ChrManipulator / ChrCtrl) or a native play-animation call —
  not found yet.

### Run 16 (2026-10-27 23:35): native warp every 50 ms
- Owner: the puppet follows him smoothly with a running animation.
- Log: ~16 warps/s, raw steps between, remote_fresh=1728, remote_stale=0, animation resets every
  2 s. The puppet's animation switches between 405001/405003/405010 (its own walk/run family):
  frequent native warps move it like real movement, and the game's locomotion picks the cycle.
  Flicker seen in run 9 (33 ms) is gone now that its own animation is reset.
- 50 ms is the new default.
- Status: a network-driven, harmless, smoothly walking/running puppet that faces its direction of
  travel. Still open: turning in place (no yaw in PlayerSnapshot), actions (attacks, deflects,
  jumps), the player's appearance, Steam relay, a real two-PC test.

### Run 17 (2026-10-27 23:42): yaw in PlayerSnapshot
- PlayerSnapshot gained `yaw: f32` (Hero physics +0x70 element 1); the puppet turns to the remote
  yaw (fallback: direction of movement); the simulator mirrors it. Wire format changed: DLL and
  simulator are rebuilt together.
- Owner: the puppet turns with him, including in place. When he stops, it sometimes keeps its run
  animation (the reset only ran every 2 s).
- Fix: reset the animation as soon as the target has not moved 5 cm for 200 ms.

### Run 18 (2026-10-27 23:47): stop reset
- Owner: mostly stops correctly; occasionally the run animation stays.
- Guess: a periodic warp after the reset reads as movement and restarts the cycle.
- Change: no periodic warps while the target stands (only to fix >2 m), and follow-up resets every
  400 ms for 1.7 s after a stop.

### Run 19 (2026-10-27 23:49): no simulator
- Owner: the puppet froze in place in its run animation.
- Log: remote_fresh=0 for the whole run; the DLL got os error 10054 on 75 handshake attempts; the
  simulator log ended at the previous run (it never started this time; cause unknown, likely a
  leftover process/log handle). The stop logic never ran because no data came.
- Fixes: reset the puppet's animation when network data stops; launch script deletes old logs,
  starts the simulator, checks it is running, and refuses to start the game if it is not.

### Run 20 (2026-10-27 23:56): stop handling confirmed
- Launch script reported the simulator running before the game started.
- Owner: "everything works as it should" — the puppet follows, turns with him, and drops the run
  animation when he stops. One run, owner-observed; no automated check.

## Session 1 (cont.): actions — native ForcePlayAnimation (offline, 2026-10-28)

### Observations (1.06 image)
- RTTI: `EventEntityCommandUtil::ComCmd_ForcePlayAnimation(int, int)` (vftable 0x29ac598, method
  0x754720) and `ComCmd_PlayAnimation` (0x29ac588 / 0x754800) forward to `EventChrEntity` vt[+0x38]
  and vt[+0x30].
- vt[+0x38] = 0x750e00: `0xbd84c0([[ChrIns+0x1ff8]+0x58], anim_id)`. 0xbd84c0 stores the id at
  module+0x10 and at `[owner+0x20]+0x24` (owner via 0xb271d0). vt[+0x30] = 0x751120: same call,
  but only when 0xbd8430 says the module is idle (PlayAnimation vs ForcePlayAnimation).
- RTTI also lists `NetworkManipulator` and `NetAIManipulator`: probably how the game drives
  remote players; not analysed.

### Run 21 plan (puppet v5)
- Learn the remote's rest animation (unchanged ≥1 s while still). When the remote animation
  changes to something else while standing, call 0xbd84c0 (byte-checked) with
  `SEKIRO_COOP_PUPPET_ACTION_ANIM` (default 3000, an enemy attack id seen in run 8). Stop and
  periodic resets are held for 1.5 s after it. Every remote animation transition is logged, to
  map player animation ids to actions. Launch script copies the DLL log next to it.

### Run 21 (2026-10-28 ~01:35): ForcePlayAnimation works
- 10 forced calls, 0 refused; 300 ms later the puppet's animation read 3000 in 9 of 10 cases
  (once 7000). Owner: the puppet swings when he attacks, dodges or blocks, and jumps when he jumps.
- The puppet's jump likely comes from the position/height path, not from the forced anim (guess).
- Remote (= owner's) animation ids, in the owner's test order (attack ×3, block ×2, jump ×2,
  dodge ×2, run and stop). Derived from order, not confirmed:
  - rest 790010 (also 0); attack 790040; block 50203000 → 50203010;
  - jump 201001 → 201011 → 201040; dodge 213300 / 213301; run/stop 1151, 1200, 1500, 1510.
- Unknown: whether the owner had the katana (790040 may be an unarmed swing).

### Run 22 plan
- Force only mapped ids (default 790040 → 3000), also while moving. Catalog probe: while the
  owner stands still, play 18 candidate enemy ids, one every 3 s, logging whether each took,
  so the owner can name the block, dodge and jump lookalikes. Mirror offset 5 m so swings miss.

### Run 22 (2026-10-28 01:45): catalog
- Rebuild needed a second try: rustc STATUS_ACCESS_VIOLATION again (third AV of a dev tool on the
  owner's PC; cause unknown).
- Catalog on c1020 (char 10201000): 3001, 3002, 3003, 3005, 3010 took (puppet anim = id after
  300 ms); 4000, 5000, 5001, 5010, 6000-6002, 7000, 7001, 7010 did not (puppet stayed in 405002).
  Owner: no. 2 a jump attack, no. 3 a lunge, then the lunge again, then nothing visible.
- Remote id 790040 also stayed for >1 s while the owner stood, and a 3-4 hit combo did not change
  the id between hits. Guess: `+0x1ff8 → +0x10 → +0x20` is a base layer / state (790010 idle,
  790040 combat idle after a swing), not the attack animation. Block shows as 50203000 →
  50203010 → 50203012 (and 400 in between while moving).
- Run 23 plan: watch u32 fields of Hero sm+0x10 / sm+0x28 / sm+0x58 / chr+0x20 for anim-like
  values (6-8 digits) to find the attack slot; fast catalog (152 ids, 0.4 s each) that logs which
  ids the puppet accepts.

### Run 23 (2026-10-28 01:52): action slots found
- Fast catalog (152 ids, 0.4 s each) on c1020: accepted 3000-3010, 3012, 3014, 3015, 3020, 3100.
  Nothing in 4000-9900, 7001-7030, 10000-10040, 400000-410000 took. Owner: a jump-like one
  around no. 2-3 (3001/3002), dodge-like around no. 4-5 (3003/3004).
- Hero slot scan: the TimeAct object `[ChrIns+0x1ff8]+0x10` has two rings of 5 entries, stride
  0x28. Ring at +0x20 (+0x48, +0x70, +0x98, +0xc0) = base layer (700000, 790010, 790500, 790040,
  201001, 50203000, 213300, 1151 ...). Ring at +0x34 (+0x5c, +0x84, +0xac, +0xd4) also gets the
  combo hits 50300000, 50300100, 50300010, 50300110, 50300020, 50300030, jump attack
  50308000/50308050, parry 50002000 (guess), plus the base-layer actions. +0xf8/+0x100 mix both.
- Derived: the snapshot's animation id is ring +0x20; attacks need ring +0x34.

### Run 24 plan
- PlayerSnapshot gains `action_id` + `action_seq`: any +0x34 slot changing to a non-zero id is a
  new action. Puppet map: combo hit 1/2/3/4+ → 3000/3001/3005/3006, jump and jump attack → 3002,
  dodge 213300/213301 → 3004; block/parry → nothing (no counterpart found). Wire format changed.

### Run 24 (2026-10-28 ~01:58): actions over the network
- Owner: the puppet's attacks follow his combo rhythm; on dodge it swings in an arc (3004); on
  jump it does a plain strike (3002); no reaction to block (none mapped).
- Log: combo hits 50300000 → 50300010 → 50300020 → 50300030 arrived in order and each was played
  (3000, 3001, 3005, 3006; puppet anim = wanted id 300 ms later every time). Other action-ring
  ids seen: 600, 401, 432, 433, 443, 11 (moving/turning, guess), 201050 (jump variant).
- Bug: one action is written into 4-5 slots over ~100 ms, so action_seq rose 4-5 times per
  action and the puppet re-forced the same animation that often. Fix: count an id once unless
  300 ms passed. Jump/dodge mapping removed (no lookalike on c1020).
- Derived: an enemy puppet can only show the enemy's own moves. Exact moves need a puppet that
  uses the player's animation set (c0000), so the remote ids could be played as they are.

## Session 1 (cont.): spawning a character (offline, 2026-10-28)

### Observations (1.06 image)
- `CSDebugChrCreator` (vftable 0x2a952a8) is allocated in the WorldChrMan constructor
  (0xa46b80, `[this+0x3148]`) and stepped from WorldChrMan's update (0xa50a1c: vt[+0x10]).
  Steps: STEP_Idle 0xc0ff70 (sets +0x1b0 = 1), STEP_Load 0xc0ff80, STEP_LoadWait 0xc10010.
- `CSDebugChrPerfChecker::_STEP_CreateChr` (0xc15f00) drives it: +0x118 = NpcParam id (looked up
  in SoloParamRepository by 0x10c6750), +0x114 = second id (ThinkParam, guess), +0x1a4 = distance
  in front of the Hero, then `0xc0f2a0(creator, model)`.
- 0xc0f2a0: only if +0x1b0 != 0, store the model number at +0x1a0 and run 0xc10880: position =
  Hero position + forward × (+0x1a4) + up × (+0x1a8), angle from +0x1ac, name formatted from the
  model number into +0x120, then switch to the load step.
- Model ids (soulsmodding.com character list): Owl c5060, player c0000, c1020 = Samurai General
  (the run 8-24 puppet; c1010 is the Ashina soldier).

### Run 25 plan
- `SEKIRO_COOP_SPAWN=5060:auto`: 10 s after load, log the NpcParam rows 50600000-50609999, take
  the first, check the creator (vftable, idle byte, request-function bytes), set the fields and
  request model 5060 4 m in front of the Hero; then pin the character with that NpcParam id.
  Unknown: whether retail steps the creator (+0x1b0), and whether a spawned character reaches
  the mod's registry (fed by the ApplyEffect hook).

### Run 25 (2026-10-28 02:10): spawn works
- Log: NpcParam rows for c5060 = 50600000, 50600080, 50601000, 50601010, 50601080, 50609080,
  50609090, 50609091. Creator vftable OK, idle byte +0x1b0 = 1 (retail steps it), request
  bytes OK; model 5060 / NpcParam 50600000 / think 50600000 requested.
- Owner: the Owl appeared and killed him. The puppet code never pinned it: the spawned
  character did not appear in the mod's registry (fed only by the ApplyEffect hook).
- Offline: STEP_LoadWait ends with `creator+0x1b8 = 0xa492e0(WorldChrMan, &creator+0x100)`, the
  created ChrIns. Fix: watch +0x1b8 after the request, check char id = NpcParam id, pin it and
  switch its logic off at once.

### Run 26 (2026-10-28 02:20): spawned Owl as the puppet
- First launch hung in me3-launcher (no game window, no mod log); after closing it, the second
  launch worked. Cause unknown.
- Log: created character pinned 48 ms after the request (char id 50600000, handle 268795904),
  logic bit cleared, team 2; 7135 steps, 497 warps, 9 forced actions, remote_fresh=7136, stale 0.
- Owner: the Owl became the puppet, follows, answers attacks with combo-like moves, reacts only
  to attacks (only attacks are mapped). FPS fell to 25-30.
- Unknown: the FPS cause. 65 height resets in the run (guess: the 5 m X offset puts the larger Owl
  into geometry and physics fights the raw writes, as in runs 3-5).

### Run 27 plan
- Phased FPS test from the pin: A normal 0-40 s, B warps only 40-80 s, C no position writes
  80-120 s, D normal + fast catalog (187 ids, 0.4 s) for the Owl while the owner stands.

### Run 27 (2026-10-28 02:26): Owl catalog, FPS phases
- Phases ran (A 0-40 s, B 40-80 s, C 80-120 s, D after); height resets 0 this time. Owner did not
  report FPS per phase: FPS cause still unknown.
- Owl (c5060, NpcParam 50600000) accepted 3000-3049 (except 3022), 3060, 3061, 3063-3066, 3068,
  3069, 3090-3093, 3100; nothing in 3200-6900 (step 100) — those left it in 40000 (idle).
- Owner: block and parry appeared in the last ~20 s, i.e. among 3090-3093 and 3100 (the only
  accepted ids then); jump hard to pick, many jump attacks.
- Run 28: slow catalog (3 s each) of 3090, 3091, 3092, 3093, 3100, 3060, 3061, 3063-3066, 3068,
  3069 for the owner to name block, parry, dodge, jump.

### Run 28 (2026-10-28 02:33): Owl slow catalog
- Owner: FPS stable at 60 in all phases of run 27, so the run 26 drop is not the puppet code
  (guess: spawn spot in geometry; run 26 had 65 height resets, run 27 none).
- All 13 ids played (puppet anim = id 300 ms later). Owner: no. 4 (3093) = block; no. 8 (3063)
  = dodge, not sure. No parry or plain jump named.
- Run 29 map: block 50203000 and parry 50002000 → 3093, dodge 213300/213301 → 3063, jump none.

### Run 29 (2026-10-28 02:38): Owl actions
- Owner: attacks right; block hold plays its own block animation (3093); parry looks similar
  (also 3093); dodge (3063) plays a two-hit combo instead: wrong id.
- Run 30: each dodge plays the next candidate of 3061, 3064, 3060, 3065, 3066, 3068, 3069 (logged
  with its number) so the owner can name the right one.

### Run 30 (2026-10-28 02:43): Owl dodge
- Log: dodges 1-7 played 3061, 3064, 3060, 3065, 3066, 3068, 3069 in order. Owner: no. 2 (3064)
  is a backstep with a firecracker throw. Dodge → 3064.
- Also seen: a short block press gives 50203000 then 50002000 (parry) within ~0.5 s.
- Owl puppet action map now: combo hits 1-4 → 3000/3001/3005/3006, block and parry → 3093,
  dodge → 3064, jump → none (height follows). Owner-observed, no automated check.

### Run 31 plan: player-model puppet
- `SEKIRO_COOP_SPAWN=0:auto` (model c0000): spawn with the first NpcParam row in 0-9999; if there
  is none, log the low NpcParam ids and do not spawn. `SEKIRO_COOP_PUPPET_ANIM_PASSTHROUGH=1`:
  the puppet plays the remote action ids as they are (6-8 digit ids only).
- Unknown: whether c0000 can be created as an enemy/NPC character at all (crash risk; test copy).

### Run 31 (2026-10-28 02:48): c0000 spawn crashes the game
- NpcParam rows for model 0: 0, 1, 10, 11, 12, 100, 540, 541, 542. Spawned c0000 with NpcParam 0 /
  think 0; created and pinned after 49 ms; the game crashed ~0.3 s later.
- Windows Application Error: sekiro.exe AV at RVA 0x9cbf09, in 0x9cbe40, an equipment-slot
  accessor (asserts "slotIdx>=0", "slotIdx<EQUIP_SLOT_IDX_Num"); it read `[equip+slot*4+0x24]`
  through a bad equipment pointer. Derived: the c0000 character has no equipment data with
  NpcParam 0. Not in sekiro_coop.dll.
- Run 32: same with NpcParam 540 (guess: a human-NPC row that may carry equipment).

### Run 32 (2026-10-28 02:52): c0000 with NpcParam 540
- Same crash, same place (sekiro.exe RVA 0x9cbf09, equipment-slot accessor 0x9cbe40).
- Callers of 0x9cbe40 include 0x79dbe0 (passes `rcx+0x98`, slot 0-0x18) and 0x7ceaf0, 0x9cb210.
- Derived: a c0000 character made by the enemy creator lacks the player equipment data the
  c0000 model/render path reads. A Wolf-looking puppet needs the player-character path
  (PlayerIns + its game data, as DS3 remote players), not the debug creator. Not attempted.
- Launch script back to the Owl puppet (5060, action map).
