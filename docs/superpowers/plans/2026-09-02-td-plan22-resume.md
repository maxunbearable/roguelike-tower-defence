# Plan 22 — What a run keeps when you come back

**Goal:** Make resuming a run give the player back the run they left.

**Why this one.** Two candidates were checked and the first one closed cleanly,
which is worth recording as a non-finding: **there is no entity leak.** Across a
full run the ECS returns to zero enemies and zero projectiles with 415 spawned in
total, so the cleanup spread across four systems is doing its job. That took
thirty seconds to establish rather than the eight minutes it would have last
round, which is the previous round's build-type fix paying for itself
immediately.

That left save/restore. The existing snapshot test checks gold, lives, wave index
and every tower, and all of it was correct — but `RunSave` carries eight fields
and `World` holds fourteen members, so the interesting question was what falls in
the gap.

## Two things fell in the gap, both player-facing

Demonstrated by playing a run, saving it, restoring it and printing both sides:

```
before saving:    strike cooldown 13.0001s,  towersBuilt 1
after reloading:  strike cooldown 0s,        towersBuilt 0
```

**Run statistics were wiped.** The results screen reports enemies killed, leaked,
gold earned and towers built — added two rounds ago precisely so a player can
read how a run went. None of it survived a reload, so anyone who stopped for
lunch was told at the end that they had done nothing.

**Ability cooldowns were refreshed.** This one is an exploit, and it works
because of an interaction rather than a single oversight: cooldowns tick during
the **build phase**, and the build phase is exactly and only when the game is
allowed to autosave (`canSnapshot()` requires it). So firing an ability, quitting
to the hub and pressing Continue handed it straight back.

## The fix

Both now travel in `RunSave`, as plain fields rather than the simulation's own
`RunStats` type — `core` must not depend on `sim`. Older saves load unchanged:
the statistics default to zero and the cooldown list defaults to empty, with the
restore reading defensively rather than indexing past the end of a vector that
older saves do not have.

---

## Execution log

**The tests were checked both ways.** With the snapshot side reverted, three of
the four fail; with it in place, all four pass. That matters more than usual here
because a save/restore test is unusually easy to write vacuously — restore into a
freshly constructed world and compare against a default, and everything agrees
because everything is zero. The helper plays a real run first: builds four
towers, fights a wave to its end, spends an ability, and asserts the run actually
did something (`towersBuilt > 0`, `enemiesKilled > 0`) before comparing anything.

**The round trip is tested through JSON, not just in memory.** `snapshot()` into
`restore()` proves the two functions agree with each other; it does not prove the
save *file* carries the fields. A separate test goes through `toJson`/`fromJson`,
which is the path a real resume takes.

**A small structural annoyance worth noting:** `World` is neither copyable nor
movable, so a helper cannot return one. The fixture fills a world in place
instead. Fine, but it means every test constructs its own world with the right
loadout, difficulty and gold override — which is exactly the kind of repetition
that eventually hides a mistake in one of them.

## Not done

`enemiesSpawned_` is still not saved. It is used only by tests and nothing shows
it to a player, so it was left alone rather than widening the save format for
something with no observable consequence — but it does mean the counter under-
reports after a resume, and a future test that trusts it across a save would be
wrong.
