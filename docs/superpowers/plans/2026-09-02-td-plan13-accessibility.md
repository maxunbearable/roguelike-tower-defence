# Plan 13 — Accessibility and settings

**Goal:** Stop conveying essential information by colour alone, and give the
player the options a released game is expected to have.

**Why this one.** "Production ready" now has a concrete, checkable definition:
Steam publishes an [accessibility feature list](https://partner.steamgames.com/doc/accessibility_features)
that appears on the store page and that players can filter by. Measured against
its 16 categories, this game **failed three outright**:

| Steam category | before |
|---|---|
| Adjustable Difficulty | ✅ (built last round) |
| Custom Volume Controls | ✅ music and effects already separate |
| **Color Alternatives** | ❌ eleven damage types separated by hue alone |
| **Camera Comfort** | ❌ screen shake with no way to reduce or disable it |
| **Adjustable Text Size** | ❌ |

A grep confirmed the starting point: no accessibility option of any kind existed
anywhere in the source.

## What the audit found

Damage types are drawn as coloured pips in the readout that tells a player what
to build against. Eleven of them, separated purely by hue — and two pairs are
barely separable *with* full colour vision:

- `arcane` (186,148,232) against `void` (158,120,200)
- `radiant` (252,240,176) against `shock` (244,224,108)

Something else turned up while checking this, by rendering every element overlay
and looking at the sheet. The README claimed:

> Element overlays are per element rather than per specialisation (earth's three
> are bespoke; the other five share one per element).

**That is exactly backwards.** Fifteen of eighteen specialisations *do* have
their own overlay — shape says which spec, hue says which element — and **earth
is the one with none**. `tools/import_magic.py` skipped earth with the comment
"earth already has three bespoke overlays and icons; do not clobber", but earth
has bespoke *icons* (hand-authored in `make_sprites.py`) and no bespoke
*overlays*. So all three earth specialisations drew the same green flame and were
indistinguishable while playing.

## Research

- [Game Accessibility Guidelines — *Ensure no essential information is conveyed
  by a fixed colour alone*](https://gameaccessibilityguidelines.com/ensure-no-essential-information-is-conveyed-by-a-fixed-colour-alone/)
  and [Xbox Accessibility Guideline 103](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/103)
  — the single highest-impact rule; use colour as a *back-up* for text, symbol,
  pattern or shape, never as the carrier.
- [A Practical Guide To Designing For Colorblind People](https://www.smashingmagazine.com/2024/02/designing-for-colorblindness/)
  — text labels "take the guesswork out of what a symbol is intended to
  indicate"; distinct shapes work for a handful of series, not eleven.
- [Steam accessibility features](https://partner.steamgames.com/doc/accessibility_features)
  — Camera Comfort asks for shake to be adjustable *or* absent.

Tags rather than shapes, because eleven distinguishable shapes at 7px do not
exist and text is what the guidance actually recommends.

## Tasks

- **A.** `core::damageTypeTag()` — a two-letter tag per damage type, in `core/`
  so it is testable without raylib.
- **B.** Colour alternatives on the resistance pips: wider pip, tag drawn in dark
  ink on the hue, direction bar unchanged.
- **C.** Camera Comfort: a shake multiplier applied where the offset is read, so
  every consumer honours it. Three presets rather than a bool.
- **D.** A settings panel hosting both, alongside the volumes already there.
- **E.** Earth's three missing overlays.

---

## Execution log

**The guardrail is the valuable part.** `tests/content/test_accessibility.cpp`
walks every damage type that any enemy resists or any tower deals, and asserts
each has a tag that is neither the `"??"` fallback nor a duplicate of another
type's. Adding a twelfth damage type without tagging it now fails a test instead
of quietly shipping a pip only colour can identify.

**Three presets, not a toggle.** Shake is *feedback* — it tells the player
something landed. Offering only on/off forces someone who finds it uncomfortable
to give up that information entirely, so there is a Reduced setting between them.
The multiplier is applied inside `shakeOffset()` rather than at each call site,
which means every consumer honours it and a future one cannot forget to.

**Earth.** The importer's skip is now scoped to the icon rather than the whole
element, with the reason written down so it is not "fixed" back. Verified by
hashing the output: `enchant_poison`, `enchant_rock` and `enchant_quake` are
three distinct images, not three copies, and they match the other five elements'
convention of shape-for-spec in the element's hue.

**A settings reset bug, caught while wiring.** `hud_ = ui::HudState{}` runs on
every run start and resets the whole struct — including the colour-alternatives
flag copied into it. A setting that is stored but silently unapplied on the next
run is worse than one that does not exist. `applySettings()` is now called
immediately after every HUD reset, and it is the single place that pushes profile
options into the systems that honour them.

**Honest limitation: this round's UI was not verified by screenshot.** The
display became unavailable partway through (`GLFW: The GLFW library is not
initialized`) and stayed that way across four attempts over roughly ten minutes,
which has happened intermittently in earlier rounds. The standing instruction to
render and look could not be satisfied for the settings panel. What was done
instead: the panel geometry was computed independently and checked against the
panel bounds, since text overflowing its container is the failure mode that has
recurred most often here — every row fits (deepest element ends at 390 of 424)
and both option rows' text is roughly 112 and 142px inside a 340px row. That is
weaker evidence than looking, and the panel should be eyeballed next run. The
earth overlays *were* verified visually.

## Not done

**Adjustable Text Size**, the third failing Steam category. It is invasive in a
way the other two are not: every caption is a literal `10` at its call site, so
doing it properly means routing size through a helper across the whole UI rather
than adding a setting. Worth its own round.
