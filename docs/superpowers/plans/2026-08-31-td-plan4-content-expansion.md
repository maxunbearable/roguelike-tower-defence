# Content Expansion Implementation Plan

**Goal:** Five maps with bosses, four elements, three tower types — the full shape
the original design brief described, built on the combination engine that already
exists.

**Spec:** `docs/superpowers/specs/2026-08-30-pixel-roguelike-td-design.md`

**Architecture:** The combination engine is already data-driven and this plan
leans on that rather than fighting it. A tower spec is stat modifiers plus
flag-granted trait components; an element spec is one hook object
(onShoot/onHit/onKill/onTowerTick). Nine behaviours already emerge from six
authored pieces with no per-pair branching. Scaling to 9 tower specs × 12 element
specs must add **no** pairing logic — 108 combinations from 21 authored pieces.

**Tech Stack:** C++20, EnTT, toml++, raylib 6.0. No new dependencies.

## Global Constraints

- `td_core` (core/ sim/ content/) never includes or links raylib.
- Determinism: one seeded `std::mt19937_64` per run, injected; `rand()` prohibited;
  RNG state serialised into saves. New content must not break seed replay.
- Fixed 1/60s simulation timestep.
- No per-pair branching anywhere in the combination engine. If a change needs to
  name a tower spec and an element spec together, the design is wrong.
- `assets/sprites/*.png` stays gitignored; every new sprite id needs a generated
  fallback so a fresh checkout runs.
- 156 existing tests stay green.

## What is already data-driven (verified by reading, not assumed)

- `Registry` globs every file in each content dir into an id-keyed map and
  exposes `maps()`, `towers()`, `elements()`, `trees()`. Multi-content needs no
  loader change.
- Maps are entirely TOML: tiles, waypoints, wave recipe, enemy pool.
- Enemies are entirely TOML, including per-damage-type `resist`.
- Tower spec stats and tower trees are entirely TOML.
- Element **stats** are TOML; element **behaviour** is C++.

## What needs C++

| Need | Why |
|---|---|
| Element behaviours (Fire, Water, Wind) | Hook objects; `makeElement` dispatch currently hardcodes `earth` |
| New tower traits (splash, chain, amplify, drain) | `rebuildTower` maps flags to components by name |
| Boss support | Wave recipe has no boss concept; no boss HP bar; hard CC must not trivialise bosses |
| Map select + per-map progress | Save has one global `bestWave`; no map selection at all |
| Dynamic hub tabs | `kTabIds[3]` is hardcoded to global/arrow/earth |
| Multi-tower build menu | `buildMenuItems` hardcodes `"arrow"` |

---

## Content design

### Towers (3)

Each keeps one damage type and three specs that differ in **firing profile**, so
the elements differentiate themselves across them without naming anyone.

| Tower | Damage | Profile | Specs |
|---|---|---|---|
| `arrow` | piercing | baseline single-target | sniper (heavy/slow/crit) · elf (fast/ramping) · hunter (multishot/pierce) |
| `cannon` | siege | slow, heavy, splash | mortar (long range, wide splash) · bombard (fast short shells) · siege (one huge shell, armour demolition) |
| `arcane` | arcane | medium rate, utility | tempest (chains) · hex (amplifies damage taken) · drain (kills pay gold) |

New traits: `trait.splash` (radius, falloff), `trait.chain` (jumps, falloff),
`trait.amplify` (pct, duration), `trait.drain` (goldPerKill).

Art from the pack, real distinct silhouettes: cannon → Black Buildings
Barracks/Tower/Castle; arcane → Monastery variants.

### Elements (4)

| Element | Damage | Specs |
|---|---|---|
| `earth` | earth | poison (stacks, rewards hit count) · rock (armour shred, rewards hit count) · quake (periodic AoE) |
| `fire` | fire | burn (DoT scaled by hit MAGNITUDE, rewards heavy hits) · blast (AoE per hit, rewards multishot) · melt (strips resistance, rewards rate) |
| `water` | frost | chill (stacking slow) · shatter (bonus vs slowed, scales with slow) · freeze (hard CC chance) |
| `wind` | shock | shock (chains to nearby) · gust (pushes back along path) · cyclone (periodic vortex) |

Deliberate: each element has one spec that rewards hit **magnitude**, one that
rewards **count**, and one that is **periodic**. That is what makes all 108
pairings differ without any pairing code.

### Maps (5) and bosses

| Map | Theme | Resistance bias | Mid-boss (25) | Boss (50) |
|---|---|---|---|---|
| `greenfields` | meadow | none | Ogre Warlord | Warlord Grulk |
| `ashen-wastes` | volcanic | fire-resistant | Cinder Brute | Cinder Colossus |
| `frostmere` | frozen | frost-resistant | Rime Stalker | Rime Tyrant |
| `blightmarsh` | swamp | earth/poison-resistant | Plague Carrier | Plague Mother |
| `obsidian-gate` | final | mixed, high armour | Gate Sentinel | Gate Warden |

Resistance bias is the point: a map that resists earth forces a different
element, which is what makes five maps replayable rather than one map five times.

---

## Stages

Each stage ends green and playable. Later stages are pure content on top.

### Stage A — systemic groundwork
Riskiest, unblocks everything else.

- A1. Boss support in the wave generator: `bossWaves`, `boss`/`midBoss` ids,
      deterministic placement. Tests: boss appears exactly on the named waves;
      the curve is unchanged on non-boss waves; expansion stays deterministic.
- A2. `EnemyDef.boss` flag. Hard CC (petrify, freeze) becomes a heavy slow on
      bosses instead of a full stop, so a boss cannot be trivially locked down.
      Test: petrify on a boss reduces speed but never to zero.
- A3. Per-map progress in the save: `mapProgress[mapId] = {bestWave, cleared}`.
      Backward compatible with existing saves. Tests: round-trip; a v1 save loads.
- A4. Map select screen, maps locked until the previous is cleared.
- A5. Dynamic hub tabs from `registry.trees()`, ordered global → towers → elements.
- A6. Multi-tower build menu from `registry.towers()`; element menu from
      `registry.elements()`.

### Stage B — new traits and elements
- B1. Traits: splash, chain, amplify, drain in `rebuildTower` + combat systems.
      Tests per trait.
- B2. `makeElement` becomes a registry keyed by element id, so a new element is
      one file plus one registration line.
- B3. Fire, Water, Wind behaviours + TOML + trees. Tests per spec.

### Stage C — new towers
- C1. `cannon` and `arcane` tower defs, level curves, trees, sprites.
- C2. Balance via the existing matrix harness; extend it to the full
      tower-spec × element-spec grid and keep the relational guardrails.

### Stage D — maps and bosses
- D1. Four new map files, generated from waypoints so tiles and path cannot
      disagree (the lesson from the first hand-authored map).
- D2. Ten boss enemy defs, wired into each map's recipe.
- D3. Boss HP bar UI.
- D4. Rebalance the shard economy: five maps means far more income, so tree costs
      must be rescaled or meta progression ends in one map.

---

## Out of scope

Music, tutorial, settings, new mechanics beyond the traits listed, and any
change to the fixed-path design. Tower and element counts stop at 3 and 4.

---

## Execution log

**Stage A — systemic groundwork. Done.**

- Boss waves in the generator: `[[waves.boss]]` with a wave and enemy id, added as
  an extra group behind the escort rather than replacing it. 6 tests, including
  that a boss does not disturb the enemy rotation or scaling of the wave it lands
  on, and that a boss named beyond the recipe is ignored rather than crashing.
- Hard CC on bosses became a heavy slow (`kBossHardCcSpeed`), and soft slows are
  capped (`kBossMaxSlow`). **The first version of this test passed vacuously** —
  it asserted a petrified enemy travels 0 distance, which is also true when the
  movement system never runs, because the world starts in the build phase. It now
  confirms the enemy moves before freezing it.
- Per-map progress in the save, keyed by map id so adding or reordering maps
  cannot shift anyone's record. A version-1 save with no `mapProgress` key still
  loads, with a test that pins that.
- Map select screen; maps unlock by CLEARING the previous one, not by reaching a
  high wave on it.
- Hub tabs derived from `registry.trees()`, ordered global → towers → elements.
  Eight tabs now fit across 1408px. The campaign order is authored per map
  (`order = N`) because the registry stores maps alphabetically, which is not the
  order they are played in.
- Build ring offers every tower in content; element page offers every element.

**Stage B — traits and elements. Done.**

Four new traits (splash, chain, amplify, drain) granted by flag exactly like
execute and rampUp. `makeElement` became a four-line dispatch in its own file, so
an element is now one new file plus one line. The shared AoE helpers moved to
`sim/AreaEffects.h` — they were private to Earth.cpp, and every new element and
area trait would have copied them.

Fire, Water and Wind shipped with three specs each. Each element deliberately has
one spec gated on hit MAGNITUDE, one on hit COUNT and one PERIODIC, which is what
makes all 108 pairings differ with no pairing code.

**Stage C — towers. Done.** Cannon (siege, splash) and Arcane Spire (support:
chain / amplify / drain), each with three specs, level curves and trees. Art from
the pack: three colour families with a different SILHOUETTE per spec, not a
recolour — Castle (156px wide) and most Monasteries (132px tall) were rejected
because at a 64px tile they swallow their neighbours.

**Stage D — maps and bosses. Done.**

`tools/make_map.py` generates each map's tiles from its waypoints and refuses to
write one that is not axis-aligned, leaves the grid, crosses itself, or does not
start and end on an edge. This exists because the first hand-authored map shipped
with its tiles disagreeing with its path.

Sixteen per-map enemy variants reuse the four imported sprite sets through a new
`sprite` field plus a `tint`, so each map fields a roster with its own resistance
bias without new art. The theme resistance is merged with each creature's own
quirk, so a goblin stays earth-hardened wherever it appears.

Ten bosses, two per map. HP is set against the curve rather than by feel: at wave
50 greenfields' multiplier is ~198x, so a 1200-base boss lands at ~238,000, about
7x a regular enemy of that wave. Boss health bars render across the top of the
field.

**What the expanded matrix caught.** Generalising `simulateCombo` to take the
tower and element ids turned 9 measured pairings into 108, and it immediately
found two things:

1. **Cyclone was a runaway** — 79% on the fastest tower against a row median of
   23%. Shot-counted cadence times a flat payload means a high-rate tower gets
   the effect almost continuously for free. Retuned to every 7th shot for 6.
2. **Four specs are structurally unmeasurable by that harness.** chill, freeze,
   shatter and gust scored identically to a bare tower, or worse: the scenario
   holds enemies stationary to measure throughput, so a slow does nothing,
   shatter never triggers, and gust shoves the target permanently out of range.
   They are excluded from the throughput band with that reasoning written down,
   and asserted on their actual mechanism in `test_control_elements.cpp` against
   moving enemies instead. Tuning them to satisfy a throughput harness would have
   been the wrong fix.

The band is now applied PER TOWER rather than across all 108, because cannon
shells hit far harder than arrow volleys and cost more to field — one band across
three towers would measure base damage, not balance.

**Economy: verified rather than changed.** Owning every node across eight trees
costs 3341 shards. Measured: a fresh profile reaches wave 18 for 117 shards, a
fully upgraded one wave 34–37 for ~400. That is roughly 8 strong runs to own
everything across five maps, which is a reasonable arc, so no rescale was needed.
The economy test used to sum a hardcoded `{global, arrow, earth}` and was
therefore measuring a third of the real cost; it now sums every tree.

**Two stale things removed.** `requestStart(demoTowers)` had always called
`openMenuAt(20, 7)`, so every `--autostart` screenshot had a radial menu sitting
over the board. And the enemy-count test asserted exactly 4 enemies, which broke
the moment bosses shipped in their own file.

**Result:** 165 tests before this plan, **175 after**. All green.

## Still open

- Element overlay art for the nine new element specs (`enchant_*`); they degrade
  to no overlay rather than breaking.
- Element icons for fire/water/wind in the build ring fall back to `icon_gem`.
- The AutoPlayer only builds arrow towers, so the difficulty report measures an
  arrow-only baseline and understates what a real player can field.

---

## Addendum: second content pass

Two more towers and two more elements, taking the matrix from 108 to **270**
combinations (15 tower specs × 18 element specs) built from 33 authored pieces.
Still no per-pair code.

### Towers

| Tower | Damage | Identity | Specs |
|---|---|---|---|
| `ballista` | impact | longest reach, pierces a whole rank | harpoon (one huge bolt) · scorpion (rattling stream) · javelin (impacts knock down) |
| `brazier` | searing | shortest range, fastest cadence; placement is everything | pyre (wide splash) · **forge (buffs neighbouring towers)** · cinder (relentless, ignores armour) |

Two new traits: `trait.slowOnHit` (javelin's knockdown) and `trait.towerBuff`
(forge's aura). Forge is the first tower in the game whose value is what it does
for its *neighbours* — it trades 45% of its damage and 30% of its rate for a
±18% damage and rate aura, and deliberately does not buff itself, or a lone
forge would be a strictly better tower rather than a support choice.

`Buffed` is recomputed every tick by `runTowerBuffSystem` rather than folded into
`TowerStats`, because TowerStats is a pure function of what has been *bought* and
this changes as neighbours are built and sold.

### Elements

| Element | Damage | Identity | Specs |
|---|---|---|---|
| `shadow` | void | attrition — nothing helps the enemy in front of you | wither (permanent, no timer) · siphon (kills return lives) · rift (a kill leaves a burning hole) |
| `light` | radiant | the answer to a map built to resist you | sear (ignores resistance entirely) · judgement (smites the healthiest in reach) · beacon (marked targets share what they take) |

Three of these needed genuinely new mechanisms rather than another variation:

- **Wither** is the only effect in the game with **no timer**. It accumulates all
  wave and is lost only when the enemy dies, and it amplifies *every* source, not
  just the tower that applied it. It is deliberately absent from `StatusSystem`.
- **Sear** bypasses resistance via a new `dealFlat`. Because it is flat per hit
  rather than proportional, it is proportionally enormous on a fast weak tower and
  nearly irrelevant on a siege shell — the opposite gradient to every other
  element, which is exactly why it answers a resistant map.
- **Beacon** applies a mark, and the *combat system* shares damage from any
  tower's hit on a marked target. Putting it there rather than in the element hook
  is what lets one beacon tower upgrade the whole board's fire instead of only its
  own. **Rift** lives on its own entity, because the enemy that opened it is gone.
- **Siphon** needed `World::gainLife`, capped at the starting count so a long run
  cannot bank an unlosable buffer.

### What the matrix caught this time

`brazier/forge` scored **1–15%** against a row median of ~2.5%, failing both the
upper and lower band. That is correct and expected: measured alone against a fixed
pile of health, a support spec that gave up its own damage is nearly inert, and
the flat and periodic elements then dominate its row because its own hits
contribute almost nothing.

Same resolution as the control elements: `isSupportTowerSpec` excludes it from the
throughput bands with the reasoning written down, and its mechanism is asserted
directly — a tower beside a specialised forge does >10% more damage than the same
tower alone, and a forge does not buff itself.

### Two brittle things this broke

- `test_towers.cpp` asserted `placeTower(2, 1, "ballista") == UnknownTower`, using
  "ballista" as a deliberately non-existent id. It became a real tower. Now uses
  `"no_such_tower"`.
- The matrix asserted `towers.size() == 9 && elements.size() == 12` and
  `checked == 108`. All three are now derived from the registry — hardcoding the
  count in the test that exists to catch content growth was self-defeating.

### Hub tabs went to two rows

Twelve trees at one row of 118px tabs needs 2064px on a 1408px board. Six per row
at 200px fits with room for readable labels.

### Economy

Owning every node is now **5357 shards across 126 nodes**. Against a measured
~400 shards for a strong run that is roughly 13 runs, which still sits inside the
guardrail (`treeCost` between 5× and 60× a first run).

**Result: 177 tests, all green.** Suite runtime is now ~2.5 minutes, dominated by
the 270-combination matrix.
