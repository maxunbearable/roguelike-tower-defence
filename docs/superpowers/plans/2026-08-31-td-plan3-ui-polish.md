# UI Polish Pass Implementation Plan

**Goal:** Bring the interface up to the standard the board already reached, without adding
a single mechanic.

**Scope rule (from the user, verbatim):** "do not add functionality, only improving what we
have". Every task below re-presents something that already exists. No new screens, no new
actions, no new systems, no new content.

**Diagnosis:** The board is now painted pixel art. The interface is still flat dark
rectangles and 10px text from the prototype. That mismatch — not any missing feature — is
what reads as "raw". The asset pack ships a complete UI kit (carved 9-slice panels,
banners, buttons with hover/pressed/disabled states, ribbons, bars, icon glyphs) that was
never imported. Meanwhile the skill-tree content already carries a `name` and `desc` for
all 23 nodes, and the UI throws both away except on hover.

**Tech Stack:** C++20, raylib 6.0, existing SpriteAtlas. No new dependencies.

## Global Constraints

- `td_core` (core/ sim/ content/) must never include or link raylib. Enforced at configure
  time. Pure geometry goes in `src/core/`, drawing in `src/render/`.
- Pixel art: integer scaling only, `TEXTURE_FILTER_POINT`, no fractional sprite scaling.
- `assets/sprites/*.png` stays gitignored — the pack licence forbids redistribution.
- 148 existing tests must stay green.
- Virtual resolution is fixed: `kVirtualW = 1408`, `kVirtualH = 800`, HUD band 96px at
  y=704.
- No new game mechanics, actions, screens, or content.

---

## Assessment: what is actually raw

Captured from the live build, one screenshot per screen.

| Screen | State |
|---|---|
| Slots | Title is the placeholder string `TOWER DEFENSE`. Bottom 55% of the screen is empty. Slots are flat unpainted rectangles. |
| Hub (skill trees) | Worst offender. The global tree is 3 circles of radius 17 holding a bare cost number. Node names and descriptions exist in content but only appear on hover. ~90% dead space; the tree is top-anchored at y=150 and spans only 200px of an 800px screen. |
| Radial menu | Four small dark discs with unlabelled 16px icons. No cost shown, no name shown. The translucent backing disc covers the tower being upgraded. |
| Results | Centred plain text on a dimmed board. No framing. |
| HUD | Functional and correctly wired, but flat rectangles and 10px labels. |

The fix is one coherent art+layout pass, in dependency order: foundation first, then the
screens worst-to-best.

---

### Task 1: Nine-slice foundation

Everything else depends on this. A 9-slice draw stretches a panel's edges and tiles
nothing, keeping corners pixel-exact at any size — which is the only way to get painted
panels at arbitrary UI rectangles without smearing the pixel art.

**Files:**
- Create: `src/core/NinePatch.h` (pure geometry, raylib-free, header-only)
- Create: `tests/core/nine_patch_test.cpp`
- Modify: `src/render/SpriteAtlas.h/.cpp` (add `drawNine`)
- Modify: `tools/import_tinyswords.py` (import the UI kit)

**Interfaces:**
- Produces: `core::ninePatch(srcW, srcH, inset, dstX, dstY, dstW, dstH) -> std::array<NinePatchQuad, 9>`
  where `NinePatchQuad { Rect src; Rect dst; }` and `Rect { float x, y, w, h; }`.
- Produces: `SpriteAtlas::drawNine(id, x, y, w, h, inset, tint)`.

Steps:

- [ ] **Step 1: Write the failing test.** Cover the three properties that matter: corners
      keep their source size, the nine destination quads exactly tile the requested
      rectangle with no gap or overlap, and a destination smaller than twice the inset
      still produces a valid (clamped, non-negative) layout rather than inverted rects.
- [ ] **Step 2: Run it, confirm it fails to compile** (`NinePatch.h` does not exist).
- [ ] **Step 3: Implement `core::ninePatch`.** Clamp the inset to half the destination on
      each axis so tiny panels degrade gracefully instead of producing negative widths.
- [ ] **Step 4: Run the test, confirm green.**
- [ ] **Step 5: Import the UI kit** — carved panel, banners, the four button states,
      ribbons, and the icon glyph sheet. Halved like every other sprite.
- [ ] **Step 6: Add `SpriteAtlas::drawNine`,** built on `core::ninePatch`.
- [ ] **Step 7: Verify** the full suite stays green and the app still builds.

Verification for every later task is render-and-look: capture the screen, open it, judge
it. That loop already caught the shading bug, the bush/slime confusion and the tiling
lattice, and it is the only honest test for whether a screen reads.

---

### Task 2: Radial menu

The build menu is the control the player touches most, and right now it shows neither what
a button does nor what it costs. Kingdom Rush's rule — "a quick description of the major
tower upgrades is displayed to the side" — is exactly the missing piece.

**Files:** Modify `src/ui/RadialMenu.h/.cpp`, `src/app/Game.cpp` (item labels only)

- [ ] Give every ring button the painted button art, with the pack's hover and disabled
      states driving the existing hot/affordable flags.
- [ ] Put the cost on the button and the name in a carved side panel that follows the
      hovered item. Both strings already exist on `RadialItem`.
- [ ] Stop covering the tower: the backing disc becomes a soft vignette ring rather than a
      filled circle over the subject.
- [ ] Grey unaffordable items with `Button_Disable` instead of silently offering them.

---

### Task 3: Hub skill trees

The single biggest readability win in the game, and almost entirely a layout problem: the
data is already authored.

**Files:** Modify `src/ui/Screens.cpp`

- [ ] Scale the tree to the space it has: raise node radius and spacing, and centre the
      whole tree on the panel rather than anchoring it at y=150. Compute the layout from
      the node position extents so all three trees (which differ in width) each sit
      centred.
- [ ] Show each node's `name` under its circle at all times. Keep the hover panel for the
      `desc` and cost.
- [ ] Label the branch columns — trunk / sniper / elf / hunter, poison / rock / quake —
      so the specialisation structure is visible without buying anything.
- [ ] Put the tree on a carved panel, the heading in a ribbon, and the tabs in ribbon art
      with a clear active state.
- [ ] Add an owned/affordable/locked legend. Three colours currently carry that meaning
      with nothing explaining them.

---

### Task 4: Slots screen

The first thing a player sees, and it currently shows a placeholder title over half a
screen of black.

**Files:** Modify `src/ui/Screens.cpp`

- [ ] Replace the placeholder title. **Assumption, stated rather than asked:** the game is
      called **WARDSTONE** — wards defend, the earth element is stone, and runs pay out in
      shards of it. It lands as one named constant so it is a one-line change if the user
      disagrees.
- [ ] Title on a banner; each slot a carved panel; ERASE and the implicit
      new-game/continue actions become real painted buttons.
- [ ] Fill the vertical space: taller slot cards carrying the stats that already exist
      (best wave, runs played, skills owned, run-in-progress).
- [ ] Make CONTINUE visually primary on a slot with a run in progress. The distinction the
      user asked for ("new game - 3 save slots - continue") exists in logic but not in the
      art.

---

### Task 5: Results screen

**Files:** Modify `src/ui/Screens.cpp`

- [ ] Put the result on a carved panel with the outcome in a ribbon — red for defeat,
      yellow for a clear.
- [ ] Group the numbers that already print (waves survived, the spec line, shards awarded,
      shards total) into a readable block instead of a centred column of loose text.

---

### Task 6: HUD

**Files:** Modify `src/ui/Hud.cpp`

- [ ] Carved 9-slice band instead of a flat rectangle.
- [ ] Lives and gold get the pack's bar art and larger numerals; they are the two values
      read under pressure.
- [ ] Speed/pause controls become painted buttons with real pressed and active states.
- [ ] Audit the layout at the full 1408px width — it was written for a 960px board and
      still assumes it in places.

---

### Task 7: Enchantment overlays and impact effects

The last generated art still on screen, and it clashes with everything imported.

**Files:** Modify `tools/import_tinyswords.py`, `src/render/Renderer.cpp`, `src/render/Effects.cpp`

- [ ] Replace `enchant_poison/rock/quake` with pack-derived art so the element overlay
      matches the buildings it sits on.
- [ ] Use the pack's Explosion and Fire sheets for the impact and death effects that
      `Effects.cpp` already fires. Same events, same timings — better art.

---

## Out of scope

Named explicitly so the boundary is not accidentally crossed: bosses, maps 2–5, music,
tutorial/onboarding, a settings screen, damage numbers, new enemies, new towers, new
elements, new skill-tree nodes, and any change to balance or mechanics. This pass changes
how the game looks and reads, not what it does.

---

## Execution log

Every deviation from the plan above, and why.

**Task 1 — nine-slice foundation. Done, with one correction found by looking.**
The pack's `9Slides`/`3Slides` naming is literal: overlaying a 64px grid on the
sources showed the slice lines landing exactly on the cell boundaries, so the
inset is 64 at source and 32 halved. Confirmed by measurement rather than
assumed.

The first implementation **stretched** the centre and edges. On the hub's
1360x620 panel that scaled a 32px patterned cell across the whole panel and
turned the parchment hatching into giant blocks — clearly visible in the first
capture. Fixed by tiling every stretchable region and drawing only the corners
1:1, via a second pure helper (`core::tileRuns`) with its own tests. 8 tests.

**Task 2 — radial menu. Done.** Painted buttons with the pack's hover and
disabled art, ring instead of a filled disc so the tower stays visible, button
radius 20→27 and ring 58→74 because a fitted building icon at radius 20 came out
around 34px and read as a smudge. The Build button now shows `tower_plain`, the
actual building, matching the specialise buttons and the skill tree.

**Task 3 — hub skill trees. Done, and the biggest single win.** Node radius
17→30, spacing 78/66→150/128, layout derived from each tree's node extents so
all three centre themselves. Every node's authored `name` is now always visible;
it used to appear only on hover. Branch hues are applied to links and rings even
before anything is owned, so the three specialisation paths separate on sight.
Legend added.

Deviation: the plan called for **branch column labels**. Dropped — each branch's
root node is already named "Sniper"/"Elf"/"Hunter", so a label would have
duplicated it. Colour coding carries the grouping instead.

Element branches (poison/rock/quake) have no `tower_<branch>` art and were
drawing blank circles; they now fall back to `icon_<branch>`. That needed
`drawFitted` to be able to grow a sprite, which it never could — a 12px icon in
a 43px circle stayed 12px. Growth is integer-only; a fractional upscale of pixel
art shimmers.

**Task 4 — slots screen. Done.** The title was the placeholder `TOWER DEFENSE`
and is now `kGameTitle`, one constant. **Assumption, not a decision the user
made: the name is WARDSTONE.** A ribbon is only 32px tall so a 40px title
overflowed it; the banner plate is used instead.

**Task 5 — results screen. Done.** Carved panel, outcome on a red or yellow
ribbon, numbers grouped. The gem icon was first placed at a fixed offset and
landed on top of the award text; it is now positioned off the measured string.

**Task 6 — HUD. Done.** Painted band and buttons. The layout genuinely did still
assume the 960px board: content sat in x 24..620 and 1100..1408 with the middle
third empty. Rezoned across the full width. Lives and gold are 40px numerals —
they are what gets read under pressure. Two collisions found by looking and
fixed: a four-digit gold total ran into the wave text, and four fielded specs at
20px wrapped to three rows and spilled out of the band.

The pack's 1/2/3 glyphs now carry the speed control and a real speaker glyph the
mute, replacing a `%dx` caption and the ASCII strings `))` and `x`.

Removed `drawMainMenu` and `drawRunOver` — dead code, superseded by the slots
and results screens, and carrying a second stale `TOWER DEFENSE` placeholder.

**Task 7 — partly dropped, because the premise was wrong.** The plan asserted
the generated enchantment overlays "clash with everything imported". Captured at
3x on an imbued tower, they do not: the green stalks read as vines on the tower
base and sit in the same palette. Replacing working art for its own sake would
have been churn, so they stayed. The pack has no better candidate for an element
overlay either — its Effects folder is one explosion sheet and one fire sheet.

The impact/death effects were also left alone. `Effects` already does particles,
rings, floating damage numbers and screen shake procedurally and in palette;
swapping in sprite-sheet explosions would be a lateral move, not an improvement.

**Result:** 148 tests before, **156 after** (8 new, all on the nine-slice
geometry). All green.
