# Plan 18 — Is any tower a bad purchase?

**Goal:** Now that the matrix can be trusted, use it for what it is for — and
check the answer before changing anything.

**Why this one.** Last round's plan ended by naming brazier as "genuinely the
weakest tower family" and proposing to buff it. That note was the starting point
here, and checking it first was the whole value of the round: **it was wrong**,
which would have been the third consecutive round where acting on my own written
note produced a bad change.

## The metric matters more than the measurement

The matrix reports damage **per tower**. Towers cost different amounts — 105 for
an arrow against 192 for an arcane — and the game runs a measured gold deficit,
so gold is the binding constraint and **value per gold** is the question a player
actually faces.

Ranking by raw output alone said brazier was worst. Ranking by value per gold,
using each family's *best* pairings rather than its mean, says something else:

| family | mean/gold | best-pairing/gold |
|---|---|---|
| arrow | 96% | **100%** |
| ballista | 97% | 80% |
| cannon | **100%** | 74% |
| brazier | **57%** | **77%** |
| arcane | 69% | **60%** |

Brazier has a low mean and a high ceiling. That is not weakness, it is what a
*specialist* looks like, and it is exactly what the tower's own design comment
claims: "cheap, weak per hit, utterly dependent on placement... the natural
partner for every stacking element". Its mean is dragged down by the pairings it
is not for.

Arcane is low on **both**. A tower with no good pairing is not a specialist, it
is a bad purchase in every situation — and it is the most expensive tower in the
game.

The robust statistic (mean of each family's top five pairings, per 100 gold)
confirms it: arrow 100%, ballista 80%, cannon 78%, brazier 75%, **arcane 57%**.

## The change

Through `tools/balance.py`, which owns the authored intent, not by hand-editing
generated TOML: arcane damage 18 → 21 and build cost 110 → 96, together about
+33% value per gold.

Result: **arcane 66% → 77%**, landing in the cluster with cannon 78%, brazier 78%
and ballista 80%. Every other family unmoved.

The campaign is unaffected, as it should be — arcane is an unlockable, so a fresh
profile never touches it. Fresh still reaches wave 10 with 12 towers on all three
seeds; fully upgraded still clears 50 of 50 on all three.

---

## Execution log

The round was mostly measurement, and the measurement changed the answer twice —
first from "brazier is weak" to "brazier is a specialist", then from a single
best-cell statistic to a top-five mean, because one cell is noise.

## Not done, and it is the interesting one

**Arrow is the best value per gold in the game at 100%, and it is the free
starting tower.** Every tower a player spends shards to unlock is worth 75-80% of
the one they already had. That is a progression problem rather than a balance
one: the reward for buying a charter is a *worse* tower, justified only by role
and by the one-spec-per-map rule forcing variety.

It is deliberately not fixed here. Arrow is the tower every difficulty
measurement in this project rests on — fresh-profile waves, the per-difficulty
loss counts, the reachability guardrail — so changing it moves every recorded
number at once and needs a round that re-measures the campaign end to end rather
than being bolted onto this one.

Before doing it, the thing to check is whether arrow's lead survives per
*scenario*: it has no armour penetration and the shortest useful range of the
cheap towers, so if it wins on Swarm and loses on LoneTank it is a specialist too
and the 100% is an artefact of averaging. That check comes first, because this
round is the third in a row where the honest answer was "the thing you were about
to fix is not broken".
