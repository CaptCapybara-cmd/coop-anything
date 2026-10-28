# Handoff: continuing the Crimson Desert quest-sharing work (2026-10-28)

For an agent picking this up in a new session, possibly on another PC. Read [AGENTS.md](../../AGENTS.md) first,
then this file, then [NOTES.md](NOTES.md) (newest entries at the bottom) and [README.md](README.md).

## The goal

Play Crimson Desert in co-op with friends by reviving ByJunter's abandoned cdmp mod. First feature: **shared quests**.
The owner's policy:

- request quests are **accepted together**: when the host accepts one, the guest gets the same accept, plus the NPC's
  meeting scene so the guest "knows" the NPC;
- **each player cooks and hands in their own**: hand-ins and rewards are never shared;
- there is no per-player "variant" to match. What looked like one is the accept's game-clock time; see the
  2026-10-20 15:15 CORRECTION in NOTES.md.

## Where the code is

| Repo | Fork | Branch | What |
|---|---|---|---|
| cdmp (the mod) | [CaptCapybara-cmd/CrimsonDesertMultiplayer](https://github.com/CaptCapybara-cmd/CrimsonDesertMultiplayer) | `dev/quest-stage-tests` | stage sync, quest accept sharing (protocol 12), story log, datagram CRC, flight recorder, fuzz target, Tailscale UDP test mode |
| coop-anything (this hub) | [CaptCapybara-cmd/coop-anything](https://github.com/CaptCapybara-cmd/coop-anything) | `game/crimson-desert` | these notes, RE and Proton tools in `games/crimson-desert/tools/` |

The hub's tool fixes for Linux (`tools/test_all.py` and others) are on the fork's `fix/tool-tests-linux` branch. The
owner closed the PR (cranebeaker/coop-anything#139): **wait for something more substantial before any PR.**

Key cdmp files: `src/common/{quest_accept,stage_sync,story_log,story_digest,mission_rows}.h`, `src/common/protocol.*`,
`src/mod/net/{quest_share,stage_sync,stage_tracker,stage_replay,flight,coop}.cpp`, `tests/tests.cpp`,
`tests/fuzz/quest_target.cpp`, `kit/coop/TEST-QUESTS.md`, `scripts/package_coop.sh`.

## Where it stands

Done and verified on one PC (Linux, Proton, test copy of the game):

- the guest gets the host's accept and the meeting scene; a human then cooked and handed in, and the quest completed
  with the right reward;
- live accept sharing through cdmp's relay passed with protocol 12;
- under the hub fuzzer (`tools/fuzz`), 1000 sessions plus the hell and mobile presets ran clean, after 5 real bugs
  were found and fixed (NOTES.md 2026-10-22);
- flight recorder: works in the game and with `tools/flight/flight.py`.

**Not done: a test between two PCs.** That is the next step.

## The next step: the two-PC test over Tailscale

The owner has two PCs, one Linux and one Windows, on one Tailscale tailnet, and one Steam account. The mod's Steam
transport can't connect one account to itself, and one PC's Steam client runs in offline mode. So cdmp has a **test-only
UDP mode limited to Tailscale**:

- it is off unless `[dev] udp_tailnet=1` is set;
- the host binds only to its own Tailscale address and drops datagrams from outside 100.64.0.0/10;
- the guest refuses a `peer_host` outside that range;
- unit test: `test_tailnet_udp` in tests/tests.cpp, mutation-checked.

It has **not run in the game yet**, and Proton seeing the `tailscale0` address is not checked yet.

Settings in both PCs' `cdmp.ini`, on top of the kit's (`cdmp/dist`, built by `scripts/package_coop.sh`):
```ini
[net]
transport=udp
peer_port=37015
; guest only: the host's Tailscale IP
peer_host=
[dev]
udp_tailnet=1
```
Then follow `kit/coop/TEST-QUESTS.md`: connect, guest away, host accepts Renee's Request, guest arrives and gets it,
each hands in their own, and optionally a reconnect. Collect `cdmp.log` and `flight/*.cofr` from both PCs, then run
`python -I tools/flight/flight.py diverge host.cofr guest.cofr`.

If the Windows PC is the host, its firewall will ask about the game the first time. Making the Linux PC the host
avoids that.

## Building

- **Windows:** Visual Studio 2022 or its Build Tools, then `.\scripts\build.ps1` in cdmp (builds and runs the unit
  tests). See cdmp's README.
- **Linux:** a clang-cl cross build in podman: `games/crimson-desert/tools/xbuild/`. Unit tests run with Proton's
  wine inside Steam Linux Runtime 4 (`SteamLinuxRuntime_4/run -- "<Proton>/files/bin/wine" cdmp_tests.exe`). Without
  the runtime, it fails with "/lib/ld-linux.so.2: could not open".
- Fuzz target: `cmake -S tests/fuzz -B build-fuzz -DCOOPFUZZ_DIR=<coop-anything>/tools/fuzz`.

## Rules the owner set (on top of AGENTS.md)

- **No push and no PR without the owner saying so explicitly.** Before each push, confirm exactly what it will
  contain. No PR until there is "something more substantial" than tooling.
- PRs, when allowed: show the diff first, open as a draft with the template, one topic per PR, and leave the
  owner-review box for the owner.
- Commits: give the author on each commit as `-c user.name=CaptCapybara-cmd -c user.email=<that account's GitHub
  noreply address>`, the same as the existing commits on these branches. Never use `git config`. End messages with
  the Co-Authored-By line your harness gives you.
- Nothing private in anything published: no real names, emails, local paths with user names, IPs, tokens or game
  files.
- Never touch the owner's game install or saves. Use a test copy, and back up saves first.
- The owner starts Steam themselves; ask before anything that signs in to their accounts. Never enter credentials.
- Test with bots or unattended runs where you can, not with the owner. Ask the owner only for what needs a person.
- Pin downloads by SHA-256. Ask before installing software or writing outside the workspace.
- Write NOTES.md entries as you go, labelled observed, derived, guess or unknown. At the first contradiction, stop
  and say so.
- A GPU warning under Proton is a known Linux quirk: ignore it.

## Open items after the two-PC test

- more quest recordings: other requests, and a main-story step;
- maybe an on-screen note for the guest when a shared accept applies (needs a visual check by the owner).
