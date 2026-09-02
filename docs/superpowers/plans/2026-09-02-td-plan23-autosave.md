# Plan 23 — Closing the window should not cost you the build phase

**Goal:** Stop the game discarding the part of a run the player cares most about.

**Why this one.** Last round covered *what* a resume restores. This is the other
half of the same question, and it had never been looked at: **when** the game is
willing to write a run at all.

Two things, and together they are worse than either.

**The autosave fired on one condition.** `world_->waveIndex() != savedWave_` —
the wave index changing. So it captured the state at the *start* of a build phase
and nothing after it. Sized as a timeline:

```
build phase N begins ..................... AUTOSAVE
build 3 towers, upgrade 2, specialise 1 .. not saved
start the wave ........................... not saved
fight wave N ............................. not saved
close the window here .................... ALL OF IT LOST
build phase N+1 begins ................... autosave would have fired
```

The unwritten window is exactly the moment a player spends their gold.

**And the main loop exited without saving.** `while (!WindowShouldClose())`, then
`CloseAudioDevice(); CloseWindow(); return 0;`. So closing the window *was* that
case, every time.

## The fix

The autosave decision now lives in `sim::shouldAutosave()` — testable, separate
from the plumbing, and gated on whether the run has actually **changed** rather
than on the wave index.

The state it compares is wave index, tower count and gold. Gold is the load
bearing one: it moves on every purchase, upgrade, specialisation and sale, and a
tower-count check alone would miss upgrades and specialisations, which are the
expensive ones. Gold is safe to compare here precisely because a run is only
snapshottable during a build phase with an empty field — bounties cannot be
drifting it while the comparison happens.

`canSnapshot()` remains an absolute veto. Enemies and projectiles are not
serialised, so a snapshot taken mid-wave would restore a wave that had lost its
enemies, and no amount of "the state changed" may override that. There is a test
that builds a tower *during* a wave and asserts the run is still not written.

And `main()` flushes on the way out.

---

## Execution log

Straightforward, and the tests are the useful part: six cases covering the fresh
run, building, upgrading, specialising, selling, the mid-wave veto, and the
original wave-boundary behaviour — which was never wrong, only insufficient, and
still has to hold.

**What this does not fix, and cannot without a bigger change.** Quitting *mid-wave*
still loses that wave. The run is unwritable while enemies are on the field
because they are not part of `RunSave`, so the honest outcome is that a player
who closes the window mid-wave replays the wave — rather than, as before, also
losing every purchase they made before starting it. Serialising a live field is a
real option and a much larger one: enemies carry position, path distance, health,
armour, and a status set, and restoring them wrongly would be worse than
replaying a wave.

**A note on cost.** This writes the save file on every purchase during a build
phase rather than once per wave. That is a handful of writes per phase, each a
small atomic temp-and-rename, against the alternative of losing the phase. Worth
it, and worth watching if the save format ever grows.

## Not done

`enemiesSpawned_` is still unsaved, carried over from last round. Still test-only,
still no player-visible consequence.
