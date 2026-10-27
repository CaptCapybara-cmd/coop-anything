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
