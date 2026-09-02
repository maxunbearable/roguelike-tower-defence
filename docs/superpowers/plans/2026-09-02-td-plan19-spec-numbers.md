# Plan 19 — Say what a specialisation actually does

**Goal:** Put real numbers on the 33 choices the whole game is built around.

**Why this one, and why not another balance round.** The last four rounds all
lived inside the balance and measurement harness, and three of them ended with
"the thing I was about to fix is not broken". That vein is worked out; continuing
to drill it would be repeating work rather than doing new work. So this round
deliberately went elsewhere and, as usual, checked the evidence before choosing.

The check: of roughly 150 skill-tree descriptions, 109 contain a number. That
sounds healthy until you ask *which* 41 do not. **All 33 specialisation cores.**

```
arrow.sniper.core     "Far fewer, far heavier shots. Bonus damage to healthy targets."
earth.poison.core     "Hits inject stacking venom that spreads when the host dies."
arcane.hex.core       "Marks a target so every tower hurts it more."
```

The numeric trunk nodes ("+15% Earth effect magnitude") were fine. The 33 pieces
that the "270 distinct builds" claim rests on — the decision that defines a whole
run — were flavour text with no figures at all.

This was scoped out of plan 7 explicitly, as "a content pass across every tower
and element tree... worth its own round". This is that round.

## Research

Round 7's sources apply directly and were revisited rather than re-searched:

- [Defender's Quest — *Optimizing Tower Defense for FOCUS and THINKING*](https://www.fortressofdoors.com/optimizing-tower-defense-for-focus-and-thinking-defenders-quest/)
  — **total information**: every effect should show exact numbers, and
  specifically *not* "50% more damage" but "5 damage per second for 5 seconds".
  Exact figures are what turn a game from memorisation into thinking.
- [Tower Defense Design Guide](https://www.designthegame.com/learning/tutorial/tower-defense-design-guide)
  — decisions should be calculated rather than guessed.

## The approach: derive, do not write

The obvious fix is to hand-write numbers into the 33 descriptions. That is the
wrong fix: hand-written figures drift the moment a value is retuned, and a
description that lies is worse than one that is vague. This project has already
been bitten twice by content and comments falling out of step with reality.

So `content::specNumbers()` derives the figures from each node's **own
modifiers**. Sniper does not claim "heavier shots"; it reports `damage x2.6, fire
rate x0.45, range x1.7, crit +20%, armour pen +4, execute below 60%`. Retune any
of those and the text follows on its own.

It is raylib-free, so the whole derivation is tested without a window.

## Execution log

**Formatting bugs, both found by dumping the output and reading all 33 lines
rather than trusting the code.**

- `damage taken 30.0%` should be `30%`. The integrality test ran on the *stored*
  value (0.30, not whole) instead of the *displayed* one (30, whole). Fixed to
  test what the player sees.
- Durations printed bare: `mark lasts 3.5` now reads `3.5s`.

**Coverage was found by enumeration, not assumption.** The first label map
covered the tower stats and left 22 element parameters unmapped, so five element
specs produced empty lines — caught by the test asserting every core says
something. The parameters are listed explicitly rather than pattern-matched:
a label that guesses is a label that will one day be wrong.

**A `std::clamp` UB, avoided rather than hit.** The hover panel sizes itself to
its widest line and then clamps its x position into
`[kPanelX + 8, kVirtualW - w - kPanelX - 8]`. A numbers line long enough to make
`w` exceed the screen would invert that range, and `std::clamp` with `lo > hi` is
undefined. The width is now capped, and the geometry was checked arithmetically
since there is no display to look at.

**Not verified by screenshot.** The screen has been locked for six rounds, so the
two UI insertions were checked by computing the panel layout instead: both
variants fit (deepest row ends 12px inside the panel), and the clamp range is
valid. The derivation itself — the part that could be wrong in an interesting way
— is covered by tests.

## Not done

The radial menu appends the numbers to the prose in parentheses on one line,
which for Sniper is long. Whether that wraps acceptably is exactly the kind of
thing only a screenshot settles, and it is the first thing to look at when the
display returns.
