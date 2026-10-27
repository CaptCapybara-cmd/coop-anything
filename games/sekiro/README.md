# Sekiro: Shadows Die Twice (2019)

| | |
|---|---|
| Engine | FromSoftware proprietary (x64, Windows), shares lineage with Dark Souls III |
| Versions tested | Steam 1.06 (`sekiro.exe` ProductVersion 1.6.0.0, PE timestamp `0x5fa3e066`, SHA-256 `637aca527538c0ec6e1f136c8ed66046e95dfbdbb1f51926e134d9916398b856`) |
| Projects | [Sekiro Online](https://www.nexusmods.com/sekiro/mods/577) (Souls-style summons, abandoned 2021); [sekiro-coop](https://github.com/mstampfli/sekiro-coop) (seamless 2-player prototype, Rust) |
| QA level | none (one real game client plus the project's peer simulator; no two-client test yet) |
| Last updated | 2026-10-27 |

## Where things stand

Nobody can play the Sekiro story seamlessly with a friend yet. Sekiro Online gives Souls-style
co-op through summon signs and Steam, but it has been unmaintained since 2021 and we did not test
it. sekiro-coop is the only seamless attempt: its networking and world-event sync work, but the
second player is not shown in the world, and the per-launch probes below show why that is hard.

## Existing work

- **Sekiro Online** (Nexus #577): v2.05, 2021-06-13. Summon signs, invasions (cannot be turned
  off), 1 host plus up to 5 phantoms over Steam P2P, level scaling. Must reach the Dilapidated
  Temple first. Page says it is no longer developed and mentions a new version "as of 09/03/2025";
  nothing newer was found. Closed source.
- **sekiro-coop** ([mstampfli/sekiro-coop](https://github.com/mstampfli/sekiro-coop), 6 commits,
  last 2026-06-15, AGPL-3.0-or-later): Rust DLL loaded by [me3](https://github.com/garyttierney/me3),
  MinHook detours, raw UDP direct IP (Steam P2P is an empty stub), host-authoritative enemy HP.
  Its `docs/HANDOFF.md` plans to drive a puppet NPC by patching `common.emevd`; not started.
- No Seamless Co-op (LukeYui) release for Sekiro was found.

## How the game works (for co-op)

- The Steam exe on disk is packed (`.bind` section, encrypted `.text`): AOB patterns only match the
  decrypted image in memory, so static scans of the file find nothing.
- me3 0.11.0 starts the game with Arxan neutralised (`--disable-arxan true`), blocks matchmaking
  (`--online false`) and can redirect the save to a separate file (`--savefile`).
- Live character state is reached from a `ChrIns` (character instance):
  - handle `+0x08`, character id `+0x68`, team byte `+0x74`;
  - HP/posture `+0x1ff8 → +0x18 → +0x130/+0x134/+0x148/+0x14C`;
  - animation id `+0x1ff8 → +0x10 → +0x20`;
  - physics-module position `+0x1ff8 → +0x68 → +0x80`.
- The game's global debug flag "all no update AI" is at `debug_flags + 13` (1.06 RVA of
  `debug_flags`: `0x3d7a369`).

## What we changed

Nothing is released. All changes are local experiments on top of sekiro-coop `1ef5fa8`, described in
[NOTES.md](NOTES.md):

- Build fixes: MinHook rebuilt from the vendored (byte-identical to upstream `d94c64d`) source
  instead of the prebuilt `.lib`.
- A puppet probe (`SEKIRO_COOP_PUPPET_PROBE=1`) that pins one enemy and holds its physics position
  next to the player. Findings:
  1. Writing the physics position **from the game thread** (inside the SetFlag detour) moves the
     character on screen. sekiro-coop's earlier conclusion that the render path ignores these
     writes came from writing on a separate 60 Hz thread.
  2. The team byte (6 → 2) does not stop the puppet attacking; freezing all AI does.
  3. The game keeps pulling the character back to a stored home position: during a 1 s write pause
     it snapped back to its spawn point, and ~25-80% of writes were reverted. Also writing the
     `ChrIns+0x1060` copy did not help; `+0x1050` is not maintained by the game. Position poking
     fights the engine; a puppet needs the native move/warp, animation and per-character AI calls.
  4. Writing the player's height into slopes made physics push the puppet out every frame
     (FPS fell to ~15); keeping the game's own height let it fall through the world.

## QA

- sekiro-coop's own data plane, reproduced on one PC against its `peer-simulator` over loopback:
  session established, remote snapshots and flag/effect events received, host enemy-state echo
  resolved for every enemy (`matched=2 unmatched_count=0`). No desync kill, no crash.
- No two-client test and no bot test yet, so the QA level stays `none`.

## How to play

Not playable yet.

## Known issues

- The second player is invisible (sekiro-coop).
- Connection is direct IP only; no NAT traversal.

## Next steps

1. Analyse a decrypted in-memory image of 1.06 offline to find the native warp/move, animation
   playback and per-character AI-disable functions, and how the DS3-lineage engine spawns remote
   player characters (what Sekiro Online appears to use).
2. Build the puppet on those calls instead of memory writes.
3. Replace raw UDP with the Steam Spacewar relay (playbook/networking-steam-spacewar.md).
4. Two-client bot tests (playbook/automated-qa.md).
