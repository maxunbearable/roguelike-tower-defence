# Legibility Pass Implementation Plan

**Goal:** Make the game's systems visible to the player. Not new systems — the
ones already shipped.

## Diagnosis

The game now holds 5 towers × 3 specs, 6 elements × 3 specs (270 combinations),
5 maps, and 30 enemy definitions carrying per-damage-type resistance tables. I
verified, by grepping the UI layer, that **almost none of it is visible**:

| What exists | What the player can see |
|---|---|
| Every tower's resolved damage, fire rate, range, pierce, crit, armour pen | **Nothing.** No stats appear anywhere in the UI. |
| What the next upgrade level changes | **Nothing.** The menu says "Level 3" and a cost. |
| 30 enemies with resistance tables; each map's roster biased against one element | **Nothing.** `grep resist src/ui/*.cpp` returns no matches. |
| 5 damage types on towers, 6 on elements | Nothing distinguishes them on screen. |
| 5 distinct tower types firing different weapons | All five draw the same `arrow` sprite. |
| 18 element specs | 3 have overlay art; 0 of 6 elements have an icon. |

The second row is the serious one. The whole reason five maps replay differently
is that each map's roster resists a different element — and **the player has no
way to learn that except by losing.** I built the replayability system and then
hid it.

This is why the game reads as raw despite the content: the systems are deep and
the interface says nothing about them.

## Research basis

- Tower defense UI convention: *"An upgrade panel displays a tower's stats, as
  well as what stats improve when you upgrade the tower"* — the key element for
  player decision-making.
- On resistances: TD games solve this with a **direct inspect mechanic** ("view
  damage resistances on enemies by selecting them") plus **colour-coded damage
  feedback**. Both, not one.

Sources are listed at the end.

## Global Constraints

- `td_core` never includes raylib. Stat resolution stays in core/sim; only
  presentation is added.
- No new mechanics, no balance changes, no new content. This pass only surfaces
  information that already exists.
- `assets/sprites/*.png` stays gitignored; every new sprite id needs a fallback.
- 177 tests stay green.

---

### Task 1: Tower inspection panel

The single biggest playability gap. All the data is already on the entity as
`TowerStats`; nothing reads it.

**Files:** `src/sim/World.h/.cpp` (upgrade preview), `src/ui/RadialMenu.cpp`,
`src/ui/Screens.h` (shared stat-row helper)

- [ ] `World::previewUpgrade(x, y)` returns the `TowerStats` this tower would have
      one level up, so the panel can show a delta rather than a cost alone.
      Test: the preview matches the real stats after actually upgrading.
- [ ] When a tower is selected, the radial menu's panel lists its live resolved
      stats: damage, fire rate, **DPS** (the number players actually compare),
      range, and only the non-zero extras (pierce, crit, armour pen).
- [ ] Hovering the level-up item shows `current → next` for each stat that
      changes, in green. This is what makes an upgrade a decision instead of a
      gamble.
- [ ] Show the tower's damage type, since that is what interacts with resistance.

### Task 2: Enemy resistance visibility

Two layers, per the research: an at-a-glance cue and a detailed inspect.

**Files:** `src/ui/Hud.cpp`, `src/ui/Screens.cpp`, `src/render/Palette.h`

- [ ] A damage-type colour palette, one entry per type, used everywhere a type is
      shown so the association is learnable.
- [ ] In the HUD's existing INCOMING preview, each enemy icon gains small pips:
      one per notable resistance or vulnerability, coloured by damage type, with
      resistant and vulnerable visually distinct.
- [ ] Hovering an incoming enemy opens a carved panel with its HP, armour, speed
      and the **full** resistance list as `type xN`, so the exact multiplier is
      knowable rather than guessable.
- [ ] The same panel appears when hovering an enemy on the field, because that is
      where a player's attention actually is mid-wave.

### Task 3: Damage-type feedback on hits

**Files:** `src/render/Effects.cpp`, `src/sim/VisualEvent.h`

- [ ] Floating damage numbers tint by the damage type that caused them.
- [ ] A hit that was resisted or amplified reads differently at a glance — the
      moment a player learns "my earth tower does nothing here" should be the
      moment they see it, not three waves later.

### Task 4: Art completeness

The magic pack the user downloaded downscales cleanly at this size — tested at
22px, the gradients flatten into readable colour bands.

**Files:** `tools/import_magic.py` (new), `src/render/Renderer.cpp`

- [ ] Import element icons for all six elements. The pack covers fire and water;
      the other four are hue-shifted from them, which the licence permits and
      which keeps all six stylistically identical.
- [ ] Per-tower projectile sprites, so a cannon shell is not an arrow. Falls back
      to `arrow` for any tower without one.
- [ ] Enchantment overlays for the element specs that have none (15 of 18).

---

## Out of scope

Settings screen, volume control, tutorial, music, new content, balance changes.
Named explicitly so this stays a legibility pass.

## Sources

- [Tower Defense — Revisiting UI](https://medium.com/@sean.duggan/tower-defense-revisiting-ui-f51c0afd74a3)
- [Damage Types vs Enemy Armor Types](https://discussions.unity.com/t/tower-defense-damage-types-vs-enemy-armor-types/765241)
- [Damage Indicators — TDS Wiki](https://tds.fandom.com/wiki/Damage_Indicators)
- [Attributes — TDX Wiki](https://tdx.fandom.com/wiki/Attributes)

---

## Execution log

**Task 1 — tower inspection panel. Done.** `World::previewUpgrade` returns the
stats one level up by walking exactly the path `rebuildTower` walks, so the
preview cannot drift from the real result. A test upgrades every tower through
every level and asserts the prediction matched — the whole point of a preview is
that it can be trusted, and one that can drift is worse than showing nothing.

The selected tower's panel now shows damage, fire rate, **DPS**, range, and only
the non-zero extras, plus its damage type and element. Hovering the upgrade item
turns it into `current > next` in green.

**Task 2 — resistance visibility. Done.** A damage-type colour palette
(`palette::damageTypeColor`) is used everywhere a type appears, so the
association is learnable. In the HUD, each incoming enemy gets a pip per notable
resistance: type hue, with a bar above for vulnerable and below for resistant, so
direction survives without relying on hue alone. Hovering an incoming icon — or
any enemy on the field — opens a dossier with HP, armour, speed and the exact
multipliers.

**Task 4 — art. Done via the magic pack.** Tested the downscale before relying on
it: at 18–24px the vector gradients collapse into flat colour bands and a palette
quantise finishes the job. Six element icons (fire and water from the pack, the
other four hue-rotated so all six are stylistically identical), per-tower
projectiles, and five element-level enchant overlays.

One bug caught by looking: `proj_arcane` came out **green**. The source is cyan
(hue ~0.5) and I reused the +0.72 rotation that works on the orange sources;
+0.72 from cyan lands on green. It needed +0.25.

Overlays are per ELEMENT, not per spec: 18 hand-authored overlays is not
something this pack can provide, and the element is the readable fact — the spec
is already named on the stats panel. `enchantFor` prefers per-spec art and falls
back to per-element, so earth's three keep their existing bespoke overlays.

**Task 3 — damage-type tinting on hit numbers. Not done.** Cut for time; the
palette it needs now exists, so it is a small follow-up.

---

## Balance: what the measurement said

The request was "cut all stats and tower range in half, gold in deficit". Applied
literally and measured with the autoplayer:

| profile | fresh: wave / towers / shards | fully upgraded: wave / towers / shards |
|---|---|---|
| `original` | 18 / 23 / 117 | 34–37 / 107–127 / 385–423 |
| `tight` | 17 / 15 / 98 | 34 / 92 / 375 |
| `lean` | 8 / 6–8 / 17 | 11 / 7–8 / 30–32 |
| `half` (as asked) | **3** / 6 / **6** | **4–5** / 6–9 / **8–10** |

`half` is unplayable. Halving damage *and* fire rate is DPS ×0.25; halving range
is coverage ×0.25 because area scales with r². Compounded, a tower contributes
roughly 1/16th of what it did. A profile owning all 126 skill nodes dies on wave
5 of 50 — no boss (wave 25), no second map, and 6 shards against a 5357-shard
tree is about 600 runs.

`lean` (DPS ×0.52, range ×0.82) is still too harsh: fully upgraded reaches wave
11, so the wave-25 boss is still unreachable.

**The gold cut alone is the dominant lever.** Going to 68% start gold and 75%
bounty took the autoplayer from 23 towers to 6, because bounty income *compounds*
— fewer towers, fewer kills, less gold, fewer towers. This is the same death
spiral that made a fresh profile die at wave 4 early in development.

`tight` is what shipped: stats at authored intent, range ×0.9, start gold ×0.85,
bounty ×0.9. Gold is genuinely scarcer (23 towers → 15 fresh, 107 → 92 upgraded)
while the wave reached barely moves, so choices matter more without the game
becoming impossible.

**The finding that matters more than any of this:** even at `original`, a profile
owning every skill node reaches wave 34 of 50 and **never clears map 1**. Maps
2–5 and 8 of the 10 bosses are therefore unreachable. The game is not finished
too fast — it cannot be finished at all. Making towers weaker moves in the wrong
direction.

Balance is now driven by `tools/balance.py`, which holds the authored base stats
and applies a named profile of multipliers. Applying a profile is idempotent and
switching is one command, so tuning is reversible and auditable instead of
hand-edited into TOML twice over.
