# Plan 14 — Display scaling

**Goal:** Make the game fit the screen it is running on.

**Why this one.** Two constraints picked it. First, the display has been
unavailable for two rounds running (`GLFW: The GLFW library is not initialized`),
so anything whose correctness depends on looking at it was the wrong choice this
time. Second, while reading `PixelCanvas` to understand that, the scaling rule
turned out to be wrong in a way that is provable with arithmetic alone.

The rule was:

```cpp
const int s = std::max(1, std::min(sw / kVirtualW, sh / kVirtualH));
```

Integer division, floored at 1. Measured against real displays:

| display | scale | canvas | screen used |
|---|---|---|---|
| **1366x768 laptop** | 1 | 1408x800 | **107%** |
| 1920x1080 | 1 | 1408x800 | 54% |
| 2560x1440 | 1 | 1408x800 | 31% |
| 3440x1440 ultrawide | 1 | 1408x800 | 23% |
| 3840x2160 | 2 | 2816x1600 | 54% |

The first row is a bug, not a preference. **107% means the canvas is larger than
the window**: on a very ordinary laptop the bottom of the HUD — gold, lives, the
wave button — is cropped away and unclickable, and there is no way for the player
to get it back. `max(1, ...)` is what forbids the only correct answer, which is
to scale *down*.

The rest is a polish problem: two thirds of a 1440p screen is black.

## Research

- [Choosing the Right Rendering Resolution for a Pixel Art Game](https://notkey.studio/en/tutorials/choosing-the-right-render-resolution-for-a-pixel-art-game/)
  — pick a base resolution that scales cleanly to common screens; 320x180 x6 is
  exactly 1920x1080. Ours (1408x800) divides nothing common, which is why every
  display below 2816 wide lands on scale 1.
- [Saint11 — *Consistency*](https://saint11.art/blog/consistency/) and
  [Pixel Art UI Resolutions](https://grokipedia.com/page/Pixel_Art_UI_Resolutions)
  — integer scaling with nearest neighbour is lossless, which is the reason to
  accept letterboxing; **never smooth pixel art** unless you know exactly why.
- [GDevelop issue #7495](https://github.com/4ian/GDevelop/issues/7495) — offering
  integer-vs-fill as a player option is an established pattern rather than a
  compromise.

So: keep integer scaling as the default, because it is lossless. Offer fill for
players who would rather use their whole monitor. And **never let either one
crop the game**, which is not a preference at all.

## Tasks

- **A.** `core::fitCanvas()` — the geometry, raylib-free so it is testable.
  Integer while the canvas fits; fractional below 1:1, where there is no integer
  option left except cropping and a slightly soft HUD beats an invisible one.
- **B.** `PixelCanvas` uses it for both the blit and the mouse mapping.
- **C.** A Crisp/Fill option in the settings panel, persisted per profile.

---

## Execution log

**205 assertions across 8 display sizes.** The test that matters asserts the
canvas never exceeds the window in either mode at any of eight real resolutions —
that is the 1366x768 bug, stated as a property rather than a special case.

Two others are worth naming:

- **Clicks map back to the pixel they were drawn on.** The blit and the mouse
  mapping share this maths; if they ever disagree, clicks land somewhere the
  player is not looking. Tested by projecting canvas points out to window
  coordinates and back at every display size and in both modes.
- **Degenerate sizes are safe.** A minimised window reports zero, and a zero
  scale would take every subsequent click-to-tile conversion with it via a
  division. The guard is there and covered.

**The base resolution is the underlying problem, and this does not fix it.**
1408x800 divides no common display, which is why crisp mode is stuck at 1x
everywhere below 4K. The research is clear that a base like 320x180 or 480x270
would scale cleanly to 1080p and 1440p — but changing it means moving every UI
coordinate, the 64px tile and the 22x11 grid. That is a project of its own.
Fill mode is the honest mitigation until then, not a substitute for it.

**Not verified by screenshot, again.** The display did not come back this round
either, so the new settings row was checked the same way as last round's: the
panel geometry computed independently against the panel bounds, with every row
fitting (deepest ends at 434 of 470, panel sits 115..585 inside a 700px play
area). The scaling change itself is covered by tests rather than by looking,
which is why it was a reasonable thing to do in a round without a display — but
the settings panel now has two rounds of unverified layout on it and should be
eyeballed as soon as the display returns.
