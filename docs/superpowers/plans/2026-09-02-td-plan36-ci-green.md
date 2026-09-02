# Plan 36 — Installers, and getting CI actually green

**Goal:** Ship installers, and make the build badge tell the truth on all three
platforms.

## The blocker installers would have hit

```
$ strings build/td_app | grep Projects/game
/Users/maksympetrov/Projects/game/content
```

Resource paths were compile-time absolutes into the source tree, so any installer
would have shipped a game that exits with "Cannot start: content..." everywhere
but the build machine. `core::resourceRoot()` searches instead: `$TD_RESOURCE_DIR`,
beside the executable, `../Resources` (macOS bundle), `../share/wardstone`, `..`,
and the source tree **last**.

Verified: **3.7 MB dmg**, resources inside `Contents/Resources`, and the icon in
the plist and the bundle.

## Three platforms, three different bugs

Publishing the workflow turned the badge red on two of three runners, each for a
reason that could not reproduce locally.

**Linux — a header that was not self-contained.** `WaveGen.h` used `uint64_t`
without `<cstdint>`. libc++ supplies it transitively; libstdc++ does not.
`AutoPlayer.h` had the same latent fault. Both fixed, and a `td_headers` target
now compiles every header as its own translation unit with the real build flags,
built by CI on all three platforms. 63 headers, all self-contained.

**Windows — two.** `raylib_sw` did not link `winmm`, so `InitTimer`'s call to
`timeBeginPeriod` left `td_shot` and `td_uitests` with unresolved externals;
raylib's own CMake links it for that platform and the hand-rolled target did not.
And the tests used POSIX `setenv`, which MSVC does not have — they go through a
helper that uses `_putenv_s`. Added `/utf-8`, since the sources are.

**Linux again — a test that measured 8 seeds and claimed 24.** The campaign
ladder guardrail inverted maps 1 and 2 there:

```
  map 1 greenfields:  8.75 waves
  map 2 ashen-wastes: 11.875 waves
```

Two faults behind it. The guardrail averaged 8 seeds where the report it mirrors
averages 24, so the two disagreed about the same measurement — 12.25 against
10.8 for map 1 — and 8 seeds of waves-survived is a noisy mean, because it is a
small integer whose distribution is bimodal near a death threshold. Maps 3-5
agreed across platforms within half a wave; only the two easiest maps, where a
fresh run lives longest, swung.

Seeds now match. The assertion moved from pairwise monotonicity to the ladder's
shape — the first half materially harder than the last, and the final map the
hardest — because that is reproducible: the halves gap measured **4.85 on macOS
and 4.81 on Linux**, against 2.5 for the inverted ladder the test exists to
catch. Pairwise steps are ~1.1 waves apart, finer than the measurement's
cross-platform reproducibility, so they are not asserted.

## The repository

`LICENSE` (proprietary — the game is for sale, so MIT would be wrong), CI across
three platforms, a release workflow that packages on tag, a README hero with
badges and a download section, repo description and ten topics, and an
application icon drawn from the game's own palette rather than the sprite packs,
whose licences forbid redistribution.

---

## Execution log

**A verification that proved nothing.** The first relocation check installed the
app, ran it, and watched it load textures. That established nothing: the source
tree still exists here, so the fallback could have done the work. Deleting the
staged `content/` and running again confirmed exactly that — it started anyway.
The search became a pure function, `resourceRootFrom(exeDir, sourceFallback)`,
tested against fabricated layouts including the two orderings that matter: a
packaged layout must beat a source tree that is also present, and with nothing
installed the fallback must still work.

**A clean clone verified green.** The art packs are gitignored, so CI only has
the procedural fallback. Hiding `assets/sprites/` and running the suite: **330
passed**. The badge is honest.

**The icon was built twice.** The first was a hand-typed character grid and came
out lopsided and off-centre. Drawn from shapes instead, it is symmetrical by
construction; checked at 16, 32, 64 and 128 px.

**Comments cut back.** Several headers had reached 1.5-3.1 comment lines per line
of code, narrating history rather than stating a reason — `WaveGen.h` was 25
comment lines for 8 of code. That history belongs in this directory. Worst file
went 3.12 to 1.36, about a hundred lines came out, suite green throughout.

**The same shell trap, a third time.** `$INC` unquoted in zsh arrives as one
argument, so a header-self-containment sweep reported 40 false failures before I
noticed the include flags were never applied.

## Not done

**Nothing is signed or notarised.** macOS will warn on first launch, Windows will
show SmartScreen. Both need paid identities and CI secrets.

**macOS CI is slow** — over twenty minutes, because it builds two full copies of
raylib (GL and software) plus Catch2 on a small runner, and the dependency cache
key includes the whole of `CMakeLists.txt`, so editing it invalidates the cache.
A `CPM_SOURCE_CACHE` plus `restore-keys` would fix most of that.

**Hover contrast on light panels**, carried from plan 34.
