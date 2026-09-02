# Plan 21 — Failing usefully

**Goal:** Make the game say what is wrong instead of crashing, and ship a binary
worth playing.

**Why this one.** Seventh round with the screen locked, so again the target had to
be something provable without looking. Two production concerns had never been
examined at all: what happens when content is malformed, and whether anything
leaks over a long run. The first turned out to be broken in three separate ways.

## What was wrong

**Content errors were thrown into the void.** `main()` called
`registry.loadAll(...)` with no `try`/`catch`. Demonstrated rather than assumed,
by putting a typo in `arrow.toml`:

```
libc++abi: terminating due to uncaught exception of type std::runtime_error:
  content/towers/arrow.toml: missing or wrong-typed key 'buildCost'
```

The loader had carefully produced a precise message naming the file and the key.
Nobody ever saw it — the player got a crash.

**Validation errors were logged and then ignored.** `content::validate()` returned
its findings, `main()` wrote them to the log at `LOG_ERROR`, and then started the
game anyway. A tree modifier pointing at a stat path that does not exist —
exactly the mistake nearly shipped two rounds ago — would run in an undefined
state rather than refusing to start.

**A window that cannot open produced a wall of noise.** `InitWindow` has no return
value and the game carried on regardless, emitting `GLFW: Failed to find selected
monitor` and framebuffer errors. That is precisely the state this machine has
been in for seven rounds, and it reads like a graphics driver fault rather than
what it is.

## The fix

`content::loadAndValidate()` — loads, validates, and **never throws**: a malformed
file, a missing directory and a failed validation all come back as an outcome
with a message fit to show a player. All validation errors are reported at once,
because fixing content one failed launch at a time is a miserable way to work and
the validator has already found them all.

Verified by running the real binary against deliberately broken content:

| | before | after |
|---|---|---|
| typo'd key | `libc++abi: terminating...` | `Cannot start: .../arrow.toml: missing or wrong-typed key 'buildCost'`, exit 2 |
| bad stat path | logged, game started anyway | `Cannot start: content failed validation: tree 'global': node 'global.strike1' modifies unknown stat path...`, exit 2 |
| no window | pages of GLFW warnings | `Cannot open a window. The display is unavailable — on macOS this usually means the screen is locked.`, exit 3 |

The window case is the one thing the locked screen made *verifiable*: it was
tested by running it, not reasoned about.

## The build type, which is the bigger finding

A test corrupting TOML syntactically rather than semantically did not fail — it
**aborted**:

```
Assertion failed: (is_bare_key_character(*cp) || is_string_delimiter(*cp)),
  function parse_key, file parser.inl, line 3037
```

toml++ asserts inside its own parser, and `assert()` calls `abort()`, which no
`catch (...)` can intercept. So in a Debug build the process dies before any of
the error handling above runs.

Which led to the real problem: **`CMakeLists.txt` defaulted to `Debug`**. The
README tells a player to run `cmake -B build -G Ninja`, and that produced an
unoptimised, assert-enabled binary — to *play the game with*. The default is now
`RelWithDebInfo`: optimised, symbols kept, `NDEBUG` set. Debug is one flag away.

Checked both ways rather than assumed: the startup tests **abort in Debug and
pass in Release**, so a Release build was configured separately and run to prove
it before changing anything.

The one test whose behaviour genuinely depends on the build type is guarded by
`#ifdef NDEBUG` and says why, rather than being deleted or left failing.

**The cost of that default, measured.** The full suite ran in **450 seconds**
under Debug and runs in **9.14 seconds** optimised — about fifty times faster,
for the same 283 tests. Every round of this project has been waiting eight
minutes on a suite that takes nine seconds, and every player building from the
README got the same unoptimised binary.

---

## Execution log

The round went where the evidence did. The plan was to look at two production
concerns and pick one; the first produced three defects and a fourth underneath
them, so the second (entity growth over a long run) was never reached. It remains
unexamined — the probe was written and discarded when the startup work turned out
to be the bigger fish.

## Not done

**Entity growth over a long run.** The probe for it needed an EnTT API that has
moved (`storage<entity>().in_use()`), and the startup findings took the round.
Worth a look: nothing currently asserts that a 50-wave run does not accumulate
entities, and the cleanup is spread across four systems.
