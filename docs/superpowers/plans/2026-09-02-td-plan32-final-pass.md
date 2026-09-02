# Plan 32 — Final pass: the shop window

**Goal:** Verify the repository builds and tells the truth about itself, and
regenerate the screenshots it shows.

**Why this one.** The README's six screenshots are the repository's shop window
and were captured on 1 September — before build plots, the campaign ladder, wave
variation, and the `ownAll` fix. Every one of them showed a game that no longer
exists, and four of them showed a fully upgraded board while claiming to show the
game (plan 30). Regenerating them turned out to require fixing five things.

## The build, from scratch

The path the README gives a player:

```
cmake -B build -G Ninja && cmake --build build
```

Clean tree, 282 targets, **320 tests green**. The only warnings from our own code
were one, and it was a real defect.

## What the compiler had been saying

```
src/app/Game.cpp:27: warning: enumeration value 'Locked' not handled in switch
```

`PlaceResult::Locked` had been added to the enum and not to the message mapping,
so it returned an empty string. The mapping now lives beside the enum, where a
new outcome cannot be added without one, and a test walks every enumerator.

Following it up found something worse in the actual code path:

```cpp
ok = world_->placeTower(x, y, item.arg) == sim::World::PlaceResult::Ok;
...
if (!ok) say("not enough gold");
```

**Every refused build said "not enough gold", whatever the reason.** A new
profile carries 275 gold and the build menu offers it a cannon at 166 that it has
not unlocked — so the one explanation the player got was the one thing that was
not true. Each action now reports why it actually refused, and the locked case
says where to unlock it.

## Five things wrong with the screenshots

Regenerating them is now `tools/shots.sh`: a temp save directory, a pinned run
seed, and a showcase profile written out in the script rather than whatever
happened to be in the developer's save. Getting six correct frames out of it
found:

1. **Four of the six were byte-identical.** `--openslot` was applied *after*
   `--autostart`, so it started a run and then reset the screen to the skill
   trees. Caught by comparing md5s before looking at them.
2. **`devCluster` scanned a hardcoded `x=6..10, y=5..7` rectangle**, which
   stopped being buildable when maps grew authored plots. Asking for twenty
   towers produced five, and the gameplay shot was a nearly empty board. It reads
   the map's plots now.
3. **It also built twenty of the same tower.** It rotates the types, which is the
   point of a game about combinations.
4. **`--menu` restarted the run**, discarding whatever `--cluster` had built, so
   the frame captioned "tower stats" showed the *build* menu on an empty plot.
5. **The boss shot had no boss in it.** `--wave` is a zero-based index, so 25 is
   wave 26 — one past the boss — and a maxed board kills the Ogre Warlord within
   a couple of seconds of it spawning. `--wave 24 --after 12` catches it at 40%.

## And a real bug the showcase profile exposed

With more than one map unlocked, the map cards' blurbs **overflow and run into
each other**:

> Open meadow. Where every build starts.Three long straights: good for reach, punishing forA winding route. Every tower gets several pass...

Cards are 250px; the blurbs run to 69 characters and were drawn with a single
centred call and no width limit. It survived every screenshot ever taken because
a locked card prints "LOCKED" instead of a blurb, and every capture came from a
fresh save. `paint::wrapToWidth` now breaks them on word boundaries, with a
rendering test that measures each line with the same `MeasureText` the drawing
uses — and checks the unwrapped string genuinely did not fit, so the test cannot
pass without the wrapping doing something.

---

## Execution log

**The README was overstating three things.** Its Status section claimed a fresh
profile "dies around wave 14", a fully upgraded one "clears all 50", and that
gold is what binds. Measured: a fresh profile dies around wave 11 and builds
about 15 of 29 plots; a fully upgraded one clears the first three maps but takes
the last two on 4 of 8 and 3 of 8 seeds and neither on Brutal; and what binds
changes over a run — gold early, the map itself at the end. All three now carry
the measured figures.

**The screenshot licence basis was undocumented**, so it is now written down —
including its limit. Checked at both vendors' own pages: **neither licence
mentions screenshots at all.** CraftPix forbids reselling "the art source files
... or slightly modified version of the art"; Tiny Swords forbids redistributing
or repackaging "the assets". The basis for committing gameplay frames is
therefore the commercial-use grant and the ordinary reading that a composited
frame is the product rather than the source — a judgement, not a term either
vendor wrote. `docs/ASSET-POLICY.md` says so plainly, and draws the line at
anything that functions as an asset sheet, which stays in gitignored
`docs/previews/`.

**Suite: 320 green**, from a clean configure as well as the working build.

## Not done

**The skill tree's long diagonal connectors** are still untidy — noted in three
rounds now. It is cosmetic and it is visible in `03-skill-tree.png`.

**The maps screen still does not rank the maps**, though the ladder is measured
and the cards now have room to say it.

**Adjustable Text Size** remains a reflowing-UI project rather than a setting,
for the reasons measured in plan 30. `paint::wrapToWidth` is the first piece of
machinery that project would need.
