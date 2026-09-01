# Plan 2 — The Combination Engine

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans.

**Goal:** Nine distinct playstyles from three arrow-tower specs crossed with three Earth element specs, produced by composition rather than by nine hand-written cases, and held in place by automated matrix tests.

**Architecture:** Two orthogonal mechanisms. A **tower spec** contributes stat modifiers plus declarative *trait components* — it changes numbers and geometry. An **element spec** contributes one *hook object* (`onShoot/onHit/onKill/onTowerTick`) — it changes events. Because element potency scales with hit magnitude and hit frequency, the three firing profiles differentiate the three elements automatically. Everything both mechanisms read is resolved into a single `StatBlock` by folding modifiers in a fixed order, so all tuning stays in TOML.

**Spec:** `docs/superpowers/specs/2026-08-30-pixel-roguelike-td-design.md` sections 2, 5, 6, 7, 8.

**Builds on:** Plan 1 (`docs/superpowers/plans/2026-08-30-td-plan1-foundation.md`).

## Global Constraints

Plan 1's constraints all still hold — `td_core` stays raylib-free, fixed 1/60s timestep, seeded RNG, all gameplay numbers in TOML. Additionally:

- **No per-pair special-casing.** There must be no code branch of the form "if spec==sniper && element==poison". Synergy is emergent; hand-authored pair bonuses are permitted only as additive flavour on top, and none are authored in this plan.
- Modifier folding is `final = (base + sum(adds)) * product(mults)`, then `set` overrides, then `flag` union. Order-independent within each class.
- Exactly one element behaviour is live per run, chosen at run start.
- Status effects refresh duration and take the maximum magnitude; they do not stack in count. Poison is the sole exception, stacking to `maxStacks`.

---

### Task 1: Modifier and StatBlock

**Files:** `src/core/Modifier.h`, `src/core/StatBlock.h/.cpp`, `tests/core/test_statblock.cpp`

**Produces:**
```cpp
enum class ModOp { Add, Mult, Set, Flag };
struct Modifier { std::string target; ModOp op = ModOp::Add; float value = 0.0f; };

class StatBlock {
public:
    void setBase(const std::string& path, float v);
    void apply(const Modifier& m);
    void apply(const std::vector<Modifier>& mods);
    float get(const std::string& path, float fallback = 0.0f) const;
    bool  flag(const std::string& name) const;
    bool  has(const std::string& path) const;
};
```

`get` computes on demand — `sets` win outright, otherwise `(base + add) * mult` — so no `finalize()` step exists to forget to call.

- [ ] Write failing tests: add-then-mult ordering, order-independence within a class (applying two adds in either sequence gives the same result), `set` overriding both, flags as a union, unknown path returning the fallback, and multiplying an absent base staying at zero.
- [ ] Implement, run, verify.

---

### Task 2: Skill trees as data

**Files:** `src/core/SkillTree.h`, `src/content/TreeLoader.h/.cpp`, `content/trees/{global,arrow,earth}.toml`, extend `Registry` and `Validate`, `tests/content/test_trees.cpp`

**Produces:** `SkillNode {id,name,desc,branch,cost,requires[],x,y,modifiers[]}` and `SkillTree {id,kind,nodes[]}` with `Kind ∈ {Global,Tower,Element}`. All three trees share one schema; a new element is a new TOML file, never new C++.

**Validation additions (asserted over shipped content):** every `requires` id resolves within its tree; no prerequisite cycles; every modifier `target` is a known stat path; every `branch` is either `trunk` or one of the tree's declared specs; costs positive.

- [ ] Author the three tree files. Arrow: trunk plus `sniper`/`elf`/`hunter`. Earth: trunk plus `poison`/`rock`/`quake`.
- [ ] Write failing tests, implement loader and validation, verify.

---

### Task 3: Loadout and stat resolution

**Files:** `src/core/Loadout.h`, `src/core/Resolve.h/.cpp`, `content/elements/earth.toml`, `tests/core/test_resolve.cpp`

**Produces:**
```cpp
struct Loadout {
    std::string towerId, towerSpec, elementId, elementSpec;
    std::set<std::string> ownedNodes;   // Plan 3 fills this from the save
    bool ownAll = true;                 // Plan 2 owns everything
};
StatBlock resolveStats(const content::Registry& reg, const Loadout& lo);
```

Folding order is exactly the spec's: base tower and element stats, then global tree, arrow trunk, equipped tower branch, earth trunk, equipped element branch.

**The load-bearing test:** nodes in a *non-equipped* branch must contribute nothing. That single property is what makes "you own everything, but only one spec plays" work without special-casing, and it is the first thing to break if folding is done carelessly.

- [ ] Write failing tests including the non-equipped-branch property and a distinctness check that all three tower specs produce different `arrow.damage`/`arrow.fireRate` pairs.
- [ ] Implement, verify.

---

### Task 4: Tower spec traits

**Files:** extend `src/sim/Components.h`, `src/sim/World.cpp`, `src/sim/systems/CombatSystems.cpp`, `tests/sim/test_specs.cpp`

Trait components are attached at tower spawn from `flag` modifiers in the resolved block, so which traits exist is decided in TOML:

- `Execute{threshold, mult}` — Sniper. Bonus damage above a health fraction.
- `RampUp{perSec, maxMult, current, lastTarget}` — Elf. Fire rate climbs while held on one target, resets on switch.
- Hunter needs no new component: it is `projectileCount` and `pierce` adds plus the existing spread geometry.

- [ ] Write failing tests: Elf's effective fire rate rises over sustained fire and resets on target switch; Sniper deals more to a full-health target than a wounded one; Hunter's shot count matches `projectileCount` and pierce carries through multiple enemies.
- [ ] Implement, verify.

---

### Task 5: Status effects

**Files:** `src/sim/systems/StatusSystem.h/.cpp`, extend `Components.h` and `MovementSystem`, `tests/sim/test_status.cpp`

`Poisoned{stacks,dpsPerStack,remaining}`, `ArmorShred{amount,remaining}`, `Slowed{pct,remaining}`, `Petrified{remaining}`. Movement speed becomes `base * (1 - slowPct)`, zero while petrified. `ArmorShred` feeds the existing `computeDamage` parameter that Plan 1 left wired but unused.

- [ ] Write failing tests: poison ticks damage proportional to stacks and expires; stacks cap at `maxStacks`; a second slow takes the maximum rather than compounding; petrify halts movement then releases; shred reduces effective armour and expires.
- [ ] Implement, verify.

---

### Task 6: Element behaviours

**Files:** `src/sim/ElementBehavior.h`, `src/sim/elements/{Poison,Rock,Quake}.h/.cpp`, wire hooks into `CombatSystems`, `tests/sim/test_elements.cpp`

```cpp
struct ElementBehavior {
    virtual void onShoot(World&, entt::entity tower, entt::entity projectile) {}
    virtual void onHit(World&, entt::entity source, entt::entity target, float dealt) {}
    virtual void onKill(World&, entt::entity target) {}
    virtual void onTowerTick(World&, entt::entity tower, float dt) {}
    virtual ~ElementBehavior() = default;
};
std::unique_ptr<ElementBehavior> makeElement(const std::string& spec, const StatBlock&);
```

- **Poison** — `onHit` applies stacks; `onKill` spreads a fraction of stacks to enemies within `spreadRadius`.
- **Rock** — `onHit` applies shred and rolls petrify; adds `flatBonus` damage.
- **Quake** — `onShoot` counts shots and every `everyNShots` marks the projectile; a marked projectile's `onHit` pulses AoE damage and a slow within `radius`.

- [ ] Write failing tests per element against a controlled scenario, then implement and verify.

---

### Task 7: The matrix test

**Files:** `tests/matrix/test_combinations.cpp`, `src/sim/Scenario.h/.cpp`

A headless harness `simulateCombo(towerSpec, elementSpec, Scenario, seconds)` returning damage dealt, kills, and time-to-clear, run against three fixed scenarios: a lone high-armour tank, a dense swarm, and a mixed wave.

**Assertions are relational guardrails, never magic numbers**, so rebalancing does not shatter the suite:

- All nine combinations complete without crashing and deal non-zero damage in every scenario.
- Elf+Poison out-damages Sniper+Poison on the swarm (rate drives stack uptime).
- Sniper+Rock out-damages every other pairing against the lone armoured tank.
- Hunter+anything out-damages Sniper+the-same-element on the swarm.
- No combination exceeds ~2.5x the median across the full scenario set, in either direction — that band is the actual definition of "all nine are worth playing".
- Every combination is deterministic for a fixed seed.

- [ ] Write the harness and the guardrails, run, and record the resulting 3x3 damage table in the execution log.

---

## Definition of done for Plan 2

- All nine combinations are selectable and produce measurably different behaviour.
- No `if (spec == X && element == Y)` branch exists anywhere in the codebase.
- The matrix test passes and its damage table is recorded.
- `td_core` still links no raylib; the full suite is green.

---

## Execution log

### Measured matrix (percent of total health removed, 12s, 2 towers, wave-35-scaled enemies)

```
=== LoneTank ===            === Swarm ===              === Mixed ===
        poison rock  quake          poison rock  quake         poison rock  quake
sniper    91%   91%   85%   sniper    32%   25%   28%   sniper    23%   19%   23%
elf       34%  100%   72%   elf       56%   47%   97%   elf       26%   30%   60%
hunter    43%  100%   51%   hunter    64%   46%   73%   hunter    30%   26%   42%

=== average across all three scenarios ===
        poison  rock  quake
sniper    49%    45%    45%
elf       39%    59%    76%
hunter    46%    57%    55%
```

All nine sit within **1.6x of the median** on the cross-scenario average, which is
the actual definition of "all nine are worth playing".

### The design goal was met

`grep -rn 'sniper\|elf\|hunter' src/` returns **nothing**. Tower specs exist purely
as TOML modifiers and `flag`-granted trait components. Element spec names appear
only at the single factory dispatch in `Earth.cpp` and as a default in
`Loadout.h`. There is no pair-wise branch anywhere, so the nine behaviours are
genuinely emergent from six authored pieces.

### Two real problems the matrix found

**1. Elf+Poison was the worst combo in the game (30/47/21%)**, despite the spec
calling it "the best sustained DoT". Cause: poison capped at 8 stacks, so once
Elf hit the cap its enormous fire rate stopped mattering — the cap, not the rate,
was the ceiling. Fixed in content with no per-pair rule: `dpsPerStack` 3.0 -> 1.8
and `maxStacks` 5+3 -> 10+8, making hit COUNT the gate. Swarm went 47% -> 56%,
and Elf+Poison now decisively beats Sniper+Poison (32%) as intended.

**2. The spec's predicted "Sniper+Rock = the boss-killer" is wrong.** Measured,
Elf+Rock and Hunter+Rock both clear the armoured tank 100% while Sniper+Rock
manages 91%. Armour shred accumulates per hit, so it naturally rewards hit count
over hit size. This is a legitimate emergent outcome and arguably a better one —
Rock became "the armour-stripper, best on fast towers" rather than "the big-hit
finisher". **Spec section 2's table should be updated to match reality**; it has
been left alone pending a decision, and is flagged rather than silently changed.

### Deviations

- The three Earth behaviours live in one `elements/Earth.cpp` rather than three
  files: they share a tuning surface and change together.
- `Phase::Sandbox` was added to `World` — it runs every combat system but no wave
  logic, which is what lets the matrix set up controlled scenarios. It is a
  genuine testing seam, and would also serve a future practice mode.
- `SkillNode::requires` had to become `prereqs`: `requires` is a C++20 keyword.
  The TOML key is still `requires`.
- Element effect damage (poison ticks, quake pulses) deliberately bypasses
  ARMOUR but still respects damage-type RESISTANCE. Routing element damage
  through armour would make Earth useless against exactly the enemies Rock
  exists to counter.
- The first scenario draft was far too easy — all nine cleared 100% and the
  matrix measured nothing. Scenarios now use wave-35-scaled enemies and only two
  towers.
- Three Plan 1 tests hardcoded starting gold and lives; the global tree now
  raises both. They were rewritten to state intent (a loadout owning nothing, or
  assertions relative to actual starting gold) and a new test pins the global
  tree's effect.

### Scope change requested mid-execution: enemy weaknesses

Damage types and per-enemy resistances were built now rather than deferred,
because retrofitting the damage pipeline later is far more expensive than
extending it while already inside it.

- `TowerDef.damageType` (arrow = `piercing`), `ElementDef.damageType`
  (earth = `earth`), `EnemyDef.resist` as a `{type -> multiplier}` map where 1.0
  is neutral, below 1 resistant, above 1 vulnerable. Undeclared types are
  neutral, so an enemy only declares what it cares about.
- `DamageInput.resistMult` applies with crit and before armour, so a resistant
  enemy is still helped by its armour rather than double-dipping.
- Validation rejects a resistance naming an unknown damage type.
- Two enemies demonstrate it: goblin `{earth 0.6, piercing 1.15}`, bee
  `{earth 1.35, piercing 0.85}`.
- **Still to do:** a wider enemy roster built around these weaknesses (the rat
  the user described), and per-spec damage types finer than one type per element.

### Known limitation of the matrix

Scenario enemies are stationary, so the matrix measures damage THROUGHPUT.
Control effects — quake's slow, rock's petrify — are undervalued by it and must
still be judged in play. Quake's measured strength is therefore a floor, not a
ceiling.

---

## Addendum — specs became in-run tower upgrades

Requested mid-execution: specs are not a menu choice. A tower is built up during
the run with gold — build arrow, imbue with earth, specialise the tower,
specialise the element — and the loadout screen was deleted.

**The one-spec-per-run pillar is preserved, and improved.** The run commits on
the first purchase that picks a spec: the first tower to take a tower spec locks
it for the whole run, and the element spec locks independently. The decision now
happens *inside* the run, under pressure, with knowledge of what is attacking —
which is strictly better than guessing at a menu. If per-tower independent specs
are wanted instead, deleting the two `runTowerSpec_` / `runElementSpec_` guards
in `availableTowerSpecs` / `availableElementSpecs` is the whole change.

### What this changed

- `TowerTag` carries `elementId`, `towerSpec`, `elementSpec`. Empty means "not
  bought yet", so a half-built tower is just a loadout with empty fields and
  needs no special handling.
- `core::Loadout` went from a run-wide choice to a **per-tower descriptor**.
  `World::rebuildTower` recomputes stats, traits and element behaviour from a
  tower's purchases, so a tower's state is always a pure function of what has
  been bought for it. `resolveStats` already handled empty spec fields
  correctly — folding only the trunk — so it needed no change at all.
- Element behaviours are cached per element spec and shared between towers via
  an `ElementRef` component, rather than one behaviour owned by the World.
- `LastHitBy` was added so on-kill effects (poison spread) fire the **killer's**
  element rather than a world-wide one.
- The matrix harness now builds its towers through the real purchase path, so it
  measures configurations a player can actually assemble. **The measured matrix
  came out byte-identical to the pre-rework numbers**, which is good evidence the
  refactor preserved behaviour.
- Costs are authored where the thing being bought lives: `specCost` on the tower,
  `attachCost` and `specCost` on the element.

### Verification added

`tests/sim/test_towerbuild.cpp` — 12 cases covering purchase order (an element
must exist before it can be specialised), double-purchase rejection, gold
accounting and proportional refunds, the run lock in both directions, stat and
trait changes on specialisation, shared behaviour instances, and the case that
matters most: **a failed purchase must not commit the run.**

### Spec updated

Sections 3 and 12 of the design spec were rewritten to describe the in-run
upgrade model and the removal of the Loadout screen.
