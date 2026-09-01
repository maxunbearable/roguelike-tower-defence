# Plan 7 — Player Agency and GUI

**Goal:** Stop the game being "raw" by giving the player control of, and sight
of, the systems that already exist — and rebuild the HUD around them.

**Why this and not something else.** Research (below) says the two things that
separate a good tower defence from a dull one are *agency during a wave* and
*total information*. Auditing this codebase against that turned up something
specific and embarrassing: **the depth is already built and simply not reachable.**

- `TargetPriority` has five modes — First, Last, Strongest, Weakest, Closest —
  fully implemented in the targeting system, validated at content load, and
  **the player can never change any of them.** It is authored per tower and
  frozen for the whole run.
- Pause hard-returns with `// frozen: no simulation, no build input`, which is
  the exact opposite of the single most-cited lesson in the research.
- The radial menu's *side panel* clamps to the viewport. Its *ring buttons* do
  not, so a tower near the right edge opens a menu with unclickable options
  hanging off the screen. Confirmed by screenshot.

None of that is a missing feature. It is built work the player cannot touch,
which is precisely what "raw" feels like from the outside.

## Research

- [Defender's Quest — *Optimizing Tower Defense for FOCUS and THINKING*](https://www.fortressofdoors.com/optimizing-tower-defense-for-focus-and-thinking-defenders-quest/)
  — **total time control**: the player can pause mid-wave *and still issue
  commands*, so the game tests thinking rather than reflexes ("the player can
  always whack pause and say 'alright, let's THINK about this.'"). Also **total
  information**: exact numbers on mouseover, never "50% more damage" but "5
  damage per second for 5 seconds". Also warns against **lock-and-key** design,
  naming air units as "the quintessential example".
- [TowerWard — *Target Priority Tier List*](https://towerward.com/blog/target-priority-tier-list-tower-defense-games)
  — smart targeting is a primary source of depth; priority should be set
  deliberately before a wave rather than fiddled with during one.
- [Stardock — *What Makes A Good Tower Defense Game?*](https://www.stardock.com/games/article/495008/siege-of-centauri-dev-journal-what-makes-a-good-tower-defense-game)
  — players need enough to *do*; watching is not gameplay.
- [Design The Game — *Tower Defense Design Guide*](https://www.designthegame.com/learning/tutorial/tower-defense-design-guide)
  — show range, show upgrade deltas, make decisions calculated rather than
  guessed.
- [Indie Dev Guide — *Game HUD Design Techniques*](https://www.indiedevguide.com/articles/game-hud-design-techniques-ui-ux-indie-devs/)
  and [Wes Plays — *Visual Hierarchy in Tower Defense Design*](https://www.wesplays.com/wes-plays/from-chaos-to-clarity-visual-hierarchy-in-tower-defense-design)
  — group related readouts, rank by importance, avoid dead space and uniform
  grey text.

One research point we already satisfy by accident and should not break: our six
flying enemies are **not** lock-and-key. They follow the same path and every
tower can shoot them, so `flying` is a rendering and flavour distinction, not a
wall that demands one specific counter.

## Tasks

### Task 1 — Targeting priority, exposed
Add a targeting page to the radial menu listing all five modes with the current
one marked. Persist the choice per tower in the save and through the run
snapshot. Show the mode in the tower stat panel.

Files: `src/sim/World.{h,cpp}`, `src/core/SaveGame.{h,cpp}`, `src/ui/RadialMenu.h`,
`src/app/Game.cpp`, `tests/sim/test_targeting_choice.cpp`.

Tests must prove the setting *changes which enemy is shot*, not merely that a
field round-trips — a getter/setter test would pass even if targeting ignored it.

### Task 2 — Act while paused
Pause freezes the simulation but no longer swallows build input. The pause panel
keeps ownership of clicks that land **on** it; clicks elsewhere build, upgrade,
sell and retarget as normal.

Files: `src/app/Game.cpp`, `src/ui/Screens.cpp`, `tests/…` (input is not
headlessly testable, so this is verified by screenshot plus the hit-test unit).

### Task 3 — Radial menu clamping (bug)
Clamp the ring centre so every button stays inside the play area.

Files: `src/ui/RadialMenu.cpp`, `tests/ui/test_radial_layout.cpp`.

### Task 4 — HUD rebuild
Fix the dead space between fixed columns, rank the readouts, make the FIELDED
list legible, and surface tower count and targeting.

Files: `src/ui/Hud.cpp`.

### Task 5 — First-run guidance
Contextual one-line hints that fire once each and are remembered in the profile,
covering the things the game currently never explains: building, that a tower
must be maxed before it can specialise, that elements are bought in the trees,
and that calling a wave early pays.

Files: `src/core/SaveGame.{h,cpp}`, `src/app/Game.cpp`, `src/ui/Hud.cpp`.

## Out of scope, deliberately

Rewriting all 33 spec descriptions into exact-number form ("12 damage per second
for 4 seconds"). The research is right that this matters, but it is a content
pass across every tower and element tree and it does not fit beside the code
work above. The stat panel already shows exact damage, fire rate, DPS and range;
this plan adds targeting to that panel. Logged as the next content task.

---

## Execution log

What actually happened, including where the plan was wrong.

**Task 1 — targeting, and a data-loss bug found while doing it.** Wiring the
choice through the save exposed something worse than the missing UI: `TowerSave`
had **no tower id**, and `World::restore()` hardcoded `defs_->tower("arrow")`.
Every saved cannon, arcane spire, ballista and brazier came back from Continue as
an arrow tower. Harmless when arrow was the only tower; a straight loss of the
player's most expensive purchases once tower types became unlockable. Both
`towerId` and `priority` are now saved, defaulted so older saves still load.

The choice had to live on `TowerTag`, not `TowerStats`: `statsFor()` re-reads
`targetPriority` from the tower *definition* and `rebuildTower()` runs on every
upgrade, imbue and specialisation, so a choice held on the stats was wiped the
next time the tower changed. There is a test for exactly that.

The behavioural test derives its in-range path distances from the path and the
tower's actual range instead of hardcoding coordinates — hardcoded path
coordinates in this suite have already been invalidated once by a map rework.

**Task 2 — pause.** The plan said "let the player act while paused". The real
problem was that P and ESC opened the *same* screen, and that screen was a
dimmed settings modal. Split into two: `paused` is tactical (simulation frozen,
board live and clickable, slim banner) and `menuOpen` is the settings modal
(dims, blocks). Both freeze the simulation; only one takes the cursor.

**Task 3 — clamping.** Done, and moved to `core/RingLayout.h` because the test
binary does not link raylib — the same architecture boundary that put
`NinePatch` there. Writing it caught a latent UB: `std::clamp(v, lo, hi)` with
`lo > hi` is undefined, which a naive clamp hits on any viewport narrower than
the ring. Guarded and pinned by a test.

Two further bugs fell out of this one:
- `showPage()` would open a ring for tile (-1,-1) when nothing was selected. The
  new clamping then parked it neatly in the top-left corner — a menu about
  nothing. Now guarded.
- `devCluster` looped `while (upgradeCost(x,y) > 0) upgradeTower(x,y)`.
  `upgradeCost` stays positive when a level is merely *unaffordable*, so once the
  gold deficit landed this **spun forever** and every screenshot using
  `--cluster` hung. It now tests the upgrade, not the cost, and funds itself.

**Task 4 — HUD.** Captions on every zone, hairline rules between them, tower
count added, and the FIELDED list turned into chips. First attempt used a dark
chip with the existing warm ink and rendered as **solid blocks with the label
invisible inside them** — caught by screenshot, fixed to a light chip with dark
ink.

**Task 5 — hints.** Five one-shot lines, stored per profile by ID so rewording
one never re-fires it. The message line was 10px red text tucked under the wave
counter, where a teaching line is effectively invisible; it is now a centred
banner above the band that fades out.

Icons: the ring shows icons and only reveals labels on hover, so five identical
crosshairs would have been a menu you must hover five times to read. Five
distinct 12x12 icons were authored in `tools/make_sprites.py` — forward chevrons,
reversed chevrons, a full health bar, a nearly-empty one, and a crosshair.

**Not done:** rewriting all 33 spec descriptions into exact-number form, as
scoped out above.
