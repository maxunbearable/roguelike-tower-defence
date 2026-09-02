# Plan 27 — What a tower actually costs, and where it may stand

**Goal:** Settle the question plan 18 deferred — whether every tower bought with
shards is a worse buy than the free one — and fix what the measuring turned up.

**Why this one.** Plan 18 ended on the most interesting unresolved finding in the
project: *"Arrow is the best value per gold in the game at 100%, and it is the
free starting tower. Every tower a player spends shards to unlock is worth 75-80%
of the one they already had."* If true, the meta progression sells the player
worse tools, which would undermine the loop the whole game is built on. Plan 18
also named the check that had to come first: does arrow's lead survive per
*scenario*, or is it an artefact of averaging?

## The check plan 18 asked for

It survives. Per scenario, normalised so the best tower in each column reads
100%:

| tower | LoneTank | Swarm | Mixed | mean |
|---|---|---|---|---|
| arrow | **100%** | 93% | **100%** | **98%** |
| ballista | 56% | **100%** | 83% | 80% |
| brazier | 67% | 77% | 94% | 79% |
| cannon | 65% | 76% | 89% | 77% |
| arcane | 75% | 67% | 78% | 73% |

Arrow never drops below 93% of the best; every unlockable falls to 78% or worse
somewhere. So it is not a specialist and the averaging was not hiding anything.

## But the denominator was wrong

Before acting on that, the other half: **absolute output per build site**, which
is what matters if plots rather than gold are what run out.

| tower | LoneTank | Swarm | Mixed | mean |
|---|---|---|---|---|
| ballista | 83% | **100%** | **100%** | **94%** |
| cannon | 85% | 69% | 96% | 84% |
| arcane | **100%** | 61% | 86% | 82% |
| brazier | 79% | 62% | 92% | 78% |
| arrow | 84% | 53% | 68% | **68%** |

Arrow is the *weakest* tower per site and the best per gold. Which of those two
orderings a player should care about is decided by which resource runs out
first — so that got measured too, and the answer was stark:

```
greenfields buildable tiles ........ 144
fresh profile, towers built ........  12
fully upgraded, towers built .......  34
```

**A build plot was never scarce.** The strongest possible run used under a
quarter of the map, so a tower's only cost was gold and the marginal tower was
always the cheapest one — which is the free starting tower.

Then, checking the per-gold figure itself, it turned out to be measuring the
wrong thing. The matrix simulates **maxed** towers, and the report divided their
output by `buildCost` alone — pricing every upgrade at zero. Costed properly:

| tower | cost to max | per gold | per site |
|---|---|---|---|
| ballista | 753 | **87%** | **96%** |
| arrow | 481 | 78% | 60% |
| arcane | 692 | 70% | 75% |
| cannon | 699 | 63% | 65% |
| brazier | 639 | 47% | 44% |

**Plan 18's headline finding does not survive its own metric being fixed.**
Arrow is not the best buy; ballista beats it on both measures at once. Arrow
looked dominant only because its levels were being counted as free. Nothing in
the game was selling the player a worse tower.

## What was actually wrong

The real defect is the 144. A tile that is interchangeable with a hundred others
is not a decision, and the game this one names as its reference — Kingdom Rush —
does not offer one. Maps now author their build plots:

| map | plots | mean neighbours in aura |
|---|---|---|
| greenfields | 29 | 1.86 |
| ashen-wastes | 39 | 3.69 |
| frostmere | 29 | 2.00 |
| blightmarsh | 23 | 1.91 |
| obsidian-gate | 28 | 1.79 |

Plots hug the road, because a tower that cannot reach it is not a choice, and
they come in **clumps** rather than scattered. That last part is not decoration:
the brazier's forge spec trades 61% of its own output for an aura over towers
within 2.6 tiles, so it breaks even only at 1.57 buffed neighbours. The first
layout scattered them and measured forge *below plain arrow* — a tower
specialisation deleted by a map-generation rule, without a line of its content
changing. Every map now clears that bar, and a test asserts it.

## What it did to the game

```
                  before        after
fresh profile     12 towers     12 towers    (unchanged: gold still binds early)
fully upgraded    34 towers     29 towers    (every plot on the map, still clears)
```

Gold binds the opening; the map binds the endgame. Both guardrails that could
have caught this going wrong — a fresh profile surviving its opening waves, and
the endgame staying reachable on every map — still pass.

---

## Execution log

**The round refuted the finding it set out to act on**, which is the fourth time
in this project that the honest answer was "the thing you were about to fix is
not broken". It is worth being explicit about why the wrong answer was so
convincing: the metric was published in a plan, quoted with specific figures, and
had survived a round of review. It was still measuring a maxed tower against a
level-1 price.

**Two claims were withdrawn after measuring them.** The first draft of the new
guardrail asserted the endgame board is *rich* — plots gone, gold to spare. It
is not: peak gold at the end is 169, less than a single ballista, because the
board spends everything on levels rather than banking it. Both resources are
tight; what changed is that one of them is now a position on the map. The test
says that instead.

**A visual diagnosis that was wrong, and a fix that was needed anyway.** A
screenshot appeared to show two towers stacked and overlapping, which prompted
laying clumps sideways on vertical road segments. Looking properly, it was one
tower with a two-tier sprite. But checking the layout the change replaced:
obsidian-gate had **15 vertically adjacent plot pairs** and blightmarsh 2 —
greenfields, the map in the screenshot, had none. The fix was right, for a map I
was not looking at, for a reason I had not established. Measuring the layout
would have found it; squinting at one map did not.

**71 test call sites moved off hardcoded tiles.** Tests placed towers at
coordinates like `(1, 0)` meaning nothing more than "somewhere buildable", which
stopped being true. They now ask the map through `PLOT(n)`, so the same test
survives the next time the maps change shape. The negative cases — a path tile, a
spawn, out of bounds — still name their tile, because there the coordinate *is*
the point. Two tests needed real thought rather than substitution: the knockback
test parks an enemy nine tiles along the road and needs a tower that can reach
it, and the matrix harness needs four plots close enough together to measure a
support aura at all.

**A map with no build plots now refuses to start**, with a test that strips them
and checks the message. Towers may only stand on `'o'`; a map that lost them
would load, draw, and be unplayable.

**Suite: 301 green, 9.5s.** Verified on the board: build menu opens on a plot
with all five towers priced, plots read as pads along the road, and obsidian
gate's vertical lanes show the zigzag with no overlapping sprites.

## Not done

**Plot counts vary more than they should.** Ashen-wastes has 39 against
blightmarsh's 23, and it is the *second* map. Difficulty is normalised against
path length, which no longer describes how much defence a map can hold, so the
normalisation and the plot count should be reconciled — `hp_per_wave` should
probably take plots as a term.

**The autoplayer does not choose towers by value.** It rotates through its
preferences to keep the board mixed and representative, which is right for
measuring difficulty and useless for asking "would a player now rather buy a
ballista". Answering that needs a bot that optimises, and that is a different
instrument from this one.

**Brazier measures worst on both metrics** (47% per gold, 44% per site) even with
the aura harness fixed. That may be honest — it is the support tower and the
harness gives it three neighbours where a real board gives more — or it may be a
tower that needs help. It is the obvious next thing to measure.
