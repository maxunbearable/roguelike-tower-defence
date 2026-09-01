# Asset & Licensing Policy

This game is intended for **commercial release on Steam and the App Store**.
Every asset and dependency must therefore be clearly licensed for commercial use
and for redistribution inside a closed-source binary.

## The rule

**Nothing enters this repository unless its licence is verified and recorded
here first.** "Free to download" is not a licence. Many itch.io packs marked
"free" are free *to obtain* while forbidding commercial use, forbidding
redistribution as part of a game, or requiring attribution in a specific form.
Assume nothing.

Acceptable: **CC0 / public domain**, **MIT**, **BSD**, **zlib**, **Apache-2.0**,
**SIL OFL** (fonts), **CraftPix freebies** (see below), or original work authored
for this project.

### CraftPix freebies — cleared, with one condition

Verified 2026-08-31 against craftpix.net/file-licenses:

- Commercial use in unlimited projects: **allowed**
- Attribution: **not required**
- Selling and distributing games with the assets: **explicitly permitted**,
  including Steam and the App Store
- **Reselling or redistributing the source files: prohibited.** Assets must stay
  integrated in the finished product and may not be "made available for others
  to extract and reuse independently"

That last point is the condition that matters here: a **public** repository full
of the PNGs would arguably breach it. `.gitignore` therefore excludes
`assets/sprites/*.png`. Keep the repo private, or keep the pack untracked and
distribute it only inside the built game.

Not acceptable without written permission: anything CC-BY-NC, CC-BY-SA
(viral for derived assets), "free for personal use", "credit required" packs
where the terms are vague, ripped sprites, or AI-generated assets whose training
provenance cannot be established.

## Current status: fully clear

### Code dependencies

| Component | Licence | Commercial | In shipped binary | Obligation |
|---|---|---|---|---|
| raylib | zlib/libpng | Yes | Yes, statically linked | None legally required; attribution appreciated |
| EnTT | MIT | Yes | Yes, header-only | Include copyright + permission notice |
| toml++ | MIT | Yes | Yes, header-only | Include copyright + permission notice |
| nlohmann/json | MIT | Yes | Yes, header-only | Include copyright + permission notice |
| Catch2 | BSL-1.0 | Yes | **No** — tests only | None for binary distribution |
| CPM.cmake | MIT | Yes | **No** — build only | None |

The zlib licence is unusually friendly here: it explicitly permits static
linking into closed-source software and **does not require attribution in binary
redistributions**. The MIT dependencies do require their notice to travel with
the binary, which is what `THIRD_PARTY_LICENSES.md` is for.

### Art in use: Tiny Swords (Pixel Frog)

Imported by `tools/import_tinyswords.py` into `assets/sprites/`. This is the
primary art now: towers, the goal castle, enemies, arrows and ground.

**Licence, read from the source page 2026-08-31 — NOT CC0, despite what several
secondary sources claim:**

> "Feel free to use this asset pack in both personal and commercial projects,
> modifying the assets as needed. Crediting is not required, but it helps and is
> always welcome. **You may not redistribute, resell, or repackage the assets.**"

So: commercial use **yes**, Steam and App Store **yes**, modification **yes**,
attribution **not required** (but welcome, and worth doing).
**Redistribution: no.**

Consequences that matter:

- `assets/sprites/*.png` **must not be committed to a public repository**, since
  that makes the assets available for others to extract. Keep this repo private,
  or gitignore them and ship them only inside the built game.
- The author also offers a **`TS_old version_CC0 Licensed`** download. If the
  repository ever needs to be public, that older version is the one that can
  legally live in git.
- GitHub mirrors of this pack exist. They are redistributing it in breach of the
  licence, so they were **not** used as a source.

**Silhouette carries the specialisation, not colour.** The first mapping gave
the sniper the same shape as an unspecialised tower and the hunter the same shape
as the elf, so the board told the player nothing. Now:

| spec | what the tree promises | building |
|---|---|---|
| plain | the basic arrow tower | a modest house |
| sniper | long range, heavy single shot | the tall narrow keep -- height reads as reach |
| elf | rapid fire, nature | a timber perch, rustic rather than masonry |
| hunter | multishot | the archery hall, targets and arrows everywhere |

The same sprites are drawn in the radial menu and on the skill tree's branch
nodes, so the menu, the tree and the board all describe the same thing.

The pack is authored on a **64x64 grid** with 192px unit frames, which is why the
game moved from a 32px to a 64px tile. Buildings and units are halved on import
(an exact 2:1 reduction, the only clean one for pixel art) so a tower occupies
roughly one tile instead of the two or three an RTS would give it.

Re-import with:

```bash
python3 tools/import_tinyswords.py ~/Downloads
```

### Previously used: Kenney's "Tiny" family (CC0)

The towers, creatures and figures are **composed from CC0 assets** by
`tools/compose_art.py`, then written into `assets/sprites/` as PNG overrides.
Everything comes from one style family, by one artist, so the pieces sit
together:

| pack | licence | used for |
|---|---|---|
| Kenney **Tiny Town** | CC0 | tower masonry, crenellations, gate, arrow slit, bow, foliage |
| Kenney **Tiny Dungeon** | CC0 | the figures manning each tower (knights, marksman, green-hooded rangers) |
| **Tiny Creatures** (Clint Bellanger) | CC0 | slime, wolf, goblin, wraith |

CC0 means public domain: commercial use, modification and **redistribution** are
all permitted with no attribution required. That is why these files are tracked
in git, unlike a CraftPix pack would be.

Regenerate with:

```bash
python3 tools/compose_art.py /path/to/tiny-packs
```

Anything not overridden still uses the generated art in
`content/art/sprites.toml` (terrain, props, icons, effects).

### Rejected sources, and why

- **CraftPix** — best art fit and the licence permits commercial use and Steam,
  but downloads now sit behind a paid subscription.
- **Skyel Simple Tower Defense** (itch) — free and described as modifiable, but
  tagged **CC-BY-ND**, which forbids derivatives. That contradiction is
  unresolved, so it is not safe for a commercial release.
- **Kenney Tower Defense (Top-Down)** — CC0, but flat vector, not pixel art.
- **Kenney Tiny Battle** — CC0, but modern military rather than fantasy.

### Art and fonts

| Asset | Origin | Status |
|---|---|---|
| All sprites (`content/art/sprites.toml`) | Authored for this project as indexed pixel data | Original work, owned outright |
| Terrain tiles | Generated procedurally by `src/render/TileGen.cpp` | Original work, owned outright |
| UI font | raylib's built-in default font | Raw glyph data embedded in raylib's `rtext.c`, covered by raylib's own zlib licence. Verified: no third-party font attribution appears anywhere in raylib's source |
| All sound effects | Synthesised at runtime by `src/core/Synth.cpp` | Original work, owned outright. No audio files exist in the repository at all |

**No third-party art or audio is used.** Sound effects are generated from
waveform maths at startup rather than shipped as files, which removes an entire
category of licensing risk: there is no sample whose provenance could be
questioned. Kenney's CC0 audio packs were considered and are genuinely safe, but
synthesis was chosen because it is smaller, has no download to break, and the
result is unambiguously yours.

**No third-party art is used.** Two candidates were considered during research
and both were deliberately rejected:

- **Kenney's Tower Defense (Top-Down)** — genuinely CC0 and would have been
  safe, but rejected on *artistic* grounds: 64 px art against a 32 px grid,
  which would clash with the hand-authored 16 px sprites.
- **free-game-assets itch.io packs** (archer towers, TD enemies) — rejected on
  *licensing* grounds. Their terms were never verified, and packs of this kind
  frequently restrict commercial use or redistribution.

### Fonts, if one is ever added

Safe choices, verified licences: **Pixel Operator** (CC0, Jayvee Enaguas),
**Public Pixel** (CC0), anything under **SIL OFL** (embedding permitted; the
font file itself may not be sold on its own, which does not affect a game).
Record the licence in this file and ship the notice.

## Before shipping

- [ ] Re-run `tools/gen_third_party_licenses.sh` so the notice matches the
      dependency versions actually linked.
- [ ] Bundle `THIRD_PARTY_LICENSES.md` with the build and expose it in-game or
      in the install directory.
- [ ] Confirm no dependency has been added whose licence is copyleft (GPL/LGPL),
      which would be incompatible with a closed-source store release.
- [ ] If any audio is added later, apply this same policy to it.
