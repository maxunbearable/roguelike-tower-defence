# Plan 26 — Being able to look at the game

**Goal:** Make visual verification possible without a display, and then spend it
on eleven rounds of UI that shipped unseen.

**Why this one.** Every round carries the instruction to verify by looking, and
for eleven rounds that was impossible: the screen has been locked, so the game
could not open a window. The debt compounded quietly — a settings panel, the
radial menu's stat readout, six new skill-tree nodes, all correct in code and
never once looked at. Fixing another simulation bug would have added a twelfth.
So the target this round was the instrument itself.

## Why the display was refused, exactly

Worth stating precisely, because "the screen is locked" had been treated as a
wall for eleven rounds and it is really a much smaller obstacle:

```
CG active displays: 0        <- locked
CG online displays: 1        <- the monitor is still there
GLFW monitors: 0  primary=0x0
hidden window: CREATED
GL_VERSION: 4.1 Metal - 90.5 / Apple M3
```

**A full OpenGL context was available the whole time.** macOS marks the display
*online but not active* while locked, GLFW then enumerates zero monitors, and
raylib's `InitPlatform` aborts — not because it cannot render, but because it
cannot decide where to *centre a window*:

```c
platform.handle = glfwCreateWindow(...);   // succeeds
if (monitorIndex < monitorCount) { ... }   // 0 < 0 is false
else { glfwTerminate(); TRACELOG("Failed to determine Monitor to center Window"); return -1; }
```

Both branches of that function need a monitor, so there is no flag that gets
past it.

## The fix: render in software

raylib ships a display-free backend (`rcore_memory.c`) over a software
rasteriser (`rlsw.h`) for exactly this, zlib-licensed like the rest of it. The
capture tool is therefore **the identical game code** — same `src/render`, same
`src/ui`, same `Game` — linked against raylib built with `PLATFORM_MEMORY`. No
second renderer, no mock, nothing that could drift from what players see.

Three things had to be corrected before a capture was trustworthy, and each was
found by looking at the result rather than by reading the code:

| symptom | cause |
|---|---|
| every capture upside down | rlsw returns its framebuffer bottom-up |
| RED renders blue, ORANGE renders cyan | rlsw transposes red and blue |
| terrain and every UI panel solid black | `SW_MAX_TEXTURES` defaults to **128**; the game loads over 300, and texture 129 onward silently failed |

The texture ceiling is the interesting one. It produced a screenshot that was
*plausible* — sprites drawn, HUD text crisp, layout correct — and simply had no
ground, no road and no panel art. Believing that first capture would have meant
"fixing" a renderer that was never broken.

With all three corrected, the headless capture of the tutorial state is
indistinguishable from `j-tutorial.png`, the GL reference taken when the screen
was last unlocked: same grass, same roads, same parchment HUD, same banners.

## What eleven rounds of unseen UI actually looked like

All of it correct, which is worth recording as a non-finding:

- **The settings panel** (unverified for three rounds) — sliders at MUSIC 50%
  and SOUND 85%, COLOUR TAGS / SCREEN SHAKE / DISPLAY toggles, RESUME, QUIT.
- **The radial menu's stat readout** — `elf L3`, damage 20, fire rate 8.1, dps
  185, range 4.9, targeting First, crit 9%, armour pen 2, piercing, rock. The
  plan-19 `SpecFacts` work renders and aligns.
- **The global tree at 29 nodes across eight columns** — the six plan-20 ability
  nodes are placed, readable and not overlapping, which is precisely what that
  plan flagged as unchecked.
- **The map select** and **results** screens.

## Two defects that only looking could find

**The settings modal dimmed the board but not the HUD.** The scrim was
`DrawRectangle(0, 0, kVirtualW, kPlayH, ...)` — the play area, not the canvas —
so the bottom 96px stayed at full brightness under the modal. Measured:

| sample | modal closed | modal open (before) | after |
|---|---|---|---|
| board, y=400 | (170,171,88) | (53,52,33) | (53,52,33) |
| tutorial box, y=660 | (204,184,141) | (59,53,46) | (59,53,46) |
| **HUD strip, y=750** | (204,184,141) | **(204,184,141)** | **(59,53,46)** |

**The results screen ended in a black band.** The board is only `kPlayH` tall
and the HUD was not drawn on that screen, so the bottom 96px was bare backdrop
under the scrim, leaving a hard seam. The HUD is now drawn behind it. Mean
luminance step across y=704: **15.3 before, 6.0 after**.

Both are the same mistake — an overlay sized to the board rather than the
screen — which is why they are fixed together.

---

## Execution log

**Measuring beat looking, once.** From the screenshot I was confident the
tutorial box was also undimmed. Sampling the pixels said otherwise: it dims to
(59,53,46) exactly like everything else, and only the HUD strip was wrong. The
eye was fooled by a bright panel next to a dark one. Every claim in the table
above is a sampled pixel rather than an impression.

**The rendering tests are a new kind for this project.** `td_uitests` is a
separate binary from `td_tests` on purpose: `td_tests` must never link raylib,
because that boundary is what keeps the simulation headlessly testable. The new
target draws the real `ui::drawPause` over a flat field, reads the framebuffer
back, and asserts on luminance — a test that can only exist now that the game
can be rendered without a display.

Checked both ways: with the scrim reverted to `kPlayH`, two of the four cases
fail with `play area luma 93, HUD strip luma 255`. Two of them guard the capture
helper rather than the UI — orientation and red/blue — because if the readback
is wrong then every other rendering assertion silently checks the wrong thing.

**A latent bug in the existing capture mode.** `TakeScreenshot` prepends the
working directory, so `--shot /tmp/x.png` wrote to `<cwd>//tmp/x.png` and saved
nothing, silently. It had never surfaced because previous rounds only ever wrote
relative paths. Capture now goes through `render::captureScreen()` and
`ExportImage`, which writes where it is told.

**Suite: 298 tests green, 9.3s** (294 before, plus the four rendering tests).

**Twice bitten by the same shell trap.** zsh does not word-split unquoted
parameters, so `$args` holding `--autostart --settings` arrives as one
unrecognised argument. The first time it produced a capture of the wrong screen
that I nearly reported as a bug; the second time it wrote five identical files.
Worth remembering: build capture command lines explicitly, not from a variable.

## Not done

**The GL path is unverified.** `td_app` still cannot open a window here, so the
switch from `TakeScreenshot` to `ExportImage` is verified only under the software
backend. It is the same two raylib calls on both, but that is an argument, not a
look.

**Fidelity is established by comparison, not by proof.** The headless capture
matches the GL reference of the same scene closely enough that I could not tell
them apart, and the four sampled colour checks pass — but rlsw is a different
rasteriser, and subtle differences in blending or filtering would not
necessarily show up this way. Treat it as excellent for layout, text, colour and
composition; treat a *pixel-exact* claim about the GL build as unproven.

**The dps figure in the tower panel** reads 185 where damage 20 x fire rate 8.1
is 162. It may well be right — crit and element procs are plausible
contributors — but the panel is the number a player balances around and nothing
currently asserts the three agree. Worth a round.
