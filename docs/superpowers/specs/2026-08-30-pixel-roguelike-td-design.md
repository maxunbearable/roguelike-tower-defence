# Pixel-Art Roguelike Tower Defense — Design Spec

Date: 2026-08-30
Status: Approved for planning

## 1. Product statement

A pixel-art roguelike tower defense in C++. The distinguishing mechanic is the
**combination of a tower specialization with an element specialization**. A run
locks in exactly one of each, and the pairing defines the entire playstyle of
that run. Replayability comes from the combination matrix, not from randomized
maps or randomized drafts.

First release covers one tower (Arrow) and one element (Earth), each with three
specializations, producing nine distinct playstyles.

### Non-goals for this spec

Other towers, other elements (Fire/Water/Wind), bosses, maps 2-5, audio,
in-run relic rewards. All are designed *for* but not built.

## 2. Core mechanic: the combination matrix

Nine combinations are not authored individually. Three tower firing profiles and
three element hook behaviours are authored, and the nine emerge from their
interaction. Element potency scales with hit magnitude and hit frequency, so the
tower profiles differentiate the elements automatically.

| | Poison (DoT, spreads on death) | Rock (armour shred, petrify) | Earthquake (AoE pulse, stagger) |
|---|---|---|---|
| **Sniper** (few huge hits, long range, crits) | One shot injects a large stack; targets rot from a single mark. **The reliable answer to one big target** | Armour-piercing boulder; steady burst that ignores most armour outright | Long-range delayed epicentre per shot; artillery play |
| **Elf** (very fast, small hits, ramping) | Reaches poison's stack ceiling that heavy-hit specs never touch; best sustained DoT | Strips armour fastest of anything in the game. **The armour-melter** | Every Nth shot tremors; near-constant small quakes form a permanent slow field |
| **Hunter** (multishot, pierce, crowds) | Poisons a whole pack instantly; spread-on-death chains through them | Arrows shatter into ricocheting shrapnel; anti-swarm | Multiple simultaneous epicentres; overlapping quake zones |

This authoring ratio (6 pieces -> 9 outcomes) is what keeps four elements and
N towers tractable later. Hand-authored per-pair synergy bonuses are permitted
as flavour but must never be the mechanism.

## 3. Progression model

### Persistent (meta) layer

All three skill trees are permanent. They are bought with **Shards**, earned from
runs. The player eventually owns every branch of every tree.

- **Global tree** — always applies. Economy, lives, small universal stat boosts,
  and content unlocks (new towers, elements, specs).
- **Arrow tree** — a **trunk** (always applies when Arrow is used) plus three
  branches: Sniper, Elf, Hunter.
- **Earth tree** — a trunk plus three branches: Poison, Rock, Earthquake.

### Per-run layer

**Revised 2026-08-30.** Specs are not chosen at a menu. A tower is **built up
during the run with gold**, one purchase at a time:

```
build Arrow Tower (60)
  -> imbue with Earth (90)
  -> specialise the tower: sniper | elf | hunter (130)
  -> specialise the element: poison | rock | quake (120)
```

Each step is optional and independently priced, so a tower can fight
unspecialised, carry an element with no specialisation, or be fully built.

**The run commits on the first purchase that picks a spec.** The first tower to
take a tower spec locks that spec for the whole run; every later tower may only
take the same one. The element spec locks the same way, independently. This
preserves the one-spec-per-run rule while moving the decision *into* the run —
you commit under pressure, knowing what you are facing, rather than guessing at
a menu.

Only the committed branches fold into a tower's stat block; owned nodes in
uncommitted branches are inert. That is how "you own everything, but only one
spec plays" works with no special-casing. Each tower resolves its own stat block
from its own purchases, so a half-built tower is simply one whose spec fields are
still empty.

Within a run the only currency is **Gold**, spent on placing, levelling and
specialising towers. Trees = power you own; gold = power you deploy.

Each map alternates a **build phase** and a **wave phase**. During a build phase
the player places, upgrades and sells towers, then starts the next wave manually;
starting early awards a gold bonus proportional to the build time left unused.
Selling refunds a fixed fraction of gold spent.

Each placed tower can be upgraded through **three gold levels**. A level applies
flat multipliers to damage, range and fire rate, with costs and multipliers
declared in `towers/arrow.toml`. Tower levels are independent of the skill trees
and are lost when the map ends.

Starting lives for a run come from a base value plus Global tree modifiers.
Starting gold is declared per map.

### Run structure

A run is a chain of five maps, each ending in a boss. Milestone 1 ships **one
map, no boss**.

- **Lives carry across the whole run.** A leak on map 1 still hurts on map 4.
- **Gold and placed towers reset per map.** Each map is a fresh tactical puzzle.
- Between maps: a reward choice (relics). Designed-for, deferred.
- Death on any map ends the run. Shards awarded are
  `mapsCleared * MAP_BONUS + wavesSurvived * WAVE_VALUE + sum(killed.shardValue)`.
  All three constants live in the Global content file, so meta pacing is tunable
  without a recompile.

### Unlock tuning

Sniper and Poison are available from the first run. Elf, Hunter, Rock and
Earthquake are unlocked by cheap Global tree nodes. Tuning target: **all nine
combinations reachable within roughly five runs.** Map 1 must be clearable with
zero tree investment.

## 4. Architecture

```
core/      Pure C++, ZERO raylib. Stats, modifiers, skill trees, damage
           pipeline, status effects, economy, wave logic, RNG.
sim/       ECS world (EnTT) + systems. Also ZERO raylib. tick(dt).
content/   TOML -> core data structures. Towers, elements, trees, enemies,
           maps, waves. Includes the validation pass.
render/    raylib drawing of sim state. Read-only view of the sim.
ui/        Menus, skill tree screens, loadout, HUD.
app/       Window, main loop, screen state machine.
```

`core` and `sim` never link raylib. The entire combination matrix is therefore
headlessly testable in CI with no window. For a game whose value proposition is
a balance matrix, automated testing of that matrix outranks every other
structural concern.

### Directory layout

```
CMakeLists.txt
cmake/CPM.cmake
assets/        sprites/ tiles/ ui/ fonts/
content/       towers/ elements/ trees/ enemies/ maps/
src/
  core/        Stats  Modifier  SkillTree  Damage  Status  Economy  Rng
  sim/         World  Components  systems/  ElementBehavior
               elements/{Poison,Rock,Quake}
  content/     TomlLoader  Validate  Registry
  render/      PixelCanvas  SpriteAtlas  WorldRenderer
  ui/          SkillTreeScreen  LoadoutScreen  Hud  Widgets
  app/         Game  ScreenStack  main.cpp
tests/         core/  content/  matrix/
```

## 5. Stat and modifier system

### Stat blocks

Resolution order, computed at run start and recomputed whenever a tower is
levelled:

```
base stats
  -> global tree (owned nodes)
  -> arrow trunk (owned nodes)
  -> equipped tower spec branch (owned nodes)
  -> earth trunk (owned nodes)
  -> equipped element spec branch (owned nodes)
  -> in-run gold upgrades
```

### Modifier operations

A modifier is `{ target, op, value }` where `target` is a dotted stat path such
as `arrow.fireRate` or `earth.poison.dpsPerStack`.

Operations, applied in this fixed order so results are order-independent within
each class:

1. `add`  — summed
2. `mult` — multiplied
3. `set`  — overrides
4. `flag` — boolean union; grants trait components

Formula: `final = (base + sum(adds)) * product(mults)`, then `set` overrides,
then `flag` union.

### Arrow tower stats

`damage`, `fireRate`, `range`, `projectileCount`, `pierce`, `projectileSpeed`,
`critChance`, `critMult`, `armorPen`, `targetPriority`.

`targetPriority` is one of `first | last | strongest | weakest | closest`, where
*first* means furthest along the path. Default is `first`.

### Enemy stats

`maxHp`, `armor`, `speed`, `bounty` (gold), `shardValue`, `flying`, `size`.

### Earth element stats

Trunk: `potency` (scales effect magnitudes), `duration` (scales effect durations).

- Poison: `dpsPerStack`, `maxStacks`, `spreadPct`, `spreadRadius`
- Rock: `shredPerHit`, `shredDuration`, `petrifyChance`, `petrifyDuration`, `flatBonus`
- Earthquake: `radius`, `damage`, `slowPct`, `slowDuration`, `everyNShots`

## 6. Tower specs: modifiers plus trait components

A tower spec contributes stat modifiers *and* a set of declarative trait
components attached to every tower entity at spawn. Combat systems check for
traits generically.

- **Sniper** — `DamageVsHealthPct{threshold, mult}`, high range/damage,
  low fire rate, high crit.
- **Elf** — `RampUp{ratePerSec, maxMult, resetOnTargetSwitch}`, very high fire
  rate, low damage.
- **Hunter** — `SpreadShot{count, angleDeg}`, `Pierce{n}`, moderate damage.

Traits and their parameters are authored in TOML. Adding a new trait type is the
only case requiring new C++.

## 7. Element specs: hook behaviours

Exactly one element behaviour is active per run, selected at run start.

```cpp
struct ElementBehavior {
    virtual void onShoot(Ctx&, entity tower, entity projectile) {}
    virtual void onHit(Ctx&, entity source, entity target, float dealtDamage) {}
    virtual void onKill(Ctx&, entity target) {}
    virtual void onTowerTick(Ctx&, entity tower, float dt) {}
    virtual ~ElementBehavior() = default;
};
```

Implementations: `PoisonBehavior`, `RockBehavior`, `QuakeBehavior`. Each reads
its tuning values from the resolved stat block, so all numbers stay in TOML.

## 8. Damage pipeline

Exact evaluation order:

```
1. Tower fires projectileCount projectiles, geometry shaped by traits
2. onShoot hooks (element may attach a payload, e.g. quake epicentre marker)
3. Projectile travels; collides with an enemy
4. Compute damage:
     raw = damage
     apply trait damage modifiers (e.g. DamageVsHealthPct)
     if crit roll succeeds: raw *= critMult
     effectiveArmor = max(0, enemy.armor - activeArmorShred - tower.armorPen)
     final = max(raw - effectiveArmor, raw * MIN_DAMAGE_FLOOR)   // floor = 0.10
5. Subtract final from HP
6. onHit hooks (element applies statuses)
7. Pierce: decrement; continue or despawn
8. If HP <= 0: onKill hooks, award bounty, despawn
```

### Status effects

Statuses are ECS components: `Poisoned{stacks, dpsPerStack, remaining}`,
`ArmorShred{amount, remaining}`, `Slowed{pct, remaining}`,
`Petrified{remaining}`.

Stacking rule: **statuses refresh duration and take the maximum magnitude; they
do not stack in count.** The sole exception is Poison, which stacks up to
`maxStacks`. Slows take the maximum percentage rather than compounding.

Per-tick: poison deals `stacks * dpsPerStack * dt`; durations decrement and
expire; movement speed is `base * (1 - slowPct)`, or zero while petrified.

## 9. Content format

All gameplay data is TOML. Rebalancing never requires a recompile. The three
skill trees share one node schema.

```toml
[tree]
id = "arrow"
kind = "tower"            # global | tower | element

[[node]]
id = "arrow.trunk.dmg1"
name = "Sharpened Heads"
desc = "+10% arrow damage"
branch = "trunk"          # trunk | sniper | elf | hunter
cost = 1
requires = []
pos = [0, 0]
modifiers = [
  { target = "arrow.damage", op = "mult", value = 1.10 },
]
```

Maps declare the grid, buildable tiles, the fixed path as waypoint corners, and
the wave table. Enemies lerp between waypoints; there is no pathfinding at all.

```toml
[map]
id = "greenfields"
grid = [30, 15]
startGold = 200

# tiles: one line per grid row, one character per tile.
#   '.' buildable   '#' blocked   '=' path   'S' spawn   'E' exit
# (all 15 rows present in the real file)
tiles = """
S=====........................
.....=........................
.....==============...........
"""

# Waypoint corners only; enemies lerp between them. No pathfinding.
path = [[0,0],[5,0],[5,2],[18,2],[18,8],[29,8]]

[[wave]]
delay = 3.0
[[wave.group]]
enemy = "slime"
count = 10
interval = 0.8
```

Save data (Shards, owned node ids, unlocks, run seed, stats) is JSON.

### Content validation

A validation pass runs over all shipped content and is asserted by a unit test,
so malformed content fails CI rather than the player:

- every `requires` id resolves
- no cycles in prerequisites
- every modifier `target` is a known stat path
- every `enemy` id in a wave table resolves
- costs are positive; no orphan nodes; path waypoints lie on the grid

## 10. Determinism

A single seeded `std::mt19937` is owned by the run and injected into the combat
context. `rand()` and mid-run `std::random_device` are prohibited. The
simulation advances on a **fixed 1/60 s timestep** with an accumulator; rendering
interpolates positions between ticks. The run seed is persisted, which makes runs
reproducible and the matrix tests stable.

## 11. System tick order

```
1.  WaveSystem        spawn enemies
2.  StatusSystem      tick statuses, apply DoT, expire
3.  MovementSystem    advance along path at status-modified speed
4.  LeakSystem        enemies reaching exit cost a life, despawn
5.  TargetingSystem   towers select targets by priority
6.  FiringSystem      cooldowns, spawn projectiles, onShoot hooks
7.  ProjectileSystem  move, collide, damage, onHit hooks, pierce
8.  DeathSystem       onKill hooks, award bounty, despawn
9.  EconomySystem     gold accounting
10. WinLoseSystem     map cleared / lives exhausted
```

## 12. Screen state machine

```
Boot -> MainMenu -> MetaTrees (Global | Arrow | Earth tabs)
                 -> Loadout (equip 1 tower spec + 1 element spec)
                 -> Run
                      -> MapPlay (build phase / wave phase)
                      -> MapCleared -> RewardChoice -> next MapPlay   [deferred]
                      -> RunOver -> Results (award Shards) -> MainMenu
```

Milestone 1 implements every screen except RewardChoice.

## 13. Technology

| Concern | Choice |
|---|---|
| Language | C++20 |
| Build | CMake >= 3.24 with CPM.cmake |
| Render / input / audio | raylib 6.0 |
| ECS | EnTT |
| Content | toml++ |
| Save | nlohmann/json |
| Dev overlay | Dear ImGui + rlImGui, debug builds only |
| Tests | Catch2 v3 with CTest |

CPM is chosen over vcpkg because all four dependencies are clean CMake projects
with no awkward transitive graph, so CPM's pinning and caching suffice without
requiring a package-manager install step from any contributor.

### Pixel rendering

Virtual resolution is 960x540, drawn to a single `RenderTexture2D` and
integer-scaled to the window (x2 for 1080p). Tiles are 32 px, so the play area is
a **30x15 tile grid (960x480)** and the remaining **60 px is the HUD band**.
`TEXTURE_FILTER_POINT` throughout; integer scaling only, to avoid shimmer and
subpixel blur.

### Art

Phases 0-5 use coloured-rectangle programmer art. No art dependency exists until
Phase 6, so sprite sourcing never blocks gameplay work.

Candidate sources: Kenney's Tower Defense (Top-Down) pack (CC0, 300 assets, no
attribution required) for tiles and HUD; the free Archer Towers and TD Enemies
packs from free-game-assets for towers and the slime/wolf/goblin/bee roster.
**Their license text must be read and recorded before shipping** — only the
Kenney CC0 material is confirmed unrestricted today. A single base pack should be
chosen for towers and enemies so the styles stay consistent.

## 14. Testing strategy

- **core unit tests** — modifier operations, stat folding order, tree
  prerequisite resolution, the damage formula, status ticking and stacking rules,
  economy.
- **content tests** — every shipped TOML file loads and passes validation.
- **matrix tests** — a headless harness
  `simulateCombo(towerSpec, elementSpec, scenario, seconds)` returning damage and
  kills. Assertions are **relational guardrails, not magic numbers**, so that
  rebalancing does not break the suite. Examples: Elf+Poison out-damages
  Sniper+Poison against a sustained swarm; Sniper+Rock out-damages every other
  pairing against a single high-armour target; no combination is more than ~2x
  the median across the full scenario set.

## 15. Milestone 1 scope

One map, no boss, fixed hand-authored path. Arrow tower only, Earth element only,
but **all three tower specs, all three element specs and all nine combinations
playable**, trees purchasable with Shards, meta progress persisting between runs.

### Build phases

| Phase | Delivers | Verified by |
|---|---|---|
| 0 | CMake + CPM skeleton, raylib window, pixel pipeline, screen state machine, test harness | Window opens; `ctest` green |
| 1 | TOML map load, fixed path, enemies walking it, lives | Enemies walk spawn to exit; a leak costs a life |
| 2 | Arrow tower: placement, targeting, projectiles, damage, gold, waves, win/lose | A run is completable |
| 3 | StatBlock, modifier folding, trait components, three tower specs | Stat-folding unit tests; three specs measurably distinct |
| 4 | Element hook interface, status components, three Earth specs | Nine-combination matrix under automated test |
| 5 | Trees as data, tree UI, loadout screen, Shards, save/load | Progress survives a restart |
| 6 | Art pass, HUD, balance pass | Playable and legible |

## 16. Risks

| Risk | Mitigation |
|---|---|
| Balancing nine combinations is the genuinely hard part | Relational matrix guardrail tests; all tuning in TOML so iteration is fast |
| Scope creep into five maps and bosses | Explicitly deferred; schemas designed to accept them without restructuring |
| Mixed-resolution art packs clash (Kenney 64 px vs itch 32 px) | Single base pack for towers/enemies; decision deferred to Phase 6; placeholder art until then |
| Asset licensing beyond Kenney's CC0 is unverified | License text read and recorded before any distribution |
| Element behaviours drift toward per-pair special cases | Architectural rule: synergy bonuses are flavour only, never the mechanism |
