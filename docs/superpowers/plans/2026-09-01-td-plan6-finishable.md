# Finishable Implementation Plan

**Goal:** Make the game completable, and make it feel finished rather than
prototyped. Two things, in that order of importance.

## Diagnosis

### 1. The game is mathematically unwinnable

I flagged this twice and never fixed it, so it goes first. The HP curve is
`hpPerWave ^ (wave ^ hpCurveExp)` = `1.055 ^ (wave ^ 1.18)` — an exponential of a
power, i.e. **super-exponential**:

| wave | 10 | 20 | 25 | 34 | 40 | 45 | 50 |
|---|---|---|---|---|---|---|---|
| HP multiplier | 2.0 | 5.6 | 9.7 | 27.5 | 56.7 | 105 | **197** |

Between wave 34 — where a profile owning all 126 skill nodes dies — and wave 50,
enemy health multiplies by **7.2×**. The player's damage does not grow 7× in that
span: by wave 34 every buildable tile is filled with a maxed, specialised tower,
so their DPS is flat and only gold income (linear-ish) still moves.

The curve outruns any possible player growth in the last third. That is not
difficulty, it is a wall. Consequences: map 1 is never cleared, so **maps 2–5 and
8 of the 10 bosses are unreachable content**, and 4 of 5 maps have never been
played by anyone.

Research backs the shape: the ideal tower-defence curve is *exponential*, and a
wave generator must not produce unbeatable waves. Ours is exponential-of-a-power.

**The measurement target**, so this is falsifiable rather than a vibe: a profile
owning every node should CLEAR wave 50 on map 1; a fresh profile should reach
roughly wave 15–20. The autoplayer is weaker than a human, so "autoplayer clears
it" means "a competent human clears it comfortably", which is the right place for
map 1 of 5.

### 2. It has no music

Zero. A game with no music reads as a prototype no matter how good the board
looks, and this is the most-cited "feels unfinished" signal there is. The project
already has a raylib-free PCM synthesiser used for every sound effect, so music
can be **generated**, with no asset licence attached — consistent with the rest of
the audio and with the goal of selling this.

### 3. No pause menu, no volume control

`P` toggles a paused flag and draws the word PAUSED. There is no way to change
volume, and no way out of a run except a small quit icon. Both are basic expected
UX.

### 4. Hits do not say what kind of damage they were

Cut from the previous pass. The damage-type palette now exists, so tinting the
floating numbers is small and closes the loop the resistance UI opened: you can
see an enemy resists earth, and you can see your earth tower doing nothing.

## Global Constraints

- `td_core` never links raylib. Music generation is PCM maths in `core/`;
  playback is raylib in `audio/`.
- No new assets. Music is synthesised.
- Balance changes go through `tools/balance.py` where they are stat multipliers;
  curve changes are content (`content/maps/*.toml`) and must be measured.
- 179 tests stay green, and the curve work adds tests that pin the new shape.

---

### Task 1: Make the game finishable

**Files:** `content/maps/*.toml`, `tools/make_map.py`, `tests/matrix/test_balance.cpp`

- [ ] Retune the curve so wave-50 HP lands near what a fully-built board can
      actually kill, keeping a bend so the opening stays gentle. Target roughly
      35–50× rather than 197×.
- [ ] Keep the existing shape guardrail (`w50/w15 > w15/w5`) true — a plain
      exponential would flatten it, and the gentle opening is what stops new
      players quitting at wave 4.
- [ ] Iterate against the autoplayer until a fully-upgraded profile clears map 1
      and a fresh one reaches the mid-teens. Measure, do not guess.
- [ ] Add a test that a fully-upgraded profile CLEARS map 1, so this can never
      silently regress into unwinnable again. This is the guardrail whose absence
      let the problem survive three passes.
- [ ] Apply the same retune to all five maps, preserving their relative
      difficulty ordering.

### Task 2: Procedural music

**Files:** `src/core/Music.h/.cpp` (new), `src/core/Synth.h/.cpp` (tone
generator), `src/audio/Sfx.h/.cpp` or a new `src/audio/Jukebox.h/.cpp`

- [ ] Add a sustained-tone generator to the synth (sine/triangle for pads; the
      existing square is too harsh to hold a chord) plus an `overlay` so notes
      can be sequenced at sample offsets.
- [ ] Generate two loops in a minor key: a slow **hub** piece (pad and arpeggio)
      and a **battle** piece (adds bass and percussion).
- [ ] Wrap the PCM in a WAV header in memory and hand it to
      `LoadMusicStreamFromMemory`, which loops natively — no file on disk, no
      asset to license.
- [ ] Cross-fade when the screen changes, so hub → run is not a hard cut.
- [ ] Tests on the generator: correct length, no clipping, no DC offset, loops
      seamlessly (first and last samples continuous), and deterministic.

### Task 3: Pause menu and volume

**Files:** `src/ui/Screens.h/.cpp`, `src/app/Game.h/.cpp`

- [ ] A real pause overlay on a painted panel: resume, music volume, sfx volume,
      and quit to the hub.
- [ ] Volume as click-and-drag sliders, applied live.
- [ ] Volumes persist in the save's meta, so they survive a restart.

### Task 4: Damage-type feedback

**Files:** `src/sim/VisualEvent.h`, `src/render/Effects.h/.cpp`,
`src/sim/systems/CombatSystems.cpp`

- [ ] Carry the damage type on the hit event and tint the floating number with
      `palette::damageTypeColor`.
- [ ] Show a resisted hit as visibly weaker and an amplified one as brighter, so
      "my earth tower does nothing here" is learnable in the moment.

---

## Out of scope

Tutorial, flying enemies, new content, new mechanics, and the monster pack
import. Named so this stays focused on completability and finish.

## Sources

- [Dynamic Difficulty Adjustment in Tower Defence](https://www.sciencedirect.com/science/article/pii/S187705091502092X/pdf)
- [Optimizing Tower Defense for FOCUS and THINKING — Defender's Quest](https://www.fortressofdoors.com/optimizing-tower-defense-for-focus-and-thinking-defenders-quest/)
- [A NEAT Approach to Wave Generation in Tower Defense Games](https://www.open-access.bcu.ac.uk/13568/1/A_NEAT_Approach_to_Wave_Generation_in_Tower_Defense_Games___IMET.pdf)

---

## Execution log

### Task 1 — the game is finishable. Done, and it took five measured iterations.

The curve went from `1.055 ^ (wave^1.18)` to `1.0467 ^ (wave^1.10)`, taking the
wave-50 HP multiplier from **197× to 27×**. Fully upgraded went from wave 34 to
46–49 of 50 on map 1; fresh from 17 to 22.

Then the new "endgame is reachable on every map" guardrail failed on
**blightmarsh** and **obsidian-gate**, and finding out why produced the two most
useful facts of this pass:

**1. The measuring instrument was wrong.** The autoplayer hardcoded `earth`.
Blightmarsh resists earth at 0.5 *by design* — so the harness was measuring a
player deliberately bringing the counter-productive element and reporting the map
as too hard. It now reads the roster and picks the element it is least resistant
to, which is what a player does with the dossier. The picks it makes are the
design working:

| map | resists | autoplayer now brings |
|---|---|---|
| ashen-wastes | fire | water |
| frostmere | frost | fire |
| blightmarsh | earth | wind |
| obsidian-gate | everything | light |

Obsidian answering to Light is exactly what Light's resistance-ignoring Sear was
designed for.

**2. Path length is a first-order difficulty multiplier, and I had ignored it.**
Total damage a board can deal is proportional to how long enemies are under fire.
My five maps vary from 38 to 68 path tiles — a 1.8× spread — and I had stacked
*higher* HP and enemy counts onto the **shortest** paths. Decomposed:

| map | path tiles | w50 HP | count@50 | aggregate |
|---|---|---|---|---|
| ashen-wastes | 60 | 29.6 | 29.6 | 0.93× |
| frostmere | 68 | 32.3 | 31.1 | 0.95× |
| greenfields | 48 | 27.1 | 27.6 | 1.00× |
| blightmarsh | **38** | 33.8 | 33.5 | **1.91×** |
| obsidian-gate | **44** | 36.2 | 35.5 | **1.87×** |

Two maps were ~2× harder than the rest and nobody had noticed, because nobody
could reach them. Difficulty is now normalised against path length in
`tools/make_map.py`, giving a deliberate 1.00 / 1.06 / 1.12 / 1.18 / 1.25
progression instead of a cliff.

Obsidian-gate needed one further correction: it carries two walls the
HP/count/path model cannot see — ~0.9 resistance to *every* damage type (≈1.11×
effective HP) and flat armour on every creature. Its HP target is deliberately
set **below** the others so its effective difficulty lands at the intended 1.25×.

Two of my own tests were wrong and had to be fixed:

- `the difficulty curve is gentle early and steep late` asserted `w50 > 50`. That
  magic number **encoded the unwinnable curve and was defending it.** It now
  asserts the shape plus an upper bound, which guards the failure that actually
  happened.
- The reachability threshold is 76% of a map's waves, argued rather than picked:
  the broken state was 68% on the *easiest* map, so this catches drift back
  toward the wall, while not demanding a clear from an arrow-only bot.

**Known limitation, stated plainly:** the autoplayer's element choice is
resistance-aware but not synergy-aware. On greenfields it now picks fire, whose
burn scales with hit magnitude and is therefore a poor fit for the fast elf tower
it builds — so greenfields reads 38 rather than the 46 it managed with earth. The
readings are a conservative lower bound, which is the right direction for a
guardrail.

### Task 2 — music. Done.

`core::Music` composes two 16-second loops from the same raylib-free synthesiser
the sound effects use: a i–VI–III–VII minor progression, pad triad plus arpeggio
for the hub, adding a saw bass and kick/hat for battle. The PCM is wrapped in a
WAV header in memory and handed to `LoadMusicStreamFromMemory`, so there is no
file on disk and no asset to license.

`audio::Jukebox` keeps **both** streams playing and cross-fades their volumes.
Stopping and restarting on every screen change pops, and restarting loses the
loop position, so a player bouncing between hub and run would hear the same four
bars forever.

Seven tests: bar-aligned length, no clipping when four voices layer, a silent
loop seam (a loud sample at either end is an audible click every 16 seconds), no
DC offset, battle denser than hub, determinism, and a valid RIFF header.

Two synth additions were needed: `tone` (the existing `square` sweeps pitch and
is far too harsh to hold a chord for four bars) and `overlay` for sequencing
notes at sample offsets.

### Task 3 — pause menu and volume. Done.

A painted overlay with music and sound sliders, resume, and quit to the hub.
Volumes live in the profile, so they survive a restart. Sliders respond to mouse
*held* rather than pressed, so they drag.

Two bugs found doing it:

- **`Game::active()` was undefined behaviour.** With no slot selected it did
  `slots_[static_cast<size_t>(-1)]`, i.e. `slots_[SIZE_MAX]`. Reachable from
  every dev capture path. It now returns a detached scratch slot.
- The overlay let clicks through: pressing RESUME also placed a tower on the tile
  underneath. Pause now returns before `handleBuildInput`.

### Task 4 — damage-type feedback. Done.

Floating numbers are tinted by damage type via the shared palette, with a small
`^` on a vulnerable hit and `v` on a resisted one, plus dimming for resisted. The
event's existing free-form `tag` carries the type and direction, so no new field
was needed. This closes the loop the resistance dossier opened: you can see that
an enemy resists earth, and now you can see your earth tower doing nothing about
it.

---

## Addendum: descriptions, icons, real audio, gating, gold, longer paths

Six things asked for mid-pass. In the order they were done, with what each one
turned up.

### The bug first: ten invisible bosses

The wave-25 boss did not render. Cause: I added an `EnemyDef::sprite`
indirection for the per-map roster variants and **never used it for bosses**, so
the renderer asked for `boss_ogre_warlord_0.png`, which does not exist, and drew
nothing. The boss health bar showed, which is why it looked like a bar with no
boss.

Every boss now names an existing creature sprite set with a tint, and draws at
`spriteScale = 2`. Whole-number magnification only — a boss at 1.6x would
resample the pixel art. The shadow inset and health-bar width scale with it, or a
2x body gets a 1x bar.

### Missing icons

Enumerated every id the game builds at runtime rather than eyeballing it: 15 of
18 element-spec icons were missing, and 15 of 18 overlays. Only earth's three
existed.

Filled systematically: 3 shapes (orb / flame / dart) x 6 element hues = exactly
18. Shape says which spec within the element, hue says which element, so a spec
is identifiable in the ring and in its tree node with no per-spec artwork.

### Descriptions

The real problem was not missing text, it was **identical** text: every one of
the 15 tower specialisations showed "Only one of each spec on the map at a time."
and every one of the 18 element specs showed the element equivalent. Choosing
between sniper, elf and hunter told the player nothing about any of them.

The text already existed — each spec's `<tree>.<spec>.core` node carries a real
description. The menu now reads it. `desc` was added to `TowerDef` and
`ElementDef` so tower and element copy is content rather than a hardcoded table
in the UI layer, and Remove now states the actual refund in gold instead of
"refunds part of everything invested".

23 node descriptions were rewritten where they named a mechanic without
explaining it: "+1 pierce" is a number a new player cannot act on, so it now
reads "+1 pierce: each arrow passes through one more enemy".

Five tests pin all of it, including one asserting the descriptions within a tree
are **distinct** — the specific regression above.

### Real audio, replacing the synthesised set

All CC0, so unlike the sprites it is committed. Two OpenGameArt tracks for music,
13 Kenney effects for cues, with the synthesiser kept as the fallback so a clone
missing the files still has sound.

Licence verified at source. Worth recording: the Determined Pursuit page has a
heading reading "Attribution Notice", which greps as though it demands credit.
The full sentence is "This track is in the public domain as of January 2017. No
attribution necessary." Reading it rather than the grep is the difference between
shipping legally and not.

**I cannot listen to audio**, so the cue mapping is a considered guess. It is
keyed by filename so any cue can be swapped by dropping in a different `.ogg` —
no rebuild.

### Gating: levels, elements and specialisations now cost tree nodes

- Tower level 2 and 3 are unlocked by two new global nodes. `upgradeCost` returns
  -1 when the level is not owned, so every existing caller and the whole UI
  respect it through one gate.
- A tower specialisation requires its `<tower>.<spec>.core` node.
- An element specialisation requires its `<element>.<spec>.core` node.
- Imbuing an element at all requires owning any node in that element's tree.

Locked elements still appear in the ring, greyed, with the reason attached —
seeing what the tree would buy you is the point of a meta progression.

### Longer paths, and the difficulty maths that had to follow

Every route roughly doubled: 92 to 106 tiles, from 38 to 68. Each keeps a
distinct shape (serpentines of differing pitch, and a 7-tooth comb for
obsidian-gate) so five maps do not become one map five times. greenfields is now
generated by `tools/make_map.py` like the rest, so all five are verified against
self-crossing, bounds and edge start/end.

Path length is normalised **inside the generator** now: it solves `hpPerWave` from
a declared target difficulty, the actual tile count and the wave-50 enemy count.
Changing a route can no longer silently change how hard a map is.

### Gold: the `scarce` profile

Start gold x0.62 and bounty x0.7 — far tighter than the `tight` profile that
shipped last pass. That squeeze was survivable only because the paths doubled:
the same cut on the old short routes produced the death spiral measured earlier
(23 towers down to 6).

### What the harness taught me, again

With levelling gated, a fresh profile can never upgrade — so the autoplayer just
kept **buying towers**, up to `spots.size()`, which is every buildable tile:
around 140 on a 22x11 map. Targeting is O(towers x enemies) per tick, so a single
test went from seconds to over two minutes while measuring a board no human would
ever build. Capped at 34 towers: the harness is now both far faster and a better
model of real play.

**Measured with everything applied together:**

| profile | waves | towers | shards |
|---|---|---|---|
| fresh (owns nothing, fully gated) | 22 | 34 | 177 |
| fully upgraded | 45 / 50 | 34 | 589 |

All five maps still clear the reachability guardrail.
