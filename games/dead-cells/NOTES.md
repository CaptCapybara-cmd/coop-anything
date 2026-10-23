# Dead Cells — co-op notes

Goal (owner): local couch co-op, one PC, two gamepads, two real heroes; per-player loot parity
(flasks, mutations, starting weapons, every "pick one" choice repeated for P2). Prefer Steam Workshop mod.

## Observations (sourced)
- Not in coop-anything DIRECTORY.md or games/ (checked 2026-10-23).
- Engine: Haxe/HashLink, game v35 (log line Boot.logClientInfos).
- Official mod tools (ModTools/ModsDoc.pdf): data paks (CDB, rooms) + .hx scripts for level structure/mob rosters only. Cannot add a second player.
- DCCM (github.com/dead-cells-core-modding/core, MIT): C# mods on .NET 10 hooking HashLink functions (Hook_<Class>.<method> += ...), events (IOnHeroUpdate), Hxbit serialization, workshop upload command in mdk/DCCMTool. Latest dev commit 2026-10-20, release 35.14.1.
- DCCM Workshop item 3633185550 and DeadCellsMultiplayerMod 3657857836: both show "removed ... violates Steam Community & Content Guidelines" (checked 2026-10-23). Reason unknown.
- DeadCellsMultiplayerMod (github.com/vaiserYT/DeadCellsMultiplayerMod, MIT, 584 commits, last 2026-10-19): ONLINE host/client (TCP 1234 or Steam P2P). Remote player is a "ghost" (GhostKing : KingSkin entity), not a real Hero. No local/split-screen.
- Hero has `controller` field of type ControllerAccess (deepnight Controller lib) — candidate for binding P2 to pad #2.

## Derived
- Path 2 (build from zero) for LOCAL co-op, reusing DCCM + lessons from the online mod (cine/death/Homunculus null-check pitfalls).
- Use the "second player inside one copy" route (playbook/split-screen.md, Dave the Diver): spawn real 2nd Hero, "P2's turn" swap of game.hero for game systems, shared camera on midpoint.

## Unknowns
- Whether a second dc.en.Hero can be constructed and updated in one Level without breaking singletons (Game.hero, HUD, inventory UI).
- Whether Steam will keep a DCCM code mod on the Workshop.

## 2026-10-23 — reverse engineering (game v35, findings from bytecode via crashlink)
- Input: tool.Controller reads ONE GamePad when `gc` is set, else merges activeGamepads (padIsDown f@2444). Physical pads go to GamePad objects in creation order (libs.heaps.GamePad.onDevice f@2472), so the boot Controller owns every pad. => P2 = new Controller(s2d), mode=ENABLE_PAD, gc = boot controller's GamePad for pad #2; boot controller gc = pad #1.
- Hero.init (f@1935) always binds `controller = Boot.ME.controller.createAccess("hero")` -> swap after init for P2.
- Hero ctor (f@4902) clones game.data.inventory; each hero has its own inventory, HeroMainSkillsManager and HealBlackBoard (flask charges `blackboard.healings`) => flasks/weapons/mutations are per-hero by construction.
- Level change: Game.activateSubLevel (f@663) and loadMainLevel (f@656) call hero.clean() then Game.initHero (f@665). Hook clean (take P2 out) + initHero (put P2 in).
- Camera: Viewport.update (f@2875) aims at tracked.spr.x/y. Shared camera = park P1 sprite at midpoint during the call.
- Mutations: PerkMaster.onActivate(by) opens `new ui.PerkSelect(by, npc)` -> each player activates Guillain themselves. TierSelect (scroll stat choice) opened from Hero.applyItemPickEffect(this).
- ReplaceItem/ReplaceSingleItem/ReplaceItemWithDual have no hero argument (use game.hero) -> "P2's turn" swap of game.hero during ctor/validate/update.
- DCCM's own Hook_Hero.init sets Game.HeroInstance = self for ANY hero -> after P2 spawns, DCCM HeroInstance points at P2. Mod uses Game.ME.hero instead.
- Build: Ubuntu dotnet-sdk-10.0 + MDK nupkg from win-x64.zip (offline, nuget.org not needed).

## Status
- v0.1.0 built and copied to <game dir>\coremod\mods\DeadCellsLocalCoop. QA level: untested (not yet run in game).

## Owner design backlog (decide after first working test — do NOT implement yet)
- Camera v2 (owner idea, 2026-10-23): camera centred on the players' midpoint and *drags* left/right/up/down
  according to each character's offset from the centre (soft dead-zone / look-ahead), instead of only moving
  when both players push the same edge. Reason: with the v0.1.1 hard screen-bound, both players must crowd
  the screen edge to move forward, which makes them run blind into enemies/traps and is risky on high-altitude
  biomes with moving platforms and pits (e.g. cloud/tower maps). Revisit once the base mod works in-game.
- Current state: v0.1.1 = midpoint camera + hard horizontal screen-bound + vertical pull-back after 1.5 s.
