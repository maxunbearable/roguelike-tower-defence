# Plan 20 — The one system you could not improve

**Goal:** Put the two player abilities into the meta progression, where every
other capability in the game already lives.

**Why this one.** Checked before choosing, as usual, and both halves of the
premise held this time:

```
$ grep -rn "Strike\|Ward\|ability" content/trees/*.toml
(nothing)
```

Abilities appear in **no skill tree**, and all seven of their parameters are
hardcoded constants. The game's stated design is that everything is bought —
tower levels, elements, tower specialisations, element specialisations, and the
tower types themselves are all gated behind shards. Strike and Ward were the sole
exception: identical on run 1 and run 200, in a game whose entire meta
progression is the purchase of capability.

## Research

- [Kingdom Rush — Rain of Fire](https://kingdomrushtd.fandom.com/wiki/Rain_of_Fire)
  and [Upgrades](https://kingdomrushtd.fandom.com/wiki/Upgrades) — the reference
  this project names upgrades its actives along exactly three axes: **damage**,
  **explosion radius**, and **cooldown**. Its reductions are flat whole seconds
  (−10s, −10s, −5s from an 80s base), not percentages. Reinforcements upgrade
  along health, damage and armour.

Flat reductions matter: a percentage compounds badly across a long tree, and this
tree is 150 nodes deep. Ours are −6s on a 24s Strike and −8s on a 30s Ward, and
`World` clamps them at 6s and 8s so no amount of investment makes an ability free.

## The shape

Six nodes in the global tree, two chains of three:

| | | |
|---|---|---|
| Focused Charge | strike damage ×1.35 | 54 |
| Wider Impact | strike radius +0.7 | 88 |
| Rapid Invocation | −6s cooldown, damage ×1.25 | 136 |
| Binding Sigil | ward slow +12% | 58 |
| Lasting Sigil | ward duration +3s | 94 |
| Swift Sigil | −8s cooldown, +1.5s duration | 142 |

Resolved through `StatBlock` like everything else, so they compose with the
existing modifier system rather than needing a parallel one. The constants remain
the **base** the tree modifies, which is what keeps a fresh profile identical to
before — asserted by a test, because if a fresh profile changed then every
ability measurement taken before this work would silently stop meaning what it
says.

---

## Execution log

**The tests caught a real bug, and it is the exact bug that "assert observable
behaviour" exists to catch.** Buying `Focused Charge` made the Strike deal
**zero** damage.

`StatBlock` computes `(base + adds) * mults`. `resolveStats` seeds a base for
`global.startGold` and `global.lives` and nothing else, so `global.strike.damage`
had no base — and a multiplier against an unseeded path is `0 * 1.35 = 0`. The
fallback in `get(path, 1.0f)` did not save it either, because once a modifier
touches a path it *exists*, and the stored value was zero.

A test that read the resolved tuning back would have passed: the number was
there, it was just zero. The test that failed was the one that fired the ability
at an enemy and looked at its health. Multiplied paths are now seeded at 1.

**The radius test is worth its own note.** Damage-only testing would have missed a
broken radius upgrade entirely, so that test places an enemy just *outside* the
base blast and asserts it is untouched before the upgrade and hit after it. It
measures the thing the node claims to change.

**A second test failed, correctly.** `ward fields are cleaned up when they run
out` builds a world with `ownAll = true` and waits `kWardDuration + 1` seconds.
That profile now owns the duration upgrades, so its ward lasts 10.5s rather than
6s and had not expired when the test looked. The test had hardcoded an assumption
that progression invalidated; it now asks the world how long its own ward lasts.
Worth keeping in mind for any future tuning node: `ownAll` tests silently acquire
every new capability the moment it is authored.

**Not verified by screenshot.** Sixth round with the screen locked. The six nodes
are placed at columns 4 and 5, clear of the existing −3..3 range, and the tree
layout function already shrinks to fit whatever it is given — but "already
shrinks to fit" is a claim about code, not a look at the result, and the global
tree is now 29 nodes across eight columns. That is the first thing to check when
the display returns, along with the two older UI debts.

## Not done

Ward has no radius upgrade, deliberately: it is the crowd-control ability, and a
bigger *and* longer *and* stronger hold is the combination most likely to turn a
tempo tool into a win button. Damage-side progression is the safer half to open
first, and a radius node can follow once the loss counts have been re-measured
with these in play.
