# Resident Evil 2 (1998): working notes

Running log, newest last. Observations, derived facts, guesses and unknowns are kept apart.

## Identity (observed 2026-10-27)

- Store: Steam re-release, AppID 4249110, build id 21744144 (April 2026, Capcom + GOG).
- Played through Mulder's "Steam Enhancement Pack" (mulderland.com), which runs
  `rebirth\bio2.exe`: the 2006 Sourcenext Biohazard 2 PC v1.1.0 exe with Classic REbirth.
- `rebirth\bio2.exe`: PE32 i386, 6 131 712 bytes, PE timestamp 1157628588 (2006-09-07),
  SHA-256 `00dd7aab0ae5d5f6afef1cb4a9071030c2e8d4edd00e5a24302a70c800c3c232`.
- `rebirth\bio2.exe.Backup` (original): SHA-256 `21b2a647cbeddac1ec389be847f22a434a0264a18020589de91cb2824dcb1bf5`.
  `fc /b` shows 4 bytes differ: 0x116 (0x0F -> 0x2F, LARGE_ADDRESS_AWARE) and the PE checksum.
  The code is the original 2006 code.
- `ddraw.dll` = Classic REbirth 1.0.9.1, `dsound.dll` = Ultimate ASI Loader 9.7.1.0 (loads `*.asi`),
  `re2_DDraw.dll` + `D3DImm.dll` = dgVoodoo 2.86.5. French patch "mod_fr_RE2 V1.0.2" shows up in
  Classic REbirth's MOD SELECTION dialog.
- The Steam re-release's own exes (`english\LeonU.exe`, `french\LeonF.exe`...) are the GOG build of
  the 1998 port, a different exe. Nothing below applies to them.

## Prior work (searched 2026-10-27)

- No co-op or multiplayer mod for RE2 1998 found (web search, this directory).
- Useful sources:
  - BioRand (github.com/IntelOrca/biorand), `biorandmod/biorand.cpp`: hooks for this exact exe
    (0x4C5E13 load game, 0x4E3FA0 event_exec, 0x4EF9AA NPC init call, SCD task array).
  - biohazard-utils `src/hitscan/Re2Binder.cs`: key word 0x988604, player id 0x98E9A6.
  - MeganGrass/Resident-Evil-2-Starter-Kit `source/bio2/data.h`: PS1 structs (tagGLOBAL,
    PLAYER_WORK, ENEMY_WORK...). **The PC exe keeps the PS1 layouts** (checked offsets below).
  - biohazard-utils `Script/Opcodes/SceEmSetOpcode.cs` + `OpcodeV2.cs`: SCD opcode layout.
  - yanmingsohu/re2re: a full ddisasm of a bio2.exe, but its addresses do not match this exe
    (0x53CAC8 is data there); likely another build. Not used.

## Memory map (derived, all checked in a running game)

| What | Address | How checked |
|---|---|---|
| `G` (tagGLOBAL) | 0x9864E0, pointer to it at 0x52D7E4 | per-frame loop reads `[0x52D7E4]+0x3C2C` = pEnemy |
| `G.Player` (PLAYER_WORK) | 0x989EF0 | Life +0x156 = 200, Max_life +0x162 = 200, pos moves when the bot walks |
| `G.pEnemy[34]` | 0x98A10C; [0] = Player, [1] = partner, [2+n] = em id n | em_set handler writes `[id*4 + 0x98A114]` |
| `G.Em_move_tbl[type]` | 0x98A154 (= G+0x3C74) | NPC types 0x40..0x5B all point at 0x4EF110 |
| ppEnemy_end, pFree_work, current entity | 0x98862C, 0x988624, 0x988628 | used by the loops |
| empty entity | 0x98E544 | unused pEnemy slots point here |
| SaveData | 0x98E79C; stage int16 0x98EB14, room 0x98EB16, player type 0x98E9A6 | BioRand offsets +0x378/+0x37A/+0x20A |
| Pad routine | 0x4D0F30, raw pad from `call 0x43BB00` at 0x4D0F53 | game key word 0x988604 (held), 0x988608 (pressed) |
| Key map (config 0) | 0x5338D8: game bit n <- raw PS1 bits | bit0 up 0x1000, bit2 down 0x4000, bit8 aim R1 0x0008, bit6/7/12 = 0x00A0 (circle/square) |
| SCD handlers | 0x53AE10[opcode](SCE_TASK*), task Data at +0x1C | em_set (0x44) = 0x4E77D0 |
| Per-frame entity loop | head 0x4C03CA, move call 0x4C0449 `call [G + id*4 + 0x3C74]` | |
| Room-init entity pass | 0x4DF180 (called once at 0x4DEE3D) | |
| Emd_load | 0x4B1660 (type, pEm, mem_top); types 0x50..0x5A pick Leon/Claire variants by flags | |

Entity offsets (PS1 = PC): Id +0x08, Pos_x/y/z +0x38/+0x3C/+0x40 (int32), Old xyz +0x44,
Cdir_y +0x76, nFloor +0x106, Parts_num +0x107, Status +0x10E, Move_no +0x14C, Life +0x156,
pKage_work +0x16C, pSin_parts +0x198. PC entity allocation is 0x248 bytes.
`PARTS_W` keeps its 0xAC size but its fields differ on PC (pointers into the renderer's heap).

## Experiments

1. Bot input: hooking `call 0x43BB00` at 0x4D0F53 and returning our raw pad drives Leon
   (UP 40 frames moved x from -24522 to -24932; LEFT turned Cdir_y 1200 -> -320). Circle confirms
   menus, Start/Cross skip the intro.
2. Spawning an NPC with the game's own em_set handler (fake SCE_TASK, Data -> our 22-byte opcode):
   - From the pad hook: crash at 0x4C0669 (shadow loop reads pKage_work before init). The NPC
     routine 0x4EF110 skips entities while bit 0x20000000 of 0x989ED4 is set, which it is outside
     the entity loop.
   - From a trampoline at the head of the per-frame entity loop (0x4C03CA): works. Claire (type
     0x51) appears next to Leon in stage 5 room 0x12, with shadow and idle animation, slot 33.
   - Overriding Pos/Cdir after her routine moves and turns her.
3. The em_set handler is asynchronous: if `[0x68A204]+0x13` (file load busy) is set it returns
   without advancing Data; the caller must re-run it next frame (the scheduler does this).

## Unknowns

- Which motion numbers give walk/run/aim for NPC Claire/Leon (em050/em051).
- Whether the NPC can be killed by enemies or block doors.
- How story flags are stored (SaveData flag arrays) — needed for host-authoritative progress.

## Session 2026-10-27 (continued): puppet, network, follow host

Observed (all in a running game, two copies on one PC):

4. `G` = 0x9864E0 (pointer at 0x52D7E4). Compiling `tagGLOBAL` from the PS1 header gives Player +0x3A10,
   pEnemy +0x3C2C, Sce_aot +0x2370, pSce_aot_on +0x2368, Stop_flg +0x39F4, SaveData +0x82BC: all match.
   pEnemy has 34 entries; the PC move table is indexed `[G + type*4 + 0x3C74]`, which overlaps
   pEnemy[18..33] for the unused types 0..15.
5. Per-frame entity loop head 0x4C03CA (`mov edx,[0x991FC0]`), move call 0x4C0449. 0x4DF180 is a
   room-init pass only (called once from 0x4DEE3D); hooking it does nothing per frame.
6. NPC state (Routine_0 = 4): Routine_1 = 4 walk-to-dest, 5 run-to-dest, 6 idle. `plc_dest` (0x40,
   handler 0x4E7330) writes dest to +0x1C4/+0x1C6 and +0x1D8/+0x1DA, arrival flag index to +0x1CB
   (set in 0x98EB6C via 0x503120), speed to +0x1D4. Kind 8 (back step) leaves the NPC idle.
   `plc_motion` (0x3F, 0x4E72D0): Routine = 4 | a<<8, Move_no = b, +0x1CC = c.
   `work_set` (0x2E, 0x4E5E90) stores the target entity at task+0x154.
7. Player Routine_1 while moving: 1 walk, 2 run, 3 back step, 4 turn (bot test).
8. Door AOT handler 0x4E9460, AOT handlers table 0x53B46C. Success path 0x4E95C4. Game sub-mode byte
   `[0x68A204]+9`, mode 4 = door (0x4C085E), which copies IN_DOOR_WORK into the player, sets cut
   (0x98EB18), previous room (0x98EB1A), room, and stage (calls 0x4DEF00 on a stage change).
9. Single instance: exe mutex name at 0x525138 (opened at 0x441EE2), Classic REbirth checks
   `bio2.658b45ea117473d4.game` and `re2cr.aiory0n5jnqf3r7a.game` (ddraw.dll 0x10065839).
10. Launching through `Popen` then attaching by pid is unreliable here (the pid printed for copy A was
    copy B's process once); resolve pids from the process path.
11. Room change: `ROOM` changes when the door animation starts; the new room's entity loop starts
    ~2-3 s later; the engine itself resets pEnemy[33] at room init.

QA (ran, 2026-10-27): `qa_suite.py steam-mock` 17/17, `qa_suite.py udp` 17/17. Real Steam (one
account): init, lobby created, title shows "en attente du joueur 2". Cross-stage follow by hand:
client from stage 0 room 0x0F to stage 5 room 0x12, in control afterwards.

Guesses (not verified):
- Zombies probably ignore the puppet (it is not in the partner slot pEnemy[1]).
- em slot 31 is probably unused by the stock rooms.

Unknown: behaviour in cutscene rooms, during Ada/Sherry sections, on death; two-account real Steam.
