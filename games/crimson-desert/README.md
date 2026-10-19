# Crimson Desert (2026)

| | |
|---|---|
| Engine | BlackSpace Engine (Pearl Abyss, in-house) |
| Versions tested | Steam build 25477059, `CrimsonDesert.exe` 1.0.0.2976 (game version 2.03.02), PE timestamp `0x6AB28F00`, SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`; Linux, Proton Experimental |
| Projects | [cdmp (ByJunter)](https://github.com/ByJunter/CrimsonDesertMultiplayer), [CrimsonDesertCoop (blizz3010)](https://github.com/blizz3010/CrimsonDesertCoop), unreleased mod by No-Context2224; see the three Crimson Desert rows in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | none |
| Last updated | 2026-10-19 |

## Where things stand

Two players can see each other and fight the host's nearby enemies with ByJunter's cdmp alpha, but the story is not
shared: each player stays in their own save. This folder documents a revival of cdmp aimed at shared quest progress
(claim: [cranebeaker/coop-anything#51](https://github.com/cranebeaker/coop-anything/issues/51)). So far: cdmp builds
on Linux, runs under Proton, and the quest/stage messages are mapped; nothing quest-related is synced yet.

## Existing work

- **cdmp** by ByJunter, [GitHub](https://github.com/ByJunter/CrimsonDesertMultiplayer) and
  [Nexus](https://www.nexusmods.com/crimsondesert/mods/3677). C++20 ASI mod, GPL-3.0, v0.2.0 (2026-10-11), author no
  longer maintains it. Only build 1.0.0.2976. Syncs movement, gear, horse, rope, time of day, map marker, and hostile
  enemies near the host. Steam friends only. Research notes (Spanish, English index) in its `docs/research/`.
  Revival fork: [CaptCapybara-cmd/CrimsonDesertMultiplayer](https://github.com/CaptCapybara-cmd/CrimsonDesertMultiplayer).
- **CrimsonDesertCoop** by blizz3010, [GitHub](https://github.com/blizz3010/CrimsonDesertCoop), 0.2.3, last push
  2026-07-08, no license. Player 2 takes over a companion. Its README says quest and cutscene sync are not applied.
- **No-Context2224**: unreleased, known from press only
  ([Wccftech](https://wccftech.com/crimson-desert-2-player-co-op-mod-but-more-advanced-soon/)).
- Pearl Abyss has talked about an official multiplayer mode; no date found.

## How the game works (for co-op)

- The single-player game keeps the MMO split of its ancestry: the client talks to an in-process server through
  1,114 "Troc" messages (`u16 opcode | u8 slot | u16 len | payload`). cdmp hooks both funnels
  (`ClientNet::PostPacket` exe+0x35C670, `ClientNet::OnRecvPacket` exe+0x35CA00) and can inject packets both ways.
  Message table with handler RVAs: cdmp `docs/research/data/troc_messages_2976.csv`.
- Each C->S message has a Process function (vfunc 2) that deserializes fields one by one (`Deserializer` vfunc +8,
  byte count in `r8d`) and then calls the game logic. Bound functions by the `.pdata` table, not by int3 padding
  (handlers are packed without padding; see the correction in [NOTES.md](NOTES.md)).
- **Quest progress** is stage-chart driven: Quest -> Mission -> Stage, with sequencers for scripted scenes.
  - `UpdateQuestListAck` (2808), `UpdateMissionListAck` (3009), `UpdateStageListAck` (2724): header
    `u8 scope | u32 player key | u8 a | u8 b` (full list a=1 b=0, delta a=0 b=1), then fixed records:
    quest 34 bytes `{u32 key, u32, u64, u64, u64, u8 state, u8 flag}`, mission 55 bytes, stage rows every 56 bytes.
    Quest and mission keys are static data keys (0x0F4xxx range).
  - Stages are referenced by **runtime instance ids** (small numbers like 0x158), different per session:
    `StageBranchStateReq` (2436) `u64 instance | u32 branch | u16`; `StageBranchStateAck` (2981)
    `hdr | u64 instance | u32 state | u32 branch | u32 chart hash`; `StageOnBranchEventAck` (2524).
- **Retail stubs** (parse, then do nothing): `CompleteQuestByCheatReq`, `ResetQuestByCheatReq`,
  `StageChartForceBranchDevReq`, `ForcePlayStageDevReq`, `ForcePlayStageSequencerDevReq`,
  `ChangeGameLevelStateByCheatReq`. The quest cheats are not a shortcut. The game's own `StageBranchStateReq`,
  `StageBranchStateDelayEndReq`, `UpdateStageChartDelayedEventReq`, `QuestDialogEndReq` and others are live.
- Saves: `%LOCALAPPDATA%\Pearl Abyss\CD\save\<steam account id>\slot*`; display settings in
  `user_engine_option_save.xml` next to them (`_displayType` Windowed/Borderless/FullScreen, `_viewportSize`).
- Denuvo anti-tamper; no online anti-cheat seen in `bin64`.

## What we changed

- **clang-cl build fix** in cdmp (`src/common/enemy_zone.h`): clang rejects a default argument that needs a nested
  struct's member initializers inside the enclosing class; split into two constructors. Branch
  [`fix/clang-cl-build`](https://github.com/CaptCapybara-cmd/CrimsonDesertMultiplayer/tree/fix/clang-cl-build).
  Test: builds with clang-cl 19 + xwin; `cdmp_tests.exe` prints "All tests passed" under Proton's Wine.
- **Stage test driver** (local, not pushed yet): `[dev] stage_force_test=1` injects a branch request on the first
  waiting stage after a negative control, and logs `StageTest: RESULT: PASS|FAIL|SKIP`. Run with
  `StageChartForceBranchDevReq`: negative control PASS, real test FAIL, explained by the handler being a stub.
  The `StageBranchStateReq` variant is built but not run.

## QA

- No automated two-player test exists yet. Checks that ran are listed with dates in [NOTES.md](NOTES.md).
- One-PC echo smoke test under Proton (cdmp's `cdmp_peer --mode relay`): hooks installed, overlay drew 15k+ frames,
  echo copy spawned, 500+ events replayed with 0 rejected. A person moved the character; nothing asserted automatically.

## How to play

Use ByJunter's release from Nexus (both players, same game build 2.03.02, Steam friends). On Linux/Proton add
`WINEDLLOVERRIDES="winmm=n,b" %command%` to the Steam launch options so the `winmm.dll` loader is used (worked in the
smoke test above). Back up saves; do not save while the other player is in your world.

## Known issues

- Story, NPCs, quest progress and loot are per player; only hostile enemies near the host are shared.
- Works on one game build only; any patch disables it until offsets are updated.
- One crash on Proton while opening the menu (vkd3d-proton `VK_ERROR_DEVICE_LOST`) right after the stage test
  injected a request; not reproduced in 5 menu openings without it. Treated as unknown.

## Next steps

1. Run the stage test with `StageBranchStateReq` (2436) on a test copy: does an injected request branch a waiting
   stage like the game's own?
2. Map static stage keys to runtime instance ids on each PC (from `StageBranchStateAck`, `PrepareSequencerAck`,
   `UpdateStageListAck`), so the host's progress can be replayed on the guest's own instances.
3. Relay host quest/mission deltas to the guest, then decide per quest whether to replay branches or only show
   the host's objective.
4. Build an in-game bot that loads the save and drives the test without a person.

## Tools in this folder

- `tools/xbuild/`: podman image with clang-cl 19, lld-link and the Windows SDK/CRT from xwin 0.6.5 (download the
  release tarball into `dl/` and check its SHA-256 first; building the image accepts Microsoft's SDK license), plus a
  CMake toolchain file and `build.sh <abs source dir>` that prints BUILD_OK/BUILD_FAILED.
- `tools/re/`: capstone + pefile scripts, run in a hash-pinned image against your own `bin64` mounted read-only:
  `dis.py` (disassemble at an RVA), `classify2.py` (stub or live handler, exact .pdata bounds), `disp2.py`
  (operand offset search), `callers.py` (direct callers), `cap.py` (extract payloads from a cdmp `.bin` capture),
  `questlist.py` / `rows.py` (decode list Acks), `sections.py`.
- `tools/proton/run_game.sh`: launch a hardlinked test copy with its own prefix through Proton.
