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

Two candidates were considered during research. **Both verdicts below were
revised once the licences and the art were actually checked** — the original
entries had it backwards on both counts, which is why this file records evidence
rather than impressions.

- **Kenney's Tower Defense (Top-Down)** — CC0, verified at source, so it is
  legally safe. **Rejected on artistic grounds**, now confirmed by looking at it:
  downloaded and rendered to `docs/previews/kenney-td.png`, its tilesheet is flat
  vector-style, and its "towers" are small grey sci-fi turret bases and missile
  launchers. It is a military/space TD kit, not a fantasy one, and nothing in it
  can sit next to Tiny Swords. Do not revisit this one.
- **CraftPix / free-game-assets** — the earlier rejection said the terms "were
  never verified". They now are, at <https://craftpix.net/file-licenses/>:
  commercial sale is permitted ("You can sell and distribute games with our
  assets"), **no attribution is required**, modification is permitted, and
  §2.2.1 forbids reselling or redistributing the source art files. That is the
  *same shape* as the Tiny Swords licence, so it needs no new policy — the
  existing `assets/sprites/*.png` gitignore already satisfies it. **Cleared.**
  A CraftPix free pack is in fact already in use for the magic/element art.

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

## Audio (added 2026-09-01)

Unlike the sprites, **all audio in this project is CC0 and therefore committed to
the repository.** CC0 permits redistribution, so there is nothing to gitignore.

### Music — `assets/audio/music/`

| file | source | author | licence |
|---|---|---|---|
| `hub.ogg` | [The Old Tower Inn](https://opengameart.org/content/medieval-the-old-tower-inn) | RandomMind | CC0 |
| `battle.ogg` | [Determined Pursuit](https://opengameart.org/content/determined-pursuit-epic-orchestra-loop) | Emma_MA | CC0 |

Both verified at source, not from a search summary. Worth recording why: the
Determined Pursuit page carries a heading reading "Attribution Notice", which on
a careless grep looks like it demands credit. The full text is *"This track is in
the public domain as of January 2017. No attribution necessary."* Converted from
WAV to Ogg Vorbis (28 MB to 2.1 MB) so a repository can carry them.

### Sound effects — `assets/audio/sfx/`

13 files from [Kenney](https://kenney.nl)'s CC0 packs — Impact Sounds, UI Audio,
RPG Audio and Music Jingles. Kenney releases under CC0 1.0 Universal: commercial
use, modification and redistribution all permitted with no attribution required.

**One honest caveat:** I cannot listen to audio, so the cue-to-file mapping is a
considered guess, not a mix decision. It is deliberately keyed by FILENAME
(`assets/audio/sfx/<cue>.ogg`), so replacing any cue is dropping a different
`.ogg` in with that name — no rebuild, no code change. The 13 names are: shoot,
hit, crit, death, quake, leak, build, sell, click, buy, wavestart, victory,
defeat.

The procedural synthesiser remains as a **fallback**: if a file is missing, that
cue is generated instead, so the game always has audio.

### If five distinct tower families are wanted (open decision)

Tiny Swords contains only three single-tile tower silhouettes (see
`docs/ART.md`), so five visually distinct tower types with per-level upgrade art
needs a purpose-built tower defence pack. Verified options, in order:

1. **CraftPix "…Towers Pixel Art for Tower Defense" series** — the licence is
   verified above and is the same shape as the one already accepted for Tiny
   Swords, and a CraftPix free pack is already in use here for the element art,
   so nothing new has to be cleared.
   - *Free*: <https://craftpix.net/freebies/free-archer-towers-pixel-art-for-tower-defense/>
     — one family only ("Set includes only the towers and one character", 461 kB),
     so it covers the arrow line and proves the pipeline.
   - *Paid*: the Mage, Guardian and Catapult packs in the same series. These are
     the ones worth money, because they ship **towers at several stages of
     development**, which maps directly onto the game's level 1/2/3 upgrades —
     towers would visibly grow as they are upgraded, which no amount of
     compositing can fake.
2. **Kenney's Tower Defense (Top-Down)** — CC0, and rejected. See above: it is a
   flat-vector sci-fi kit whose towers are grey turret bases and missile
   launchers. Recorded so it is not researched a fourth time.

Nothing needs to change in this file's policy to adopt option 1: art stays out
of the repository and the importers stay in it.

## Packs assessed 2026-09-01 (second batch)

All CraftPix or CraftPix-style freebies, so the licence position is the one
verified above: commercial sale permitted, no attribution, source art must not be
redistributed. Verdicts are from **rendering each pack and looking at it**, which
is the only method that has ever been right here.

| Pack | Verdict |
|---|---|
| `Free-Monster-Enemy-Sprites-for-Tower-Defense` | **Adopted.** Ten creatures, each with an 18-frame Dying sequence. Imported by `tools/import_monsters.py`. Monsters 1-5 ship Fly/Fall and 6-10 ship Walking/Jump, which is where the five flying enemies come from. |
| `free-pixel-magic-sprite-effects-pack` | **Worth adopting, not yet done.** Genuine pixel art: 15 effect strips of 8 frames at 72x72, plus 11 icons. The strips are the obvious fix for the documented gap that 15 of 18 element overlays are per-element rather than per-spec. The icons are usable but read more JRPG than dark fantasy -- pick from them, do not take the set. |
| `stone-tower-game-assets` | **Not a tower pack.** The name is misleading: it is a catapult/boulder kit -- modular siege-engine bases and posts, rock debris, impact bursts -- in a soft painted style with an `FLA` (Flash) source folder, and it contains no complete tower silhouette. Rejected for towers. Its **boulders and impact bursts** are worth taking for cannon/ballista projectiles and splash effects. |
| `free-cartoon-smoke-effects-asset-pack` | **Not assessed in depth; likely reject.** 48MB and self-described as *cartoon*, against a pixel-art game. Render it before spending time on it. |

The tower situation is unchanged by this batch: see the tower section above.
Nothing here supplies five distinct fantasy tower silhouettes with per-level
upgrade art, so that recommendation still stands.
