# Plan 31 — The tutorial asked for something the game forbids

**Goal:** Look at the real first run, now that the capture path finally shows one,
and fix what it reveals.

**Why this one.** Plan 30 found that `Loadout::ownAll` had defaulted to true
since plan 3, so every gameplay screenshot this project ever took was of a fully
upgraded board. The capture path is honest now and nobody had yet gone back and
*looked*. Everything a first run is missing was invisible for thirty rounds by
construction.

## What a first run can actually do

A brand new profile owns no nodes, and the tree gates more than it looks:

```cpp
bool World::levelUnlocked(int level) const { ... }   // false: bought in the tree
bool World::elementUnlocked(...) const { ... }       // false: bought in the tree
```

So the whole of run one is: **place level-1 arrow towers**. No levels, no
elements, no specialisations. That is the intended design — Reinforced
Foundations costs 14 shards, a first run banks 13-14, so levels arrive on run two
— and it is coherent.

The tutorial did not know it.

## The deadlock

The steps ran Build, StartWave, Inspect, **Upgrade**, Ability. The fourth said:

> **Spend your gold** — Level the tower up. Gold is scarce here, so every level matters.

`tutorialSatisfied(Upgrade)` waits for any tower past level 1, and on a first run
no tower can get there. Walked in a test:

```
tutorial stalled on step 3
CHECK( step == sim::TutorialStep::Done ) with expansion: 3 == 5
```

**The tutorial could never finish**, and because the impossible step sat fourth,
everything behind it was never taught either — including Strike, a free ability
on a cooldown and the only thing a new player gets for nothing.

The header had even written down the rule it was breaking:

```
//  2. ... A step the player cannot yet perform is skipped.
```

It was not skipped. It was not implemented at all.

## The fix

**Purchase-gated steps go last.** Strike now comes before the level-up, so a
first run is taught everything it is capable of doing.

**And the step that waits says what would unlock it.** With levels locked the
prompt is no longer a request the player cannot honour:

> **Unlock tower levels** — Towers stay at level 1 until you buy Reinforced
> Foundations in the skill trees. Your first shards are best spent there.

That turns a dead end into the single most useful sentence the game can show a
new player: where the first 14 shards go. The step then completes naturally on
the run after the purchase, which is when the lesson is relevant.

---

## Execution log

**The first test I wrote asserted the wrong thing.** It checked that a first run
reaches `Done`, which the fix deliberately does not do — the last step waits for
a purchase. Worse, it could not detect the fix at all: before and after, the walk
ends on step 3, because reordering moved Upgrade from fourth to last. It now
records the steps actually *taught* and asserts Ability is among them. Checked
both ways: with the old ordering it fails on `taughtStep(Ability) == false`.

**A capture bug had to be fixed before the fix could be seen.** A screenshot of
the tutorial came back showing step 0 whatever the save said, because
`--freshrun` was applied before `--openslot`: a run started with no profile open
reads a default profile. Ordering the flags correctly is what made the prompt
visible.

**I misread the HUD and nearly reported a bug.** The next-wave button appeared to
show `1s` remaining of a twelve second build phase. It reads `11s`; two ones in a
pixel font at 1x are a single stroke apart. The adjacent `+20 gold` is
`int(buildTimer) * 2`, which already said the timer was around ten seconds --
the arithmetic was there to check the reading against, and zooming settled it.
Worth remembering that reading a screenshot has its own failure modes.

**Suite: 317 green.** Verified by rendering both new prompts and reading them.

## Not done

**A first run has one verb.** Place a level-1 arrow tower, and that is the entire
vocabulary for roughly ten waves. It is deliberate scarcity and it is also the
first impression the game makes. Whether it should hand out a taste of depth --
one element for the first run, say, withdrawn afterwards -- is a design question
worth measuring rather than assuming, and it is not a bug.

**The skill tree draws long diagonal connectors** from the global roots out to
the ability nodes. Noted in two rounds now; still untidy.

**The maps screen still does not show which map is harder**, though plan 28
measured it.

**Adjustable Text Size** remains out of reach as a setting, for the reasons
measured in plan 30: it needs a reflowing UI, which is its own project.
