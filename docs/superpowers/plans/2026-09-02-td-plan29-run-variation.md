# Plan 29 — Runs that differ from each other

**Goal:** Make replaying a map mean something, in a game that calls itself a
roguelike and asks the player to replay maps many times over.

**Why this one.** The meta loop is built on repetition: shards buy the tree, a
run pays out shards, and the tree costs many runs' worth. So the thing a player
does most is play the same map again. Checking what that was actually like:

```cpp
// content/WaveGen.h, before
// Expands a recipe into concrete waves. Fully deterministic: no RNG, so a map
// always plays identically.
```

The header said it outright. `generateWaves` had no randomness anywhere in it,
the lead creature of each wave was `eligible[w0 % n]`, and the expansion happened
once at content-load time. **Every run of a map was the same fifty waves**, in
the same order, forever.

## Variation is easy; variation that is not a lottery is not

Drawing the lead creature from the run's seed took a few lines. The problem is
what it does to difficulty, and the measurements were much worse than expected:

| horizon | health spread across 16 seeds |
|---|---|
| first 8 waves | 36.5% |
| **first 12 waves** | **51.2%** |
| first 50 waves | 17.6% |

Worst exactly where it hurts: a fresh profile dies around wave 9, so the opening
is the part every grinding player replays, and one seed could hand them 51% more
health than another. Creatures differ nearly fourfold in health (slime 40,
goblin 160) and only one to three types have unlocked that early, so *which*
wave the heavy one leads swings the whole opening.

**So a wave that draws a heavier creature fields fewer of them.** Count carries
the compensation as far as it playably can and the remainder goes into the
health multiplier, so each wave brings exactly the health the canonical
expansion would have brought. Bounty is matched the same way, because a run that
drew heavy creatures would otherwise finish poorer — the same lottery wearing a
different hat, and gold is the binding constraint in this game.

Health spread across seeds afterwards: **0%**, at every horizon.

## What that did not fix, and could not

Exact health parity is not exact difficulty parity, and the measurement said so
immediately. Lives are lost per leaked **enemy**, so three heavy creatures cost a
fraction of what fourteen light ones cost even when they weigh the same. Matching
health by count alone turned a health lottery into a leak lottery — greenfields
ranged from 10 to 18 waves survived on the seed alone. Clamping the count swing
to roughly +/-40% brought that back down.

What remains is genuinely irreducible from the generator's side: **goblins carry
6 armour where slimes carry 0**, and armour subtracts from every hit, so equal
health is not equal difficulty against the low-damage towers an opening can
afford. Speed differs too. Compensating for that would mean modelling the
player's damage per hit, which the wave generator cannot know.

Measured over 24 seeds, the campaign ladder still holds:

| map | order | mean waves | range |
|---|---|---|---|
| greenfields | 1 | 10.8 | 8-17 |
| ashen-wastes | 2 | 9.7 | 8-15 |
| frostmere | 3 | 8.2 | 7-10 |
| blightmarsh | 4 | 7.0 | 6-8 |
| obsidian-gate | 5 | 3.8 | 3-5 |

Monotone in the mean, and close to where plan 28 calibrated it. The ranges are
the run-to-run variety this round exists to create.

---

## Execution log

**The feature would have shipped inert.** With the generator seeded and tested,
two screenshots of the same wave under different seeds came back **0 pixels
different**. New runs took a hardcoded `20260830` — in two separate places, one
of them the path every screenshot and the tutorial run goes through. Runs would
have stayed identical with a fully working shuffle behind them. This is the
eleventh round in a row where looking caught something reading did not.

Afterwards, wave 9 under one seed is eleven light creatures and under another is
eight armoured goblins — checked by reading the HUD's INCOMING panel, not by
diffing bytes.

**The canonical expansion is kept byte-identical.** A run's waves are a
different sequence from the map's, and every difficulty measurement in this
project was calibrated against the canonical one. The first version restarted
the draw cycle whenever the pool grew, which is right for a run — a newly
unlocked creature should appear promptly — but it moved 39 of greenfields' 50
waves. The unshuffled path is now the original formula rather than a bag that
happens to agree with it.

**A test had to be deleted rather than fixed.** "A seed cannot change the wave
envelope" asserted that count and health multiplier are seed-independent. That
was true when it was written and is now precisely backwards: those two fields
*are* the compensation. It asserted 3 == 14 on wave 8, which is the feature
working. It is replaced by the invariant that actually holds — per wave, health
and payout match within 2%, while pacing and armour come from the recipe.

**Two guardrails needed to become seed-robust.** "Plots bind the endgame board"
ran a single seed and failed once waves varied: seed 1 is a lucky opening that
survives to wave 17, banks the gold, and fills all 29 plots. Averaged over 24
seeds a fresh profile builds 14.7 of them, so the claim holds — but it is a claim
about the distribution now, and the test says so.

**Suite: 312 green.**

## Not done

**Run variance is real and unbounded by anything here.** Greenfields spans 8 to
17 waves on the seed. That is the genre working as intended, and it is also a
player losing a run to a draw. Narrowing it means either equalising armour and
speed across a map's roster — which would make the creatures interchangeable, the
opposite of the point — or a difficulty model that knows what the player can
field. Neither belongs in this round.

**The seed is invisible.** Roguelikes show it, so players can compare and share
runs, and so a bug report can carry the run that produced it. It is saved
already; nothing displays it.

**The endgame still has no ladder**, carried from plan 28: a fully upgraded
profile clears all five maps. Run variation does not change that — it varies the
opening, and the opening is not where a finished profile lives.
