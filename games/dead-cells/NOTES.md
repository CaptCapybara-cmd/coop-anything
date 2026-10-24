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

## Playtest 1 (owner, 2026-10-23, v0.1.1) — QA level: player reports
Worked: P2 spawned on pad 2 (P1 pad#2, P2 pad#3), movement, midpoint camera (owner happy with feel).
Bugs -> fixed in v0.2.0 (untested):
- P1 frozen after P2's ReplaceItem menu: menus open an EXCLUSIVE ControllerAccess on the boot controller
  (ControllerAccess ctor f@2477 sets parent.exclusiveId); v0.1 overwrote ui.controller without dispose ->
  boot controller stayed exclusive forever. Fix: dispose original access, give menu exclusive P2 access.
- Players pushed each other: hook Entity.canHaveRepellingWith -> false for the hero pair.
- Loot copies of swapped/dropped gear (log showed ThunderShield #720 "Equipped" copied): items ever in an
  inventory are "owned" and drops created during pick/P2 menus are never copied. DeathMoney no longer copied
  (consumables whitelisted: *Tier scrolls, QuarterUp, Food/Meat).
- Starting pipes (en.inter.Random*Spawner, meta unlocks RandomBow/RandomShield/RandomCC/Recycling): owner wants
  each player to pick their own build. Now each pipe rolls a separate item for P2 (spawnItem called again),
  reserved per player, side by side.
- P2 HUD: second ui.HUD instance built/updated under a P2 scope (game.hero, game.hud, HUD.ME -> P2's),
  leftFlow mirrored to bottom-right; P1 minimap hidden, money/cells/kills moved to top-right.
Cosmetic, owner OK to leave: P2 has no head.
- Deploy lesson: re-committing the same staged path to the PC silently wrote the PREVIOUS build (owner ran
  v0.1.1 again). Always stage each build under a new versioned folder and verify by reading it back (SHA-256)
  and checking the "[LocalCoop] vX loaded" log line. v0.2.0 dll sha256 b155a45c...de6c6 verified on PC.

## Playtest 2 (v0.2.0) -> v0.2.1
- P1 freeze on P2 menus: FIXED (confirmed by owner).
- Crash: Null access .isFullscreen in Hero.updateInteraction (P2 preUpdate) and 5x ItemDrop.minimapTracking null:
  v0.2.0 swapped game.hud/HUD.ME to P2's HUD during P2's update; P2's HUD has no minimap. v0.2.1 never swaps
  game.hud; hero->HUD reports (setHealings, updateAmmo, tiers, buffs...) made while P2 acts are routed to P2's HUD.
- Starting pipes are the ARMORY: en.inter.ArmoryButton.onActivate (f@23374) drops ItemDrops within 3 cells of the
  pulled switch (inArmory=false, gravity) and retracts every other ItemDrop in the room (tween up), other switches
  deactivate. Random*Spawner.spawnItem hook never fired for them. v0.2.1: co-op armory - each player pulls their own
  switch; first pull releases that tube only (reserved for that player), partner pulls another tube or gets a copy
  of the same one; switches turn off only after both chose. Items still inArmory can't be taken.
- P1 counters: HUD.rightFlow spans the whole screen and bottom-aligns; set verticalAlign=Top to move them up.
- Map = action 9 = pad SELECT (Share/Create/View), keyboard Tab (Options.configController f@5790).

## Playtest 3 (v0.2.2) -> v0.3.0 (verified on PC, sha256 dd9915df...)
Root causes found in bytecode:
- Life bar: Hero.updateLifeBar (f@2025) writes to ui.HUD.ME.heroLifeBar -> P2's life went to P1's bar (P1 bar showed
  P2 damage, glitched white, low-life sound looped; P2's bar never updated -> stuck orange "recover" part).
  Fix: HUD.ME = owner's HUD during updateLifeBar and during P2 scopes; P2's HUD shares P1's minimap + counters.
- Crash "Null access .alpha" in Hero.postUpdate (spr null): P2 updated after being cleaned when P1's death tore the
  level down. Guard: skip P2 pre/fixed/postUpdate when spr/_level null.
- P1 death ended the run with P2 alive: death reached Hero.onDie without Hero.kill. Down/revive now also on onDie.
- Teleport statue: Teleport.onActivate -> HUD.ME.minimap.chooseTarget(..., startTeleport(by)); minimap reads its own
  `controller` -> P1 chose. Swap to P2 access when P2 activated. Partner now follows any >8-cell jump (teleports).
- Scrolls: ids BrutalityUp/SurvivalUp/TacticUp/QuarterUp (+ choice scroll -> TierSelect) go through
  Hero.applyItemPickEffect, not pickItem. Partner gets the same scroll after the picker's TierSelect closes.
- Venus flytrap = en.ltrap.CarnivorousPlant; fixedUpdate only checks pr.Game.ME.hero. Run with game.hero = nearest.
- Dupe exploit: P1 dropped SeismicStomp via P1's ReplaceItem (outside pick depth, new InventItem instance) and P2's
  pick copied it. Now: owned by permanentId, drops during any replace menu / dropAndUpdateItem never copied.

## Playtest 4 (v0.3.0) -> v0.3.1
- Both bugs were one regression of mine: v0.3.0 pointed P2's HUD at P1's counters; HUD.onResize does
  aboveMapFlow.getProperties(bossCellCount).paddingRight -> null (counter not in P2 HUD's flow) -> P2 HUD creation
  failed -> error path destroyed P2's HUD, and HUD.onDispose (f@5346) sets this.minimap.destroyed = true -> P1's shared
  minimap died -> LevelTransition ctor (ZDoor, Giant's broken door) crashed on null getTile.
- Fix: share only the minimap; always null hud2.minimap before destroying hud2; restore HUD.ME if dispose cleared it.
- Process lesson: build.sh hid a failed build and an old dll was staged once; build.sh now fails on build errors.

## Playtest 5 (v0.3.1) -> v0.3.2
- Crash leaving the PrisonStart sub-area: P2's HUD shared P1's MiniMap; on area change Game.activateSubLevel ->
  HUD.initHero rebuilds the map, P2 HUD's onResize also ran minimap.onResize (and rebound its get_pixelScale) ->
  MiniMap.initUI reflow null .x -> P1 HUD crash. Lesson: never share HUD sub-objects between two HUD instances.
- v0.3.2: P2 HUD shares nothing; no global HUD.ME swap (only inside Hero.updateLifeBar); any exception inside
  P2's HUD code is caught and logged once instead of crashing (P2 HUD is cosmetic).

## Playtest 6 (v0.3.2, first session with no crash) -> v0.4.0 (verified on PC, sha256 f04cc7aa...)
- Cooldown icons flickering + "ready" sound loop on P1's HUD: BeheadedActiveSkillsManager.fixedUpdate (f@23217) gets
  icons via HeroActiveSkillsManager.hudGetSkillPower -> ui.HUD.ME.skillPowers; P2's manager wrote P2's timers into P1's
  icons every frame. Scanned bytecode (gamecode/hudscan.py) for every tool/hero, en/Hero, mainSkills, weapon function
  reading the HUD global (g@426): 39 functions. All per-hero ones now run with HUD.ME = owner's HUD (HudRouting.cs);
  shared ones (money, cells, minimap, death flow) stay on P1's HUD.
- Scroll given twice: P1 property was game.hero, which is P2 during P2's turn -> Partner(P2) returned P2. P1 is now a
  stored reference; and a TierSelect's resulting applyItemPickEffect (e.g. SurvivalUp from AnyUp) is not re-shared.
- P1 locked after P2 used a teleporter: MiniMap.fullscreen (f@3314) makes minimap.controller exclusive on P1's controller,
  MiniMap.restore releases whatever access the map holds. Now: release P1's access when handing the map to P2, give P2 an
  exclusive access, dispose it when the map closes.
- Open: P2 floated after a double jump until down-slamming (owner: temporary). Cause unknown - need repro conditions.
