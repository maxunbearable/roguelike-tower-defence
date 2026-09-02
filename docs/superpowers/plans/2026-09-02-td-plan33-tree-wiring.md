# Plan 33 — The skill tree hid its own structure

**Goal:** Make the skill tree readable. Three rounds had flagged its connectors
as untidy without measuring them.

**Why this one.** The hub is where the entire meta progression lives, it has
twelve tabs, and it is one of the six screenshots the README shows. "Untidy" had
been carried as a note since plan 30; nobody had put a number on it.

## Measured

Sampling the rendered panel, luma contrast between a connector and the parchment
it sits on:

| link state | contrast |
|---|---|
| prerequisite **not** owned | **86** |
| prerequisite **owned** | **9** |

Nine luma is invisible. So the tree drew its structure for a player who owned
nothing and **hid it the moment they bought anything** — the state a progressed
player is permanently in.

The cause is one line and its history. A previous pass had found unowned links
mixed toward the background and fixed them, leaving a comment that ends
"Structure first, emphasis second":

```cpp
const Color dim = paint::mix(tint, Color{78, 62, 46, 255}, 0.70f);
DrawLineEx(..., lit ? tint : dim);
```

The *emphasis* path was never touched. `branchTint("trunk")` is
`(198,178,148)`; the panel is `(204,184,141)`. Owning a node replaced a visible
line with an invisible one, which is precisely the fault the comment describes,
inverted.

**Emphasis is now carried by width and saturation, never by lightness.** Both
states are drawn dark against the parchment; a walked link is thicker and keeps
its branch hue, an unwalked one is thinner and greyer. Shipped contrasts: 64–74
walked, 86–92 unwalked, across all seven branch tints.

## And the diagonals, at last

Links were straight lines between node centres, so a root feeding a node six
columns away drew a diagonal across the whole panel and through every node
between — in the global tree, straight through Stout Walls and Bastions.

Links are now routed in three segments with the horizontal run placed beside the
*child* rather than halfway. Halfway does not work: a node showing its cost
carries a 25px caption block, and on 84px row spacing the midpoint lands inside
it. Captions are drawn on plates, which are now **opaque and as wide as they need
within their column**, so a run passing behind one is hidden rather than
striking through it. At the previous 232 alpha it showed through the text.

The result reads as a skill tree: a bus under the parent, clean drops into each
child, and three specialisation branches in distinct hues carried down to their
leaves.

---

## Execution log

**The first test I wrote was vacuous, and it took reintroducing the bug to find
out.** It counted pixels in the rendered panel that differ from the panel colour
— which sounds like it measures wiring and does not. Node circles and captions
dominate the count, and with no sprite atlas the fallback panel is dark navy, so
even the invisible pale tint contrasted with it. With the bug put back it
reported **identical numbers**: 25003 unowned, 18975 owned, either way.

It is replaced by a measurement of the thing that was actually wrong: for every
branch tint and both states, the luma contrast between the link colour and the
panel. That needed `branchTint` and the link colour moved out of an anonymous
namespace into `ui::paint`, beside a `kTreePanel` reference value — a tint and
the contrast it has to achieve are the same question and now live together. A
second test records the original defect (the raw trunk tint measures 5 luma
against the panel), so the fix cannot be "simplified" back without both failing.
Checked by reintroducing the bug: it fails.

**Sound during captures, asked for mid-round.** A screenshot run still simulates
waves, so it fired every build, shot and death cue at the machine rendering it —
six times over for `tools/shots.sh`. `--shot` now mutes the master volume, and
`TD_MUTE=1` silences a normal run. The device is still opened, so any fault in
the audio code still surfaces; only the output is off. The test binaries were
already silent: `td_tests` does not link raylib and `td_uitests` never opens a
device.

**Suite: 322 green.** Verified by rendering the global tree owned and unowned,
and a branched tower tree, and reading all three.

## Not done

**The maps screen still does not rank the maps.** The ladder has been measured
since plan 28 and the cards have had room since plan 32. It is the last of the
"the game knows something it does not say" items.

**Adjustable Text Size** is unchanged: a reflowing-UI project, not a setting, for
the reasons measured in plan 30. `paint::wrapToWidth` remains the only piece of
it that exists.
