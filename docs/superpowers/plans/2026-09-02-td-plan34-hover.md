# Plan 34 — Photographing hover

**Goal:** Look at the layer of the game that only exists while the cursor is over
something.

**Why this one.** Every capture this project has ever taken left the mouse at
(0,0), where nothing is hovered. So the radial menu's detail card, the HUD's
enemy dossier, a skill tree node's numbers and every button's hot state had
never once been seen — a whole layer of the game's information display,
invisible to the verification loop by construction.

## Performance, measured and set aside first

Two earlier plans noted "targeting is O(towers x enemies) per tick" without ever
putting a number on it, so that got measured before anything else. A board at the
ceiling — every plot filled with levelled towers, wave 44, 63 enemies on the
field:

| towers | enemies | µs/tick | of a 60Hz frame | of a 4x frame |
|---|---|---|---|---|
| 4 | 66 | 2.5 | 0.0% | 0.1% |
| 10 | 64 | 4.6 | 0.0% | 0.1% |
| 20 | 63 | 8.3 | 0.0% | 0.2% |
| 29 | 63 | **11.0** | **0.1%** | **0.3%** |

Cost is linear in towers and the absolute figures are nowhere near the budget.
The concern was real algorithmically and is irrelevant in practice. Kept as a
report so it stays that way. Rendering cost is *not* measured here and cannot be:
the software backend used for capture is not representative of the GL renderer.

## The capability

`--mouse X Y` parks the cursor before the dev hooks run. raylib's display-free
backend implements `SetMousePosition` by writing straight into its input state,
and its per-frame reset is commented out, so a set position survives.

## What it found

**One non-finding, worth recording.** The radial menu's element and specialisation
rings looked, in every previous screenshot, like unlabelled coloured orbs — six
of them, differing only in hue, for a decision costing 70-100 gold. Reading
further into `RadialMenu::draw` than my first grep went, the hover card exists
and is good: hovering an orb gives **"Earth 70g — Grinding attrition. Rewards
towers that hit often rather than hard."** I nearly wrote up a bug that had been
fixed before I arrived.

**The enemy dossier lost to an ambient hint.** The HUD invites "hover for
weaknesses", and the dossier it opens was drawn at line 277 of `drawHud` while
the transient message banner is drawn at line 353 — same strip of screen, later
in the frame. So hovering an incoming enemy while any hint was on screen gave a
card with its lower rows struck through by "P pauses the wave. You can still
build while paused." The dossier is now drawn last within the HUD, and reads:
**Hive Wasp, health 62, armour 1, speed 2.7, frost ×1.35 WEAK, fire ×1.15 WEAK.**

**Thirteen skill tree nodes repeated themselves five times over.** Hovering
Sharpened Doctrine gave:

> +6% damage for every tower
> damage x1.06, damage x1.06, damage x1.06, damage x1.06, damage x1.06

A node that raises a stat "for every tower" carries one modifier per tower —
`arrow.damage`, `arcane.damage`, `ballista.damage`, `brazier.damage`,
`cannon.damage` — and `specNumbers` labels each by the target's suffix, so all
five render identically. Scanned across the content: **13 of 154 nodes with
modifiers**, four redundant entries each.

Deduplicated rather than combined, and the distinction matters: those five are
parallel, one per tower, so a tower gets ×1.06 and not ×1.34. Multiplying them
would have turned a redundant line into a wrong one. The card now reads
**"Sharpened Doctrine / +6% damage for every tower / damage x1.06 / 36 shards"**.

---

## Execution log

**The test scans the content rather than an example.** For every node in every
tree it derives the numbers line and asserts no entry repeats — 111 nodes with
modifiers, and it requires that count to exceed 100 so a scan that silently
matched nothing cannot pass. Checked both ways: without the dedupe it reports
`13 == 0`.

**A judgement recorded rather than acted on.** The map cards' PLAY button turns
pale cream when hovered while an unhovered one stays blue, which reads as *less*
prominent — the same emphasis-by-lightness mistake plan 33 found in the tree
connectors. Measured, the hovered fill is actually *brighter* (luma 190 against
178); what it loses is chroma against tan parchment. Luma does not prove the
case and a chroma argument would be a stretch, so this is left as an observation
with its numbers rather than an art-direction change made on a hunch.

**Suite: 323 green.** Every claim above was read off a rendered frame with the
cursor placed on the thing in question.

## Not done

**Hover contrast on light panels** is the observation above: worth a deliberate
pass over hot states, with a metric that accounts for hue rather than luma alone.

**The maps screen still does not rank the maps** — measured since plan 28,
carried since.

**Adjustable Text Size** remains a reflowing-UI project, unchanged.
