# Plan 30 — The first two minutes, and a default that owned everything

**Goal:** Walk what a new player actually meets, and fix it.

**Why this one.** Plans 28 and 29 both closed on the same carried item — *"a
fully upgraded profile clears all five maps, so the endgame has no ladder"* —
so this round started by measuring it properly rather than acting on it.

## The carried claim was wrong

Fully upgraded, eight seeds per cell, waves survived of 50 and how many cleared:

| map | Relaxed | Standard | Brutal |
|---|---|---|---|
| greenfields | 50.0 (8/8) | 50.0 (8/8) | 49.9 (7/8) |
| ashen-wastes | 50.0 (8/8) | 50.0 (8/8) | 49.6 (7/8) |
| frostmere | 50.0 (8/8) | 50.0 (8/8) | 49.5 (7/8) |
| blightmarsh | 50.0 (8/8) | **48.0 (4/8)** | **43.2 (0/8)** |
| obsidian-gate | 50.0 (8/8) | **47.6 (3/8)** | **44.5 (0/8)** |

The endgame has a ladder after all: the last two maps are a coin flip for a
finished profile on Standard and unbeaten on Brutal. The claim came from reading
a single seed on a single difficulty out of an earlier report. Recorded here so
it stops being carried.

## So: what does a new player meet?

Rendered against an empty save directory, which is the only way to see it.

**The slot card says "click to begin a new game".** Clicking it opened the skill
trees: twenty-nine nodes, every one locked, every price red, with 0 shards and
nothing on screen saying where shards come from. The footer underneath said
"click a profile to enter its skill trees" — the game knew what it was doing and
the card promised something else.

Three changes:

- A profile that has never played **and cannot afford the cheapest node in the
  game** now opens on the map select. Somebody returning still lands on the
  trees, broke or not, because they know where they are.
- The footer says "click a profile to play", which is now true of both.
- With an empty purse the trees say **"shards are earned by playing -- START RUN
  below"**, because every other piece of text on that screen is a price the
  player cannot meet.

## The bigger find, from trying to photograph the opening

Capturing the real first run showed **655 gold and 48 lives** where greenfields
authors 275 and the game grants 20. Every map was over by exactly 380 — which is
40 + 70 + 110 + 160, the four gold nodes of the global tree.

```cpp
struct Loadout {
    ...
    bool ownAll = true;  // Plan 3 replaces this with the meta save
};
```

**The default loadout owned the entire skill tree.** Scaffolding from plan 3,
still in place at plan 30; the meta save it was waiting for arrived around plan
5. Any `Loadout{}` silently granted every node in the game.

Shipping players were never affected — the real run path builds its loadout from
the profile and sets `ownAll = false` explicitly. What it did affect:

- **Every gameplay screenshot this project has taken.** The dev capture path used
  `Loadout{}`, so all of them showed a fully upgraded board — 48 lives, tree-buffed
  tower stats — while claiming to show the game. The capture path now uses the
  profile's real loadout.
- **Every test that built a world without saying what it owned**, which was most
  of them.

The default is now `false`, which is the honest one: a loadout owns nothing
unless told otherwise.

---

## Execution log

**Flipping the default failed 18 tests**, every one of them a test that
specialises, levels or attaches an element — all gated on the tree. They now ask
for what they need through a `tdtest::owningAll()` helper rather than leaning on
a default, which is what they meant all along. One of them had the whole thing
written on its face:

```cpp
sim::World withTree(reg, reg.map("greenfields"), 1);  // default owns everything
```

**Two more failures were the interesting kind.** `a run snapshot restores an
identical world` restored a fully-owned snapshot into a world that owned
nothing, and the tower stats came back different — 22.46 against 20.23. That is
correct behaviour finally being visible: stats derive from the profile, so a
restore into a different profile *should* differ. The test was comparing two
different players and calling it a save bug.

**The onboarding test asserts through the game, not the flag.** It checks that a
default-loadout world starts with exactly the map's authored purse, and that a
fully-owning one starts with more. Reverting the default fails it with
`655 == 275`, which is the number that started this.

**A capture hook had to be added for the transition.** `--hub` jumps straight to
the trees and so cannot show a routing decision made when a slot is opened;
`--openslot` goes through the same path a click does. A new profile lands on the
map select and an established one on the trees, checked by rendering both.

**Suite: 314 green.**

## Not done

**The skill tree draws long diagonal connectors** across the whole panel, from
the global roots out to the ability nodes added in plan 20. It is legible but
untidy, and it is the kind of thing that reads as unfinished in a screenshot.

**The maps screen still does not say which map is harder.** Plan 28 measured the
ladder and nothing shows it; the order numbers and the lock chain imply it, which
is not the same as telling the player.

**Adjustable Text Size is not reachable and should stop being deferred.** Steam's
criterion, verified at source in plan 26, is *"scaling text up to at least 38
pixels tall at 1080p"*. The virtual canvas is 1408x800 and integer-scales to 1x
on a 1080p display, so a 10px caption is 10 screen pixels and the bar needs 3.8x.
In a fixed layout with a 96px HUD that is not a setting, it is a reflowing UI —
a project in its own right, and it should be planned as one rather than listed as
a leftover each round.
