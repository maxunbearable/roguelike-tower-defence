# Plan 16 — A dead build, and the test that should have caught it

**Goal:** Find out whether the game's headline claim — 270 balanced combinations
— is true, and fix what is not.

**Why this one.** Two false starts led here, and both are worth recording.

First, last round's note said the harness "never specialises a tower or imbues an
element". **That was wrong.** It does both, at steps 2 and 3 of `spend()`. What it
actually does is take `specs.front()`, and because `availableTowerSpecs()`
filters out specs already fielded elsewhere, a multi-tower board naturally ends
up with all three. The harness was in better shape than my own note claimed, and
building on the note instead of checking would have wasted the round.

Second, save robustness was checked as a candidate and is already correct —
`writeSlot` writes to a temp file and renames, so an interrupted write cannot
destroy a good save.

So instead of guessing, the round went looking for evidence: run the combination
matrix and see whether 270 builds are actually balanced.

## What the evidence said

Spread across all 270 cells: **1% to 34%, a 34x ratio**, median 18%. Ranking the
fifteen tower specialisations by their mean:

| spec | mean | vs median |
|---|---|---|
| **brazier/forge** | **3.0%** | **0.16x** |
| arrow/elf | 10.7% | 0.55x |
| ... | | |
| ballista/scorpion | 23.2% | 1.20x |

One specialisation in fifteen at a sixth of the median. Forge is the only
*support* build in the game: it trades its own output for an aura that raises
neighbouring towers.

## Two separate faults, found by chasing it

**1. The harness could not measure a support build.** `simulateCombo` placed
exactly **two** towers — while its own comment said "a pair of plain arrow towers
stands in for the supporting fire a real board has". So forge paid its full
self-nerf for exactly *one* neighbour to buff. Its break-even is 1.57 neighbours,
computed from its own numbers, so the scenario handed it the worst possible case,
just below the line. A real board holds twelve to thirty-four towers.

**2. Forge was genuinely a trap, not just mismeasured.** With the scenario fixed
to four towers it improved from 0.16x to 0.21x of median — better, still last.
The decidable question is not "is it weakest" but "does taking it *cost* you",
and measuring against an unspecialised baseline answered it:

| brazier build | health removed |
|---|---|
| unspecialised | **5.88%** |
| forge | **4.27%** |
| pyre | 9.48% |
| cinder | 13.44% |

Paying gold for a specialisation that reduces your output is broken by
definition.

## Why the numbers look so extreme

The fix needed +100%/+100% on the aura, which looks absurd until you see what the
scenario actually builds. `simulateCombo` levels **only** the specialised tower:

```
(5,0) level 3  damage 12.97  rate 4.43   <- the forge
(5,2) level 1  damage  5.87  rate 3.73
(4,2) level 1  damage  5.87  rate 3.73
(6,2) level 1  damage  5.87  rate 3.73
```

Forge gives up a share of a **level 3** tower to boost three **level 1** ones,
and the boosted towers all cover the same short-range overlap so much of the gain
is overkill. Doubling three small towers barely covers giving up a fifth of one
big one. Tuned by bisection to 6.80% against unspecialised 5.88% and below both
siblings: a real choice, not a trap, not a dominant pick.

## The actual deliverable

`no specialisation is worse than not specialising` — every tower's every spec,
measured against the same tower unspecialised across all three scenarios.

Every other guardrail in that file compares specs against **each other**, which
structurally cannot catch a whole tower's specs being bad, or a support spec
whose value lands on other towers. Comparing against the unspecialised baseline
can, and it fails on the original values (7.75% against 11.28%) and passes on the
fixed ones — checked both ways rather than assumed.

---

## Execution log

**A self-inflicted error worth recording, because it nearly shipped.** Tuning by
file-wide regex on `brazier.damage", op = "mult"` rewrote **five other nodes** —
trunk dmg1 (1.05), dmg2 (1.1), rate1 (1.12), pyre (1.25/0.8) and cinder (1.8/0.8)
— all silently flattened to the forge's self-nerf values. It surfaced only
because a matrix guardrail went red. Reverted and reapplied scoped to the forge
node's own block, with every other multiplier verified back at its authored
value. The lesson is narrow and concrete: never pattern-edit a value that appears
in sibling records; anchor to the record first.

**Diagnostics that paid off.** Rather than assume the aura was too small, the
buff was set to an absurd +300% as a probe: forge jumped to 53.7%, which proved
the mechanism worked and the problem was purely tuning. That one measurement
ruled out a whole class of bug in a single run.

## Not done

The matrix still levels only the specialised tower, so it measures a *maxed spec
among level-1 supporters* rather than a real board. That is defensible for
comparing specs to each other, but it is why forge needed such a large aura to
clear the bar, and it means the absolute percentages in the report are not a
picture of real play. Levelling all four would re-baseline every one of the 270
cells, so it wants its own round.
