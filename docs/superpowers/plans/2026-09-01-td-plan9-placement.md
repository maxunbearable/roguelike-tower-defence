# Plan 9 — The act of building

**Goal:** Support the decision the whole genre is built on — *where do I put this
tower?* Right now the game gives the player almost nothing to make that choice
with.

**Why this.** Six rounds of "still very raw", and the word used every time is
**basic**. So this round started by verifying the basics instead of picking a
target:

| checked | result |
|---|---|
| Audio actually present, not silently falling back to synth | 13 sfx + 2 music tracks on disk ✓ |
| Imported art present | 244 sprites ✓ |
| Performance | 20 game-seconds rendered in 7s wall clock — faster than real time ✓ |
| Stability over a full run | 218 tests including five full-map autoplays ✓ |

None of those. What *is* missing is support for the core interaction:

1. **You cannot see where you are allowed to build.** The rule is invisible —
   grass is buildable, road is not — and the only feedback is a single-tile
   outline at **alpha 80** that appears under the cursor after you have already
   moved there. On a 22x11 board with a snaking route you discover the buildable
   area by hunting with the mouse.
2. **You cannot see a tower's range before you buy it.** The range ring is drawn
   only for a tower that already exists. Every first placement is blind, and
   placement is permanent apart from a lossy sell.
3. **You cannot see how much of the route a placement would actually cover** —
   which is the only thing that makes one grass tile better than another.

This is also the answer to the sparse-looking board I flagged last round as the
next candidate: a board with no visible build structure reads as an empty field,
because that is exactly what it is.

## Research

- [Kingdom Rush](https://en.wikipedia.org/wiki/Kingdom_Rush) — the reference this
  project names — gives the player "a limited amount of gold, which they can use
  to build defensive towers **on fixed locations** around a path". The build
  sites are objects on the board, not an invisible rule about terrain.
- [Tower Defense Simulator — How to Play](https://tds.fandom.com/wiki/How_to_Play)
  and [Tower Defense X — Controls](https://towerdefensex.wiki/guides/controls/) —
  when placing, "a white circle appears around it representing its range", and a
  footprint box shows where placement is blocked. Preview rings before committing
  are called out explicitly as the thing that lets players plan.
- [Placement theory](https://dungeonpath.com/posts/tower-defense-simulator/tower-placement-theory/)
  and [World Tower Defense — placement](https://worldtowerdefense.wiki/guide/placement/)
  — the quality of a spot is "how much of the path the range covers", and good
  play means overlapping rings on bends. A game that hides path coverage is
  hiding its own strategy layer.
- [Design The Game — Tower Defense Design Guide](https://www.designthegame.com/learning/tutorial/tower-defense-design-guide)
  — decisions should be calculated, not guessed.

## Tasks

### Task A — Show the board's build sites
Every buildable tile gets a visible pad. Subtle at rest so the board does not
read as graph paper, and lifted clearly whenever the player is actually choosing
a place to build.

Prototype and **look at it** before keeping it: 22x11 is ~150 buildable tiles,
far more than Kingdom Rush's dozen or so, so the risk is turning the field into
a grid. If subtle-always looks like noise, fall back to showing pads only during
the build phase and while the build menu is open.

Files: `src/render/Renderer.cpp`.

### Task B — Range and coverage preview before buying
On a hovered build site, draw the range ring of the tower that would be built
there, and highlight the length of route that ring actually covers. While a
specific tower is hovered in the build menu, use *that* tower's range rather than
a default.

Coverage needs a real number, not a vibe: the fraction of the route inside the
ring, computed from the path. That is the figure the research says decides a
spot's quality, and it is currently invisible.

Files: `src/core/` for the coverage maths (raylib-free, so it can be tested),
`src/render/Renderer.cpp`, `src/app/Game.cpp`, `tests/core/test_coverage.cpp`.

### Task C — Make the hover readable
The current outline is alpha 80 white on grass. Strengthen it, and distinguish
buildable / occupied / unaffordable / forbidden clearly enough to read at a
glance.

Files: `src/render/Renderer.cpp`.

## Out of scope

Converting to Kingdom Rush's *fixed, sparse* build sites (a dozen per map rather
than free placement on any grass). It is arguably the better design and it is
what the reference does, but it changes how many towers a board can hold, which
moves every balance measurement and the autoplayer with it. Worth proposing
separately; not worth bundling into a readability fix.

---

## Execution log

**Verifying the basics first paid off** — by ruling things out. Audio is on disk
(13 sfx, 2 music), 244 sprites are imported, and a heavy scene renders 20
game-seconds in 7 seconds of wall clock, i.e. faster than real time. None of the
"it must be broken somewhere" hypotheses survived, which is what left the
placement interaction as the answer.

**Task B — coverage maths.** Written first because it is the only part with a
right answer, and tested against a straight path where the result is known by
hand rather than against itself.

One test failed, and **the test was wrong, not the code**. I asserted "a bend is
worth more than a straight" using a hairpin whose legs were two tiles apart —
where every position sees both legs, so the middle of the straight scored 39.8
against the corner's 21.9. The maths was right and my model was wrong: a ring
centred *on* the route always covers about 2x its range whatever the shape. What
actually makes a bend valuable is that a tower set **back** from the inside of it
is near both legs at once, while the same set-back on a straight only ever sees
one. Re-tested that way: 32 tiles against 16.

**Task A — build sites.** The plan predicted the risk correctly ("the risk is
turning the field into a grid") but got the failure mode backwards. The first
attempt — 1px corner notches at alpha 26/74 — was not too noisy, it was
*completely invisible* on grass at 1:1. Confirmed by screenshot, not by reading
the code. Replaced with an inset pad plus corner ticks, then toned down once the
lifted state proved slightly heavy: 20/50 at rest, 44/104 while choosing.

The pads do less work here than in Kingdom Rush, and it is worth being honest
about why: with ~150 buildable tiles, "where can I build" is nearly "anywhere",
so the pads mostly buy two things rather than three — an unambiguous boundary at
the path edge, where the tiles that matter are, and structure on a field that
otherwise reads as empty. The third benefit, making each site a deliberate
choice, needs the fixed-sites change that this plan explicitly scoped out.

**Task C — hover.** Alpha 80 became a double-outline in warm ink, a distinct red
outline for tiles that cannot take a tower at all, and the affordability colour
now reads from the cheapest **unlocked** tower rather than hardcoding `arrow` —
which had been quoting the wrong price and the wrong range ever since tower types
became unlockable.

---

# Addendum — Plan 10: the missing pillar

Written in the same round, after the seventh "still very raw" arrived with no
answer to the choice I had offered. Seven identical messages with no specific
feedback is a delegation, and the phrase is "a lot of **basic** improvements" —
plural. Every previous round I did one narrow thing.

## What the research settled

- [Kingdom Rush](https://support.ironhidegames.com/support/solutions/articles/4000223620-kingdom-rush-battles-beginners-guide)
  ships two player abilities — Rain of Fire and Reinforcements. "Spells are cast
  by dragging and dropping them onto the field, **requiring active engagement
  from the player during gameplay**... why active player participation with these
  abilities is essential rather than passive observation."
- [Game feel and juice](https://egmatic.com/blog/how-to-make-your-game-feel-good)
  and [the perils of over-juicing](https://www.wayline.io/blog/the-perils-of-over-juicing)
  — "A common mistake is prioritizing polish before fixing core issues... only
  then does juice amplify what is already working."

That second point is what decided the order. My instinct was transitions and
screen polish; the research says a missing *mechanic* outranks it. And this game
had **no active abilities at all**: once the towers were placed, there was
nothing to do until the next build phase. That is the pillar.

## What was built

**Strike** — an immediate blast, 24s cooldown. Damage is scaled by the current
wave's health multiplier, because enemy health rises about 55x across a map and a
flat number would be a panic button on wave 3 and confetti on wave 50. Falls off
to 55% at the rim, so aiming matters.

**Ward** — a field that holds enemies, 30s cooldown, no damage at all. A tempo
tool rather than a second damage source, so it does not quietly invalidate the
balance measurements. Re-applied every tick with a short duration rather than
stamped once, so enemies walking *into* a live ward are caught and enemies
leaving recover.

Both are **free and on cooldowns rather than costing gold** — deliberate under
the gold deficit. They are the one form of agency scarcity cannot take away, so
a losing board is never purely a spectator.

Cooldowns run during the build phase too, or calling the next wave early would
have been a way to freeze them.

Plus wave announcements, naming the boss when one is coming — waves used to begin
in silence, with a counter ticking over in a corner as the only sign, and a boss
arrived exactly like trash.

## Notes

Tests assert observable effects — enemies lose health, enemies travel less far —
rather than reading back cooldown fields, because a test that only checks what it
wrote would pass on an ability that did nothing.

Two things were caught by looking rather than reasoning, continuing the pattern:
the ward's first icon was a red bar that read as a health bar, and the Q/W key
hints were drawn onto the button border where they could not be read. A
`--abilities` dev flag now casts both so the board visuals can be inspected;
every round so far has had at least one effect that was correct in code and
invisible on screen.

**Still not done:** abilities are not yet in the skill trees. They should
eventually be upgradable there like everything else, which is the natural next
step and the reason the constants live in one place.
