# Plan 8 — The screens around the board

**Goal:** Finish the three screens that are still placeholder-grade. The board
has had five rounds of polish; the hub, map select and results have had none, and
they are three of the four screens a player touches.

**Why this.** "Still very raw" has now been said five times, and each previous
round I audited the *board*. This time I looked at every screen in the order a
player meets them — profile, hub, map select, run, results — and the answer was
obvious and consistent. It was not the board.

### What the audit found

**Skill trees (12 tabs, the entire meta progression).** The most damning finding
is not that the wiring is missing — it is that it is *drawn invisibly*:

```cpp
lit ? tint : paint::mix(tint, Color{188, 170, 146, 255}, 0.62f)
```

An unowned prerequisite link is mixed **62% toward the parchment it is drawn on**.
Links therefore only become visible once you already own the prerequisite — which
is exactly when you no longer need to see them. A new player, who owns nothing,
opens a skill *tree* and sees a grid of unconnected floating circles.

Second: `nodeIcon()` derives a meaningful icon from the modifier target **only for
the global tree**. For a tower tree, every trunk node returns `tower_<treeId>` and
every branch node returns `tower_<branch>` — so the Ballista tab is ten nodes all
drawn with the same ballista sprite. Nothing distinguishes Harpoon from Scorpion
from Javelin except a caption.

Third: the panel is ~1350px wide and the tree occupies the middle third. The rest
is blank parchment, while the node description is a tooltip that only appears on
hover and explains nothing about *why* a node is locked.

**Map select.** Five identical parchment cards of text, no map preview at all —
the player picks a map without seeing its shape. It also never states each map's
element bias, which is the single mechanic the game's replayability rests on
("each map resists a different element"). The lower half of the screen is empty.

**Results.** `BUILD FIELDED` overflows its panel and is clipped at both edges
(confirmed by screenshot). Beyond wave reached and shards earned there is nothing
— no enemies killed, no gold earned, no leaks, nothing about what actually went
wrong.

## Research

- [Wayline — *Building a Robust Skill Tree*](https://www.wayline.io/blog/unity-skill-tree-scriptable-objects)
  and [Game Design Skill Trees](https://gamedesigning.org/learn/skill-trees/) —
  dependencies need dynamic line rendering; node colour must change with state;
  **when a node cannot be bought the tooltip must say why**, listing the missing
  prerequisites; a selected node should show name, description and cost in a
  dedicated panel; group related skills with similar icons so patterns are
  recognisable at a glance.
- [Designing an MMORPG Skill Tree](https://krisnamughni24.medium.com/designing-an-mmorpg-skill-tree-eae66047baa3)
  — do not hide key information; highlight the connected path.
- [Grid Sage Games — *The Ultimate Roguelike Morgue File*](https://www.gridsagegames.com/blog/2019/07/building-ultimate-roguelike-morgue-file-part-1-stats-organization/)
  — an end-of-run screen should carry real run statistics and say **what
  defeated the player**; reviewing a run is itself part of the game.
- [22 Tips to Increase Player Retention](https://www.game-developers.org/22-tips-to-increase-player-retention-in-games-the-definitive-guide)
  — meta progression turns a loss into an investment, but only if the payout and
  the progress are made legible at the moment of failure.

## Tasks

### Task A — Make the skill tree readable
1. Unowned links drawn clearly but subordinate, so structure is visible from the
   first run. This is the single highest-value line in the plan.
2. `nodeIcon()` derives from the modifier target for **every** tree, falling back
   to tower/element art only when nothing matches, so a tab stops being ten
   copies of one sprite.
3. Tier pips on multi-step nodes, so rank within a branch reads without the
   caption.
4. The hover panel gains **why it is locked** — the names of the prerequisites
   still missing — per the research.

Files: `src/ui/Screens.cpp`.

### Task B — Map select that shows the maps
1. A procedural thumbnail per map, drawn from the tile grid and path that are
   already loaded: terrain, the route, and the goal.
2. The map's element bias: what its roster resists and what it is weak to,
   computed from the enemy definitions rather than authored twice.
3. Locked cards still show the thumbnail and theme, so the campaign reads as a
   journey rather than four grey boxes.

Files: `src/ui/Screens.cpp`, `src/sim/` or `src/content/` for the bias helper,
`tests/` for the bias computation.

### Task C — A results screen worth reading
1. Fix the `BUILD FIELDED` overflow.
2. Real run statistics: waves survived, enemies killed, leaks, gold earned,
   towers built, and the wave that ended the run.
3. Keep the shard payout as the headline — it is the reason to press on.

Files: `src/sim/World.{h,cpp}` (run counters), `src/ui/Screens.cpp`,
`tests/sim/test_run_stats.cpp`.

## Out of scope

Board decoration density. The play field is sparse compared to a Kingdom Rush
map, and that is real, but it is an art-production task rather than a code one
and it is not what makes the game unreadable today.

---

## Execution log

**Task A — skill trees.** The diagnosis held: the fix that mattered was one line.
Unowned links went from `mix(tint, parchment, 0.62)` to `mix(tint, dark, 0.70)` at
4px, and the tree acquired visible structure for the first time. `nodeIcon()` now
tries the stat-derived icon for every tree before falling back to tower/element
art, which turned the Ballista tab from ten identical ballista sprites into ten
distinguishable ones. Tier pips and a "needs X + Y" line on the hover panel
followed.

One thing the plan missed, found by zooming into a screenshot rather than reading
the code: node captions are drawn directly over the vertical links between
stacked nodes, so the label and the wiring degraded each other. Captions now sit
on a small parchment plate.

**Task B — map select.** The thumbnails work and immediately paid for themselves
by exposing a design problem the text-only screen had hidden for the whole
project: **maps 1 and 5 had no elemental identity.** Measured across each
roster:

| map | resists | weak to |
|---|---|---|
| ashen-wastes | fire 0.47 | frost 1.33 |
| blightmarsh | earth 0.50 | shock 1.32 |
| frostmere | frost 0.53 | fire 1.38 |
| greenfields | *piercing 0.90* | frost 1.04 |
| obsidian-gate | *piercing 0.80* | siege 1.05 |

Maps 2-4 have strong, designed identities. Maps 1 and 5 had only a weak piercing
tilt inherited from shared enemies and the bosses' blanket resistance tables —
and **piercing is what the starting arrow tower deals**, so the tutorial map was
quietly resisting the only weapon a new player owns. Obsidian Gate is authored as
the arcane map (its boss and its void gazers both resist arcane) but that never
survived the roster average.

Two fixes: the obsidian variants now carry the map's own element, so the final
map reads `arcane 0.79`; and `hivewasp` lost its piercing resistance, taking
greenfields to 0.93 — near enough to neutral that claiming a bias would be
false. So the threshold for reporting a bias at all was raised to 0.85/1.15 and a
genuinely even map now says **"no elemental bias"**, which is real information on
the map whose blurb is "where every build starts".

**Task C — results.** The `BUILD FIELDED` overflow is fixed by wrapping, and four
run statistics were added. Writing their test caught *my own* wrong assumption
rather than a bug: I asserted the world's gold delta equals recorded bounty, but
`startNextWave()` pays an early-call bonus and `goldEarned` counts bounty only.
The assertion was wrong, not the code.

**Bugs found and fixed along the way:** none beyond the above — unusually, this
round the audit was the hard part and the code behaved.

**Deliberately not done:** board decoration density, as scoped out.
