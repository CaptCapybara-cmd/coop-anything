# Resident Evil 2 (2019 remake) — agent notes

Game: Resident Evil 2 (2019), Steam app 883710, build 11636119, standard edition.
Engine: RE Engine. Identity recorded in the fork's `local/` (private; not for PR).

## Goal

Owner wants **online co-op with friends on separate PCs**. The directory only shows
local-only mods for this game, so this is a "build the missing online layer" project
(path 2, but standing on path-1 work).

## What exists (researched 2026-10-23)

### Local co-op mods (both REFramework Lua based)

- **Local Couch Co-op / "HUNK's Friend" (re2coop)** by Shopcreeper01
  (https://www.nexusmods.com/residentevil22019/mods/2938) — status `playable`, 2 players.
  Architecture confirmed from the installed copy on this PC:
  - `reframework/autorun/re2coop_main.lua` — 29,325 lines, v1.5.0.
  - `reframework/plugins/re2coop_pads.dll` — native input plugin.
  - Single process: spawns a second real pawn (prefab `ObjectRoot/Prefab/Character/Player/pl4000.pfb`),
    routes a second controller to it via hooks on InputSystem update methods.
  - Split-screen cameras (side-by-side / top-bottom), per-player HUD, shared world,
    separate inventories, revive/downed handling, AI-partner mode, coins a shared world so
    no serialization layer exists.
  - Hard requirements: REFramework **v1.5.9.1 RE2 build from GitHub** (REFramework installed
    on this PC: version v1.5.9.1, commit d4611aef1d63effd3a736bdf9404ea702428017d).
  - Game data versions: Non-RT build runs TDB 66, RT build TDB 70; mod has `st.NON_RT` gates
    (`sdk.get_tdb_version()`).
  - Source not published on GitHub (searched "re2coop", 2026-10-23: 0 repos). Tagged
    "AI Assisted" on Nexus.
- **RE2 Party Mode** by secretspook (https://www.nexusmods.com/residentevil22019/mods/3024)
  — v0.5.0, 18 Oct 2026, `alpha`, **local 2–4 player split-screen**, fork of the above
  (author: "I used his Local Couch Co-op as a basis"). Same REFramework Lua + `REPM_pads.dll`
  pattern. Requires REFramework v1.5.9.1. No online plans voiced by the author (checked
  description + posts, 2026-10-23). Not installed on this PC.

### Online co-op on the same engine (proves it is doable)

- **RE4 Remake Multiplayer Mod** by claybtv (https://www.nexusmods.com/residentevil42023/mods/6539)
  — 2-player **online** campaign co-op on RE Engine: enemies, story events, doors, switches
  synced over central relay servers + direct connect (UDP 39000). Closed source; ~70 open
  bugs; known issues: gimmick-area scans cause frame drops, remote enemies choppy
  (interpolation), pause breaks sessions. Proves the path; its pain list is our risk list.
- **DD2gether** for Dragon's Dogma 2 (https://github.com/But-Frog/dd2-together-release,
  binaries only) — up to 4 players, REFramework Lua (`reframework/autorun/coop/`) +
  LuaSocket transport + Steam lobbies with invite flow ("no port forwarding"). Syncs
  attacks/hitboxes/damage, enemies, NPC/quest state. Closest architectural blueprint:
  **REF Lua + small transport + game's own state machines**.
- **Steam Spacewar relay recipe** (this repo, playbook/networking-steam-spacewar.md):
  Steamworks flat API + `steam_appid.txt = 480`, ISteamMatchmaking lobbies +
  ISteamNetworkingMessages over Valve relay. No port forwarding, no extra purchases.
  Tested recipe from other games in this repo (Jericho 2026-10-21).

### Mod downloads need Nexus auth

Nexus files (Party Mode zip, anything new) require the owner's Nexus account; the agent
cannot fetch mod zips anonymously. `re2coop` mod files are readable locally — treat as
installed, don't modify them (never touch the owner's install).

## Online architecture (proposed, not yet built)

Layered on the proven local architecture:

1. **Game-side (Lua, REFramework):** reuse re2coop/Party-Mode style pawn spawn, per-player
   camera/room loading, inventory surgery. In online mode the local injector feeds P2..Pn
   pawns not from pads but from **network input buffers**, and streams our pawn's pose +
   actions out.
2. **Net layer (C++ REFramework plugin — reframework API in praydog/REFramework, docs
   refdocs.praydog.com):** transport = Steam Networking Sockets (Spacewar relay recipe) so
   friends host/join via Steam invites without port forwarding.
3. **Authority:** host = the game's own P1 process, like the AI-partner mode but driven by
   remote players' inputs. Enemies/doors/story stay in the host process — one authoritative
   world, avoiding the two-locked-worlds story desync class first. Interpolation for remote
   pawns (see claybtv choppy-enemy lesson: budget for interpolation early).
4. **QA:** two game copies + loopback first (per playbook multi-instance tests), then chaos
   proxy (`tools/chaos`), then the rest of the sync tool suite.

## Observations (this PC, verified)

- RE2 install lives at `E:\SteamLibrary\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2`
  (installdir per appmanifest); `E:\SteamLibrary\common` is missing/hidden — the folder
  oddity doesn't affect the game.
- re2coop has been used on this install (data logs exist: `re2coop_p2_log.txt`,
  `re2coop_hunk_config.json`, pads log); REFramework log shows a run at 2026-10-23 01:40.
- Non-RT re2.exe hashes: 6CAAA815BF9E95F8A841BC81FDF29FEC55E836C87BAB8A76D55C69BEF9ADA941
  (recorded 2026-10-23).

## Derived / guesses (not verified - labelled, per AGENTS.md rule 4)

- Guess: the re2coop "AI partner" mode (input-driven local pawn) is the correct injection
  point for remote players — that is inferred from the inline comments about input
  routing, not proven by tests yet.
- Unknown: how re2coop keeps the second pawn's room loaded ("keep_p2_area") works
  internally; Party Mode advertises per-player rooms, RE2 may allow several active rooms.
- Unknown: re2coop license — source is on Nexus as a packaged lua; distribution/derivative
  terms of the mod need checking before we build on its code.

## Workspace build environment (phase 2, done 2026-10-23)

Verified with `python tools/test_all.py`: **all 15 sync tools now print all green**.
On the first pass the following three failures appeared on this machine; all are
tool-vs-newer-environment issues, which we are going to submit via PR:

- **flight** hard-coded `["-G", "Visual Studio 17 2022", "-A", "x64"]` (`run_tests.py`).
  When VS 2022 is absent, it falls back to no generator → CMake chooses NMake →
  `nmake --parallel 1` → U1065. On this machine there is only BuildTools 18 (VS 2026).
  Fix: probe `Visual Studio 18 2026` → `Visual Studio 17 2022` → `Ninja`, and detect
  the generator via the `CMAKE_GENERATOR` line in the build cache (the compiler path
  in the cache also contains "Visual Studio" and confuses the old check).
- **flight (CommandOracle)** parsed via `shlex.split(posix=False)` on Windows, which
  does not remove MSVC quoting if the path contains a space → `python -I "<path>"`.
  Fix: our own `_msvcrt_split` parser (MS CRT rules) when `os.name == 'nt'`.
- **mutants (coop_mut.h)**: the test asserted that the literal like `add_wrong_operator`
  is in the test binary in contiguous bytes. MSVC 2026 folds the literales of
  `is("name")` comparisons into the instruction as 64-bit immediates
  (`add_wron`+`g_operat`+`or` interleaved), leaving nothing to find. Fix: `COOP_MUT_NOINLINE`
  (`__declspec(noinline)` / `__attribute__((noinline))`) in `coop_mut::is()` so the constant
  survives contiguous in .rdata (and additionally smaller binaries).

Environment steps (PowerShell) that are required for running it, recorded in `local/owner-state.md`:

1. `winget install GitHub.cli Kitware.CMake Ninja-build.Ninja` (already installed here).
2. Prefix PATH with `C:\Program Files\CMake\bin` + winget `Links` of Ninja (session-specific PATH).
3. Wrap the whole session with
   `cmd /c "chcp 65001 > NUL && <VsDevCmd> -arch=x64 && python tools\test_all.py"`.
   Important: it must be the banner codepage **before** Python starts (UTF-8).
   The repo calls the tools with `-I`; `-I` discards the `PYTHONIOENCODING` environment variable
   and then `print()` under cp1252 crashes on Unicode characters in test notes. Therefore we
   launched here without `-I` (only for the encoding issue, not for isolation reasons).
4. A deleted build directory (`${env:TEMP}\cofr-flight-build`, `$env:TEMP\coop-mutants-build-*`)
   forces the new generator.

## Sync protocol prototype (phase 5 started, 2026-10-23)

`tools/fuzz/re2_target.cpp` (in the fork; temporary home until the mod has its own repo) wraps
the protocol MVP we plan to transcribe into the RE2 mod, as a coopfuzz target (test runs without
the game, verified with the framework's own oracles):

- Model: 2-4 machines, machine 0 = host (P1/campaign). Each machine simulates its own pawn and
  sends it to the host with a `seq` + ack/retransmit; the host replies with a versioned snapshot
  (pawns + zombies + doors + item holders), applied only if `version > myVersion`
  (per-thing `idem`), retransmitted until the client's ack, which allows it to stop.
- Interactions as requests to the host with SNAPSHOT replication: doors (interact), items
  (pick up → machine buys it), spawning a zombie (one-time per site, double request = clean
  rejection), damage to a zombie (host). `join/leave/save/load/stale` outside of this MVP
  (documented): they are left for the lib prototype with ownership/worldEpoch
  (the complete example target in example_target.cpp already covers exactly that, it is the model
  to expand to).

Test runs (real tools, executed here):

- 200 clean sessions x 2/3/4 players: `RESULT: ALL OK` (600 in total).
- Injected twin `no_snapshot_dedupe` (applies old snapshot): caught as `double_apply`
  (3/3 sessions, shrunk scenario to 1 op) → the fuzzer does fail against this target, it is not a greenwash.
- `--minutes 2 --players 4` caught 2 real bugs (shrunk to 1 op each) and are already fixed:
  1. snapshot reading zombies/doors read flags 1 byte with a 4-byte read → desynchronization
     (pos bled with adjacent bytes, `(-1,-1)` in the shrunk report).
  2. On moving the HOST's own pawn, `version` was not incremented → the new position of P1
     never left the host.
  After the fix: `--minutes 3 --players 4`: **17,928 clean sessions, 0 failed** (`ALL OK`).

Next (in order): worldEpoch/save-load + join/leave + ownership transfer in the target
(using example_target.cpp as a model), then transcription to REFramework Lua + the C++ transport
plugin (Steam relay recipe), and there the first 2 real copies with `tools/gamepool` + `tools/chaos`.

## Unknowns to next steps

- Two-copy loopback pawn sync test (needs a test copy of the game on another drive).
- Party Mode needs installing from Nexus (needs owner's account login).
- Fork + branch + claim issue needs `gh` (not installed) and the owner's GitHub account.
