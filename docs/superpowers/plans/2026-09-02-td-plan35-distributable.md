# Plan 35 — Making it installable, and the repo presentable

**Goal:** Ship installers for macOS and Windows, and make the repository read as
a finished project.

## The blocker nobody had hit

```
$ strings build/td_app | grep Projects/game
/Users/maksympetrov/Projects/game/content
/Users/maksympetrov/Projects/game/assets
```

`TD_CONTENT_DIR` and `TD_ASSET_DIR` were compile-time absolute paths into the
source tree. Any installer built before fixing this would have shipped a game
that looks for the build machine's home directory and exits with
"Cannot start: content..." on every other computer. Six call sites across
`main`, `Game`, `Renderer`, `Jukebox` and `Sfx`.

`core::resourceRoot()` now searches, first hit wins: `$TD_RESOURCE_DIR`, beside
the executable, `../Resources` (macOS bundle), `../share/wardstone`, `..` (build
trees), then the source tree it was built from — last, so a packaged copy never
reads a developer's checkout that happens to be on the same machine.

## Packaging

CPack, one config: `.dmg` on macOS, NSIS installer plus portable `.zip` on
Windows, `.tar.gz` elsewhere. Resources land beside the executable, or in
`Contents/Resources` inside the bundle, which is what the resolver looks for.

Built and inspected here: **3.7 MB dmg, 6.6 MB bundle**, drag-to-Applications,
`content/` and `assets/` inside `Contents/Resources`.

Windows cannot be built on this machine, so `.github/workflows/release.yml`
builds and packages both on tag and attaches the artifacts to the release.

## The repository

- **`LICENSE`** — there was none, which reads as unfinished. Proprietary, all
  rights reserved: the stated intent is to sell this on Steam, so MIT would have
  been actively wrong. Third-party terms are pointed at, not absorbed.
- **`.github/workflows/ci.yml`** — build and test on Linux, macOS and Windows,
  which is what the build badge needs. The whole suite runs headless: `td_tests`
  never links raylib and the rendering tests use its software backend.
- **README** — a centred hero with five badges, a Download section pointing at
  releases, and packaging instructions. The 460 lines of depth below are
  unchanged.
- **Repo metadata** — description and ten topics set via `gh`.

---

## Execution log

**A verification that proved nothing, caught immediately.** The first relocation
check installed the app to `/tmp/stage`, ran it, and watched it load textures
successfully. That established nothing: the source tree still exists on this
machine, so the fallback could have been doing the work. Deleting the staged
`content/` and running again confirmed it — the app still started, reading the
developer's checkout.

The fix was to make the search a pure function, `resourceRootFrom(exeDir,
sourceFallback)`, and test it against fabricated layouts: resources beside the
exe, a macOS bundle, a Unix prefix, a build tree, and the two orderings that
matter — a packaged layout must win over a source tree that is also present, and
with nothing installed the fallback must still work. Seven tests, including one
that finds the real content from the running binary.

**A clean clone was verified green.** The art packs are gitignored, so CI only
ever has the procedural fallback. Hiding `assets/sprites/` and running the suite:
**330 passed**. The README's claim that a fresh clone builds and runs is true,
and the build badge will be honest.

**Comments cut back.** A style pass over my own recent work: several headers had
grown to between 1.5 and 3.1 comment lines per line of code, narrating history
rather than stating a reason. `WaveGen.h` was 25 comment lines for 8 of code.
That history belongs in this directory, which is the changelog; the code should
carry only what a reader needs now. Worst file went from 3.12 to 1.36 and about
a hundred lines came out, with the suite green throughout.

**Suite: 330 green.** Package rebuilt from scratch afterwards.

## Not done

**Nothing is code-signed or notarised.** macOS will warn on first launch and
Windows will show a SmartScreen prompt. Both need paid developer identities and
secrets in CI; the workflow is the right place to add them.

**No application icon.** The bundle uses the default, which is the most visible
remaining rough edge in a screenshot of the installer.

**Hover contrast on light panels**, carried from plan 34: worth a pass with a
metric that accounts for hue rather than luma alone.
