# Plan 12 — Difficulty

**Goal:** Let the player choose how hard the game is, and in doing so settle a
contradiction this project has carried for several rounds.

**Why this one.** The README says it in its own words:

> **"Hardcore, limited resources" and "map 1 in 8–10 losses" are the same dial
> pulled opposite ways** — 21 is what hardcore costs.

Both were asked for. Both are reasonable. Neither is reachable while there is
only one tuning, and every attempt to satisfy one moved the other. That is not a
balance problem to be solved harder — it is a missing feature. A commercial
tower defence ships difficulty settings; this one had none at all, verified by
grep before starting.

Two production basics were checked first and were already fine: the window is
resizable and the canvas blits at an integer scale.

## Research

- [Enemy scaling techniques](https://medium.com/@dalemensik413/my-favorite-enemy-scaling-techniques-in-video-games-be27f1bf22ed)
  — raising one stat is the lazy version. "Swarming is another way to increase
  difficulty of an encounter by simply giving you more enemies", and varying
  encounter composition beats a single large multiplier. Scaling health alone
  changes only how *long* a wave takes; changing how many arrive changes what a
  wave *is*.
- [Do you like meta progression in your roguelikes?](https://www.resetera.com/threads/do-you-like-meta-progression-in-your-roguelikes-roguelites.1341955/)
  — players report that difficulty affecting meta progression "makes them doubt
  the game's balance and fairness". [Dead Cells' Assist Mode](https://medium.com/super-jump/how-modern-roguelikes-are-becoming-approachable-63ad844bbc27)
  deliberately does **not** disable rewards.
- Slay the Spire's Ascension and Hades' Heat are the accepted opposite framing:
  harder is *rewarded* rather than easier being *taxed*.

So: nothing reduces shard payout below baseline. Taxing the progression of
players who needed the easier setting punishes exactly the wrong people. Brutal
pays a bonus instead.

## Design

Three settings, each scaling several modest things rather than one large one:

| | enemy hp | enemy count | start gold | lives | shards |
|---|---|---|---|---|---|
| Relaxed | 0.72 | 0.85 | 1.45 | 1.5 | 1.0 |
| Standard | — | — | — | — | — |
| Brutal | 1.15 | 1.10 | 0.85 | 0.8 | 1.35 |

Standard is a true no-op, which matters more than it looks: every balance
measurement the project has recorded was taken there, and if Standard were not
identity they would all silently stop meaning what they say. There is a test
asserting exactly that.

Difficulty is a **runtime** layer, not a `tools/balance.py` profile. Those bake
values into TOML at generation time; difficulty has to vary per run.

## Tasks

- **A.** `core::Difficulty` and `DifficultyMods` — raylib-free, in `core/`.
- **B.** Applied in `sim::World`: gold and lives at construction, health at spawn,
  group size when a wave is armed, payout at the end.
- **C.** Persisted per profile; chosen on the map select screen.
- **D.** Shown in the HUD during a run.
- **E.** The meta-loop report measures runs-to-clear *per difficulty*.

---

## Execution log

**It works, and the number is the point.** Runs to clear map 1, planned buyer,
identical profile:

| difficulty | runs to clear map 1 |
|---|---|
| Relaxed | **9** |
| Standard | 19 |
| Brutal | not within 24 |

Nine is the middle of the 8–10 target this project chased across several rounds
and could not hit without abandoning hardcore. It did not need better tuning. It
needed to stop being one number.

Relaxed's curve is a clean monotonic ramp — waves 17, 22, 22, 27, 32, 35, 40, 49,
50 — rather than the long flat opening the harder settings produce.

Standard moved 21 → 19 since it was last measured, which is the abilities from
the previous round doing their job, not difficulty leaking into it.

**A latent bug found by building this.** `gainLife()` clamped against the raw
`kStartingLives` constant. On Relaxed a run opens with more lives than that
constant, so the first thing that healed the player would have silently
confiscated the extra. Lives now cap against what the run actually started with,
and there is a test that heals a Relaxed run and checks it keeps them.

**A trap avoided deliberately.** Scaling group size would put *two bosses* on the
board at Brutal, which is a different fight rather than a harder one. Groups of
one are left alone, with a test that walks the mid-boss wave on all three
settings and asserts at most one boss is ever alive.

**Caught by looking, as usual.** The difficulty blurbs overran their buttons at
10px and collided with the neighbouring option. Split into two lines — what the
waves are like, then what they pay — with the payout line green on Brutal, and
the buttons widened.

**Tests** play each setting with the same profile and seed and measure what
changes, rather than reading the multiplier table back. The end-to-end one runs a
full `autoPlay` on Relaxed and Brutal and asserts the easier setting gets
further.

## Not done

Difficulty is per profile rather than per run. That makes it a way to play rather
than a decision to re-make on every map, but it also means a player cannot take
one map on Brutal for the bonus and drop back for the next. Worth revisiting if
anyone actually wants it; per-run would need it stored in `RunSave` too.
