# Plan 15 — The measuring instrument

**Goal:** Make the thing that validates the game actually play the game.

**Why this one.** Two reasons, one environmental and one substantive.

The environmental reason is now diagnosed rather than mysterious. Screenshots
have failed for three rounds with `GLFW: Failed to find selected monitor`, and
the cause is that **the Mac's screen is locked** — macOS refuses WindowServer
access to new windows in that state. It is not a code problem and nothing in this
repository can fix it. So this round again picked work whose correctness is
provable by measurement rather than by looking.

The substantive reason is worse. Every balance figure this project has ever
produced — how far a fresh profile gets, how many runs clear map 1, whether a
fully upgraded board finishes, the per-difficulty table added last round — comes
from `sim::autoPlay`. And `autoPlay` built **only arrow towers**:

```cpp
const auto& def = reg.tower("arrow");
...
if (w.placeTower(s2.x, s2.y, "arrow") == World::PlaceResult::Ok) {
```

On a game with five tower types whose entire pitch is 270 combinations, the
instrument exercised a fifth of the tower content and none of the combinations.
Every number it produced was a faithful reading of a board no player would field.

## Research

- [Assessing Video Game Balance using Autonomous Agents](https://arxiv.org/pdf/2304.08699)
  and [LLM Agents as Automated Game Testers](https://www.emergentmind.com/topics/llm-agents-as-game-testers)
  — the useful finding is that a playtesting agent **does not need to be skilled**.
  Agent performance "is strongly correlated with human difficulty ratings even
  when their raw win rates or efficiency lag behind humans", and bots "don't need
  to match human proficiency exactly — they need to reliably detect difficulty
  imbalances and correlate with human experience".

That reframes the job precisely. The goal is not a *stronger* bot, which would
report the game as easier than it is. It is a **representative** one: a real
player has five tower types and will use them, so a bot with one cannot correlate
with their experience no matter how well it plays.

## Tasks

- **A.** `towerPreferenceFor()` — towers this profile can build, ordered by how
  the map's roster resists their damage type. Mirrors the existing
  `bestElementFor`, which already does exactly this for elements.
- **B.** Build a *mixed* board by rotating the top few preferences, rather than
  stacking one type.
- **C.** Report `distinctTowerTypes` so a mixed board and a monotype board are
  distinguishable in the output.
- **D.** Re-measure everything and correct the recorded figures.

---

## Execution log

**The gate held, which was the first thing to check.** A fresh profile has only
the arrow tower unlocked, and the harness must respect that or every "fresh
profile" figure becomes a lie. Measured: fresh is **unchanged** at wave 10 with
12 towers on all three seeds, and there is a test asserting a fresh loadout is
offered exactly one tower.

**A knife-edge case resolved itself.** Two rounds ago the fully-upgraded profile
cleared map 1 on only two of three seeds, with seed 42 stalling at wave 49 of 50,
and that was recorded as a marginal result. With a mixed board it clears
**50/50 on all three**. The game was not as marginal as the instrument said.

**A second unrepresentative model, found by re-measuring.** After the change the
loss counts did not move at all — relaxed 9, standard 19, brutal >24 — which was
the interesting part. The meta-loop's "planned" buyer follows a priority line of
`global -> arrow -> earth`, so across a 24-run campaign it **never buys a tower
charter** and never unlocks a second tower type. The harness could now field a
mix; the purchase model never let it. Charters cost 35-70 shards against global
nodes in the hundreds, so a real player buys one early. Adding `cannon` to the
line moved standard from 19 to **18**.

The size of that change is not the point. The point is that both numbers before
it described a player who finishes a campaign having only ever built one kind of
tower, on a game that sells five.

| | before | after |
|---|---|---|
| fresh profile | wave 10, 12 towers | unchanged (gate holds) |
| fully upgraded | clears on 2 of 3 seeds | clears on 3 of 3 |
| map 1, relaxed | 9 runs | 9 runs |
| map 1, standard | 19 runs | 18 runs |
| map 1, brutal | not within 24 | not within 24 |

**A self-inflicted mess worth recording.** Editing by pattern replacement
duplicated `bestElementFor` into both the anonymous namespace and `td::sim`, and
then a brace-matched deletion removed the forward declaration along with the
copy. It compiled at one point *with two full definitions of the same function*
in different namespaces, which is exactly the kind of thing that survives review
and rots. Cleaned up, with a check that the definition now appears exactly once.

## Not done

The harness still never **specialises** a tower or imbues an element beyond the
one `bestElementFor` picks, so the 270 combinations remain unexercised by any
full-run measurement — the combination matrix tests them in isolation, which is
not the same as a board using them. That is the next step for this file, and it
is a bigger one: it needs the agent to make a build *commitment* partway through
a run rather than a per-placement choice.
