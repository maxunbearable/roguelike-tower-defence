# Plan 28 — A campaign that gets harder

**Goal:** Make the maps get harder in the order they are played, and give the
difficulty dial authority in the range where a player actually loses.

**Why this one.** Plan 27 changed what decides how much defence a map can hold
and left the consequence open: *"difficulty is normalised against path length,
which no longer describes how much defence a map can hold."* Checking it turned
up something worse than a stale normalisation.

## The campaign was not a ladder

Measured with one comparable profile across eight seeds, waves survived against
an authored order of 1..5:

| map | order | fresh waves | range |
|---|---|---|---|
| greenfields | 1 | 10.0 | 10-10 |
| ashen-wastes | 2 | **8.5** | 8-10 |
| frostmere | 3 | 9.0 | 9-9 |
| blightmarsh | 4 | **9.5** | 9-10 |
| obsidian-gate | 5 | 4.0 | 4-4 |

Map 2 was the second-hardest map in the game and map 4 was easier than map 2.
The maps screen says *"clear a map to unlock the next"*, so the order is a
promise the game was not keeping.

## Why the dial could not fix it

The health curve is `hpPerWave ^ (wave ^ exp)` and only its **wave-50 end was
pinned**. One parameter then had to serve both ends of a fifty-wave run:

| map | target | wave-10 health | wave-50 health |
|---|---|---|---|
| greenfields | 1.00 | 1.86 | 55.3 |
| blightmarsh | 1.18 | **1.89** | 61.5 |

An 18% turn of the dial moved wave 50 by 18% and **wave 10 by 1.6%**. Across all
five maps the early multiplier sat between 1.85 and 1.89 whatever the target
said — and a fresh profile dies around wave 9. The dial did nothing where the
game is played, and since a fully upgraded profile clears every map, the wave-50
end it *did* move was invisible too. Ordering was left to whatever the geometry
happened to do.

## The fix: pin both ends

Two anchors, two unknowns, solved rather than authored:

```
ln w50 = 49^exp . ln h        ln wE = (E-1)^exp . ln h
  =>  exp = ln(ln w50 / ln wE) / ln(49 / (E-1))
  =>  h   = exp(ln w50 / 49^exp)
```

`hpCurveExp` stops being a hand-set constant and becomes what the two anchors
imply. The dial then has authority early, and the ladder was calibrated against
measured runs rather than derived:

| map | dial | fresh waves | range |
|---|---|---|---|
| greenfields | 1.00 | 10.0 | 10-10 |
| ashen-wastes | 0.97 | 9.0 | 8-10 |
| frostmere | 1.23 | 8.0 | 7-9 |
| blightmarsh | 1.56 | 7.0 | 7-7 |
| obsidian-gate | 0.65 | 5.0 | 5-5 |

---

## Execution log

**Two model ideas were tried and thrown away.** Both are worth recording,
because each was convincing.

**Exposure, as a normalising term.** How many towers reach each tile of the
route, summed along it, is a far better description of what a board delivers
than route length: across the five maps route length spans 15% and exposure
spans 54%. It also *explains* Obsidian Gate exactly. Put on the same dial as map
1 it still measured 4.0 waves against greenfields' 10.0, and neither the dial,
the wave cadence, nor its armoured roster accounted for it — stripping the armour
bought back one wave of six, and giving it a gentler cadence bought nothing. Its
geometry did: a dozen towers reach each of its path tiles 2.07 times over
against greenfields' 2.66, on a shorter route, so the same board deals 27% less
damage. At roughly a wave per 6% of health, that is the missing five.

It still had to come out. Compensating for it moved obsidian the right way and
ashen the wrong way, and no exponent between 0 and 1.5 flattened the set. A term
that explains one map and mispredicts another is not a model.

**The autoplayer's spot ordering.** It ranks plots by individual coverage and
takes the best, which piles picks into the densest stretch of road. Measured
against a set-cover spread of the same twelve towers, it left between 4% and 24%
of the route unwatched depending on the map — a per-map bias in the instrument
every difficulty number depends on. That looked like something to fix before
tuning anything against it.

Fixing it made blightmarsh **harder**, not easier. Concentrated overlapping fire
kills better than thin coverage everywhere, so the ordering was not a bias to
correct, and the argument for changing it evaporated. Reverted: the instrument
should not be altered on a theory the measurement disproves.

**A validator that could never fire, caught before it shipped.** Raising early
difficulty against a fixed wave-50 anchor pushes the solved exponent under 1,
which tripped an existing rule — `hpCurveExp >= 1` — that was exact while the
exponent was authored and the curve pinned at one end. Its first replacement,
"late growth must outpace early growth", is worthless:

```
exp=1.0   late-span 35.000  early-span 10.000  bends: True
exp=0.5   late-span  3.258  early-span  1.742  bends: True
exp=0.01  late-span  0.013  early-span  0.013  bends: True
```

For any positive exponent `49^e - 14^e` exceeds `14^e - 4^e`, so the check is
true by construction. What can actually fail is the opening creeping up towards
the ending until the curve is flat, so the rule is now *wave 50 must be at least
20x wave 5* — shipped maps run 35x to 53x — with a test that flattens a map's
curve and checks the message.

**The new ladder guardrail is non-vacuous**, checked by putting the
pre-calibration dial back: it fails on `9.0 <= 8.5`, the exact inversion it
exists to catch.

**Suite: 303 green.** Rendered and read: blightmarsh and the maps screen draw
correctly, plots read as pads, and the tall shape in the middle of the board is
one two-tier tower sprite rather than two stacked — checked against the layout,
which has zero vertically adjacent plots on every map.

## Not done

**Obsidian's dial is saturated.** It measures 5.0 waves at 0.65 and 5.0 at 0.55,
so health has stopped being the lever — its floor is set by its roster and
geometry. Probes priced the pieces: the +1 armour on every enemy is worth about
a wave, and wolves arriving at wave 3 rather than 4 another. Evening the last
step of the ladder from two waves to one means touching those, not the dial.

**The late anchor still uses path length**, which the exposure measurement shows
is a poor proxy for what a finished board delivers. It is not currently
discriminating anything — a fully upgraded profile clears all five maps — which
is its own question: the endgame has no ladder at all.

**The player cannot see the ladder.** The maps screen shows waves and bosses but
nothing about relative difficulty, so the ordering this round established is
still only visible to the test suite.
