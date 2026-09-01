# Roguelike Tower Defence — *Wardstone*

A pixel-art roguelike tower defence in C++20. Towers specialise, elements
specialise, and the two **combine** — 270 distinct builds from 33 authored
pieces. Every map resists a different element, so the build that cleared the
last map is the wrong build for the next one.

![Gameplay](docs/screenshots/01-gameplay.png)

---

## The idea

Most tower defence games give you a tower list. This one gives you a **matrix**.

- Build a tower, level it to max, then **specialise** it — an arrow tower becomes
  a sniper, an elf, or a hunter.
- Separately, **imbue** it with an element and specialise that too — earth
  becomes poison, rock, or earthquake.
- Only **one of each specialisation may exist on the map at a time**, so a run is
  a sequence of commitments, not a shopping list.
- **Everything above is unlocked by the skill tree.** Levelling a tower past 1,
  imbuing an element and specialising at all are all bought permanently, so a
  first run fields plain level-1 towers and the meta progression is what turns
  them into a real board.

The combinations are the game. A tower spec changes *numbers and geometry*
(damage size, fire rate, how many projectiles); an element spec changes *events*
(what happens on shoot, on hit, on kill, on tick). Because element potency scales
with hit **magnitude**, hit **count**, or **time**, the firing profiles
differentiate the elements on their own — nine behaviours emerge from six
authored pieces, and 270 from 33, with **no per-pair code anywhere**. A test
suite enforces that: if a combination ever needed special-casing, a guardrail
fails.

Progress is permanent. A lost run pays out shards, and shards buy nodes in twelve
skill trees that persist across runs.

## Content

| | |
|---|---|
| Towers | 5 — arrow, cannon, arcane spire, ballista, brazier |
| Tower specialisations | 15 (3 each) |
| Elements | 6 — earth, fire, water, wind, shadow, light |
| Element specialisations | 18 (3 each) |
| **Combinations** | **270** |
| Maps | 5, each 50 waves, with bosses at 25, 40 and 50 |
| Bosses | 15 |
| Enemies | 45 definitions over 14 creature sets, 6 of them flying, with per-damage-type resistance tables |
| Skill trees | 12 trees, 150 nodes, 5057 shards — the global tree alone is 23 |
| Damage types | 11 |
| Audio | 2 CC0 music loops + 13 CC0 effects, committed; procedural synth as fallback |

Each map's roster resists a different element and is vulnerable to another, which
is what makes five maps replayable rather than one map five times.

## Screenshots

| Tower inspection | Skill trees |
|---|---|
| ![Tower stats](docs/screenshots/02-tower-stats.png) | ![Skill tree](docs/screenshots/03-skill-tree.png) |

| Map select | Boss fight |
|---|---|
| ![Map select](docs/screenshots/04-map-select.png) | ![Boss](docs/screenshots/05-boss.png) |

![Pause](docs/screenshots/06-pause.png)

## Architecture

The one rule everything else follows: **`td_core` must never link raylib.**

```
src/core/     pure logic — stats, damage, RNG, saves, skill trees
src/content/  TOML loading and validation
src/sim/      the simulation — ECS, systems, element behaviours
              ^^^ none of the above may include raylib. Enforced at CMake configure time.
src/render/   raylib rendering, sprite atlas, shaders
src/ui/       screens, HUD, radial menu
src/app/      screen state machine and the fixed-timestep loop
```

That boundary is why the whole game is **headlessly testable**, and it pays for
itself constantly:

- A **simulated player** (`sim::AutoPlayer`) plays complete runs with no window,
  so difficulty is *measured* rather than guessed. It found a wave-4 death spiral
  and a meta-progression that ended in one run — neither visible by inspection.
- A **combination matrix** (`sim::Scenario`) runs all 270 pairings against fixed
  scenarios and asserts relational guardrails, never magic numbers. It caught a
  spec measuring 79% against a row median of 23%.
- The same harness caught the worst bug in the project so far. `levelUnlocked()`
  checked `owns("global.unlock.level2")`, but that string is a **flag granted by**
  the node `global.level2` — and owned-node sets hold node ids, never granted
  flags. So tower levels 2 and 3 were unreachable for every real player, and
  *every existing guardrail passed* because they all used the test-only `ownAll`
  shortcut, which bypasses the gate. It surfaced only when the meta-loop report
  simulated buying nodes: a profile owning **all 150 nodes** stalled at wave 42
  of 50 with every tower stuck at level 1, while `ownAll` cleared all 50. The
  lesson is now a test (`tests/sim/test_unlock_gates.cpp`): *a gate verified only
  through `ownAll` is not verified.* The gate is also resolved from the tree
  rather than by node id, so a rename cannot silently seal it again.
- Balance is driven by `tools/balance.py`: authored base stats plus named
  multiplier profiles, so tuning is idempotent, reversible and auditable.
- A **reachability guardrail** asserts a fully-upgraded profile gets at least 76%
  of the way through *every* map. The game shipped mathematically unwinnable for
  three passes — the HP curve was `1.055 ^ (wave^1.18)`, so health multiplied 7×
  between wave 34 and wave 50 while the player, already fully built, gained
  nothing. This test exists so that cannot recur silently.
- Per-map difficulty is **normalised against path length** in `tools/make_map.py`,
  which solves each map's HP curve from a declared target, its actual tile count
  and its wave-50 enemy count. Damage dealt is proportional to how long enemies
  are under fire, so changing a route would otherwise silently change how hard a
  map is. Before normalising, two maps were ~2× harder than the rest.

Other load-bearing decisions:

- **Deterministic.** One seeded `std::mt19937_64` per run, injected everywhere;
  `rand()` is prohibited; the RNG state is serialised into saves, so resuming a
  run continues the same random sequence.
- **Fixed 1/60s timestep** with an accumulator, and render interpolation.
- **Music is generated, not licensed.** `core::Music` composes two minor-key
  loops from the same raylib-free synthesiser the sound effects use, wraps the PCM
  in a WAV header in memory, and streams it. Nothing on disk, nothing to license.
- **Content is data.** Towers, elements, enemies, maps, wave recipes and skill
  trees are all TOML. A new element is one TOML file, one tree, one behaviour
  file, and one line in a dispatch table.
- **Integer-scaled pixel art.** Everything renders to one virtual 1408×800
  canvas, blitted at an integer scale — fractional scaling is what makes pixel
  art shimmer, so it is never allowed.

## Building

Requires a C++20 compiler, CMake ≥ 3.24 and Ninja. Dependencies are fetched
automatically by CPM: raylib, EnTT, toml++, nlohmann/json, Catch2.

```sh
cmake -B build -G Ninja
cmake --build build
./build/td_app
```

Tests:

```sh
ctest --test-dir build          # 210 tests
```

The suite takes several minutes, dominated by the 270-combination matrix and the
five full-map autoplay runs. Two opt-in reports are worth running by hand:

```sh
./build/tests/td_tests "print the combination matrix" -c "[.report]"
./build/tests/td_tests "report how many runs it takes to clear map 1" -c "[.report]"
```

The second reports **two** purchase policies, because the answer depends far more
on how shards are spent than on how many are earned:

| policy | what it models | map 1 falls on |
|---|---|---|
| `greedy` | cheapest prereq-met node anywhere | not within 24 runs |
| `planned` | pushes one line and *saves* for it | **run 21** |

Those numbers moved deliberately. Map 1 fell on run 11 before the hardcore pass;
shard income was then cut by ~60% (`kShardsPerWave` 2 → 1, map-clear 40 → 20,
per-kill ×0.4) against node prices cut 38%, and it moved to run 21. **"Hardcore,
limited resources" and "map 1 in 8–10 losses" are the same dial pulled opposite
ways** — 21 is what hardcore costs, and the dial is `kShardsPerWave` in
`src/sim/World.h` plus `nodeCost`/`shard` in `tools/balance.py`.

Greedy is kept as a floor and as a warning: the six element trunks are the
cheapest nodes in the game, so it buys all six first, and under a gold deficit
that makes it measurably *worse* — an unlocked element invites spending scarce
gold on imbuing instead of building, and measured waves fall from 14 to 10 over
the opening runs.

### Useful dev flags

```sh
./build/td_app --autostart              # skip menus, straight into a run
./build/td_app --map frostmere          # force a map
./build/td_app --wave 24                # jump to a wave (boss fights)
./build/td_app --hub --tab 3            # a specific skill tree
./build/td_app --maps                   # map select
./build/td_app --pause                  # open the settings overlay
./build/td_app --menu 8 2 --menupage targeting   # a tower's targeting ring
./build/td_app --cluster 9              # a dense block of towers
./build/td_app --shot out.png --after 8 # render N seconds then screenshot
```

## Assets

**Audio is in the repository. Art is not.** Both follow from the licences: the
audio is CC0, which permits redistribution; the sprite packs permit use and
modification in a shipped game but forbid redistributing the source art.

### Audio — committed

Two CC0 music loops from OpenGameArt and 13 CC0 effects from Kenney. Sound
effects are looked up by filename (`assets/audio/sfx/<cue>.ogg`), so any cue can
be replaced by dropping in a different `.ogg` — no rebuild. The procedural
synthesiser remains as a fallback for any missing file.

### Art — not committed

Committing the sprites would breach their licences, so `assets/sprites/*.png` is
gitignored and the importers are committed instead:

```sh
python3 tools/import_tinyswords.py ~/Downloads   # terrain, towers, creatures, UI kit
python3 tools/import_magic.py      ~/Downloads   # element icons, projectiles, overlays
```

Every sprite id has a procedurally generated fallback (`content/art/sprites.toml`),
so **a fresh clone builds and runs** without any downloads — it just looks like
the prototype until you run the importers.

See [`docs/ASSET-POLICY.md`](docs/ASSET-POLICY.md) for which packs were cleared,
which were rejected and why. Two were rejected after checking the licence at
source contradicted what search results claimed.

## Documentation

- [`docs/ART.md`](docs/ART.md) — the art pipeline, and the mistakes worth not
  repeating: measured frame pitches, baked contact shadows, nine-slice tiling,
  why light pools must saturate.
- [`docs/ASSET-POLICY.md`](docs/ASSET-POLICY.md) — licence clearances.
- [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) — dependency licences,
  generated by `tools/gen_third_party_licenses.sh`.
- `docs/superpowers/specs/` — the design spec.
- `docs/superpowers/plans/` — implementation plans, each with an execution log
  recording what actually happened and where the plan was wrong.

## Status

Playable end to end: profile select → skill trees → map select → 50-wave run with
bosses → results → spend shards → repeat.

The whole campaign is reachable: the difficulty curve used to be
super-exponential and made maps 2–5 unreachable content. That is fixed and pinned
by a test — a fully-upgraded profile now gets 76%+ of the way through every map
using only arrow towers, which is a lower bound on a real player with five tower
types.

Balance is now measured rather than asserted: a **fresh** profile builds 15–17
towers and dies around wave 14, a **fully upgraded** one clears all 50, and gold
— not the simulator's tower cap — is what binds. That last point took three
attempts to get right: cutting income further did nothing because the autoplayer
was hitting a 34-tower ceiling on *every* profile, so the wallet was never the
constraint. Raising build cost (`deficit` profile, cost x1.85) binds spending
directly, where cutting bounty risks a death spiral — fewer towers means fewer
kills means less gold.

Every tower's **targeting** is the player's to set — first, last, strongest,
weakest or closest — which is the decision a wave is actually made of. **P is a
tactical pause**: the simulation stops but the board stays live, so you can
build, upgrade, sell and re-target while it is frozen, and think rather than
react. (ESC is the settings menu, which does block.) Speed runs at 1x / 2x / 4x /
8x, and the next wave can be **called on top of a running one** for gold proportional to what is still unresolved — the only income
source the player controls, which is what makes the gold deficit a decision
rather than a wait.

Known gaps, honestly:

- **The tutorial is five one-shot hints, not a tutorial.** They cover building,
  targeting, specialising, calling early and pausing, which is the shape of the
  game — but nothing walks a new player through a first run.
- The simulated player only builds arrow towers, and its element choice is
  resistance-aware but not synergy-aware, so its difficulty readings are a
  conservative lower bound rather than a picture of good play.
- Element overlays are per element rather than per specialisation (earth's three
  are bespoke; the other five share one per element).
- **Tower art is fixed but capped by the pack.** All 20 tower sprites now use a
  tower silhouette; 16 of them used to be houses, barracks, archery halls and
  chapels, because Tiny Swords is a village builder. Tiny Swords contains only
  *three* single-tile tower silhouettes, so the fourth is composited (a second
  crenellated drum stacked on the keep). Five genuinely distinct tower families
  with per-upgrade-level art needs a purpose-built tower defence pack — see
  `docs/ASSET-POLICY.md` for the two verified options and `docs/ART.md` for why
  the chapel and the castle were rejected by looking at them.
