# Plan 11 — Onboarding

**Goal:** Teach the game. It has never done this, and the README's own list of
known gaps has led with it for several rounds.

**Why this one.** First scheduled run under the new "polished and production
ready" brief. Two production basics were checked first and turned out already
handled — the window is resizable and the canvas blits at an integer scale, so
resizing is not broken. What is not handled is that a new player is given five
one-shot hint lines and then left alone with five tower types, three
specialisations each, six elements with three each, twelve skill trees, 150
nodes, targeting priorities, two abilities and an early-call economy.

For something intended for sale, that is the gap that matters most: a commercial
tower defence with no onboarding gets reviewed on its onboarding.

## Research

- [Game Wisdom — *The Struggles of Onboarding Gamers*](https://game-wisdom.com/critical/onboarding)
  and [Best Practices For Mobile Game Onboarding](https://adriancrook.com/best-practices-for-mobile-game-onboarding/)
  — **"most people will quit a game within the first ten minutes"**, which makes
  the opening the highest-leverage part of the product.
- [Onboarding Principles](https://nerdyteachers.com/PICO-8/Game_Design/105) and
  [Onboarding Methods](https://nerdyteachers.com/PICO-8/Game_Design/106) —
  introduce **one mechanic at a time** with clear visual cues; teach *through
  gameplay*, breaking systems into steps and letting the player practise.
- [Onboarding in Game Design](https://www.numberanalytics.com/blog/onboarding-in-game-design-ultimate-guide)
  — favour bite-sized interactive guidance that introduces concepts **as they
  become relevant**; always offer a skip; balance guidance against freedom.
- [Tower Defense Design Guide](https://www.designthegame.com/learning/tutorial/tower-defense-design-guide)
  — tower defence specifically wants sequential introduction of towers, enemies
  and layouts rather than a single front-loaded explanation.

Three rules fall out, and the implementation is shaped so it cannot break them:

1. **One mechanic at a time** — a single current step, never a panel of text.
2. **Learn by doing** — every step is gated on the player performing the action.
   `tutorialSatisfied()` observes the world; it is never a timer and never a
   "click to continue".
3. **Always skippable**, and never repeated.

## Tasks

### Task A — The state machine, raylib-free
`sim::TutorialStep` with five teaching steps (build, send the wave, open a tower,
spend gold, use an ability) plus Done. `tutorialSatisfied(step, world,
menuOnTower)` reads the simulation. One piece of UI state — whether the radial
menu is open on a tower — is passed in, which keeps the whole thing testable
without a window.

Files: `src/sim/Tutorial.{h,cpp}`, `tests/sim/test_tutorial.cpp`.

Tests do each step against a real `World` and check the tutorial notices. A test
that sets a flag and reads it back would pass on a tutorial that advanced on
nothing.

### Task B — Persistence
`MetaSave::tutorialStep`, an integer rather than a done/not-done flag, so a
player who quits halfway resumes where they were.

Files: `src/core/SaveGame.{h,cpp}`.

### Task C — Presentation
A panel with the current instruction and a SKIP button. Must not fight the two
systems that already write to the screen: the contextual hints, and the wave
announcer.

Files: `src/ui/Hud.{h,cpp}`, `src/app/Game.cpp`.

## Out of scope

A dedicated tutorial *level* with scripted waves. The research favours teaching
inside real play over a separate sandbox, and a bespoke level is content to
maintain forever. The guided run happens on Greenfields, in an ordinary run that
counts.

---

## Execution log

**Task A.** Nine tests, 46 assertions, all driving a real `World`. Two design
decisions came out of writing them:

- The upgrade step asks for **any** tower past level 1 rather than the specific
  one the player built. Asking for a specific tower strands anyone who sold it,
  and there is a test that sells the tower the lesson was taught on and checks
  the step stays satisfied.
- The ability step keys on Strike's cooldown specifically, and a test fires
  **Ward** first to prove that does not count. The step names Strike, so casting
  something else must not tick it off.

**Task C — two collisions found while wiring it, both real.**

The tutorial, the contextual hints and the wave announcer all write instructions
to the player. Running them together means two voices at once, which is worse
than either alone. Hints and announcements are now suppressed while the tutorial
is running.

Second, and only visible by screenshot: the panel was initially at the **top** of
the play field, which is the conventional place for objective text — and on
these maps the route runs along the top row, so it sat directly on the enemies
the tutorial was telling the player to watch. Moved to the bottom of the play
field, above the HUD.

**A behaviour worth noting:** the tutorial advances past steps the player has
already completed, in a loop with a depth guard. Someone who levels a tower
before being asked to is not then told to go and level a tower. This is visible
in the first capture, where `--autostart` grants demo towers and starts a wave,
and the tutorial correctly opens on step three rather than step one.

**Not done:** the tutorial does not yet point *at* anything — no arrow or pulse
on the button it is describing. The research asks for "clear visual cues" and
text alone is weaker than text plus a highlight. That is the natural next step
and needs a way to name UI rectangles from the tutorial, which does not exist
yet.
