# Plan 25 — Undefined behaviour in the content parser

**Goal:** Close the last sanitizer finding in the project, which turned out not
to be in the project.

**Why this one.** Last round a sanitizer found a double free that had survived
twenty-three rounds of looking, so this round pointed the same instrument at
everything else. The result is worth stating plainly before the bug: under
AddressSanitizer and UndefinedBehaviorSanitizer the simulation is **clean** —
zero findings across the full suite, the 270-combination matrix, and a complete
difficulty campaign. There was exactly one finding anywhere, and it was in
toml++.

## The finding

```
parser.inl:3037:4: runtime error: assumption is violated during execution
```

Not an assert — an *assumption*. toml++ compiles its internal checks two
different ways:

```cpp
#ifdef NDEBUG
#define TOML_ASSERT_ASSUME(expr) TOML_ASSUME(expr)   // __builtin_assume: UB if false
#else
#define TOML_ASSERT_ASSUME(expr) TOML_ASSERT(expr)   // assert: abort if false
#endif
```

So a violated check is an abort in Debug and **undefined behaviour in Release**.
The check itself is `parse_key()` asserting that the character it is about to
read is already a valid key character — a precondition that malformed input can
obviously break. `TOML_ASSUME` is an unguarded `#define`, so it cannot be
overridden from the outside.

## Measuring it before fixing it

The interesting question was not "is this UB" but "how much does it matter",
which needed a number rather than an opinion. A fuzzer generated every
single-byte corruption of a real content file — truncation at every offset,
deletion of every byte, and substitution of every byte with ten syntactically
loaded characters — and parsed each in a forked child so one crash could not end
the run.

| configuration | result over 9,961 corruptions of `arrow.toml` |
|---|---|
| Release, no sanitizer | 9,961 clean, **0 crashes** |
| Release + UBSan | 30 inputs are genuinely UB |
| Debug | the same 30 abort |

Two things fall out of that. The reassuring one: a shipped binary today handles
every one of these corruptions cleanly — the UB is real but the optimiser does
not currently exploit it. The unreassuring one: that is a property of this
compiler at this optimisation level, not a guarantee, and it is exactly the kind
of thing that changes silently on a toolchain upgrade.

**The 30 were not random.** Decoded, every single one is a `[` table header
whose first character is not a valid key starter:

```
[=ower]   [.ower]   [#ower]   [\ower]   [        [[[level]]
```

The parser sees `[`, commits to a table header, and enters `parse_key()` without
checking what follows.

Swept across all 33 content files with 13,200 sampled corruptions: **25 crashes
in 18 files** — towers, elements, enemies, maps and trees alike.

## The fix

Upstream has both halves of this on record and fixed:
[#294](https://github.com/marzer/tomlplusplus/issues/294) ("Malformed input `[&`
triggers UB in parser") and
[#301](https://github.com/marzer/tomlplusplus/issues/301) ("Fix precondition
violation in parse_key when parsing `[[[` table headers"), the latter being
literally one of the shapes the fuzzer found. The callers now check for a valid
key starter and report a normal `parse_error`.

There has been **no release since 3.4.0** (October 2023), so the fix is
master-only and the pin moves to a commit rather than a tag:

```cmake
CPMAddPackage(NAME tomlplusplus GITHUB_REPOSITORY marzer/tomlplusplus
              GIT_TAG 1e8829b793b66ad17011732a146b8077d379b011)
```

Pinning past a release is a real trade-off and the reason is recorded next to the
pin. Licence re-verified at source: the repository's own `LICENSE` is MIT.

Re-run against the same corpora: **0 crashes** of 9,961, and **0** of 13,200
across all content files, in Release, Release+UBSan and Debug alike.

---

## Execution log

**The whole suite is now sanitizer-clean.** It previously truncated partway
through, because a non-recoverable UBSan finding aborts the process and takes
the remaining tests with it:

| | before | after |
|---|---|---|
| ASan + UBSan | 234 cases, 1 failure (truncated) | **294 cases, 26,278 assertions, 0 findings** |
| Debug suite | aborted on corrupt content | **294/294 pass** |
| corruption sweep, 33 files | 25 crashes / 13,200 | **0** |

**A guarded test became an honest one.** Plan 21 wrapped `unparseable TOML is
reported, not thrown` in `#ifdef NDEBUG` with a comment explaining that the
behaviour genuinely differed by build type and pretending otherwise would mean a
test that could not pass. That was true then and is not true now, so the guard
and its explanation are gone.

**Proved both ways, because a test for a dependency's bug is easy to write
vacuously.** Against the old pin in Debug the new test dies with `SIGABRT`;
against the new pin all seven `[startup]` cases pass with 42 assertions. Note
that this test would have *passed* against 3.4.0 in Release — the UB is silent
there — which is precisely why the proof was run in Debug.

**The new test enumerates the shapes rather than one of them.** Seven headers
(`[=tower]`, `[.tower]`, `[#tower]`, `[\tower]`, `[&tower]`, `[`, `[[[level]]`)
via `GENERATE`, each asserted to come back as a reportable problem. A single case
would lock one input; the family is what the upstream fix actually addresses.

**A note on the fuzzer, which is not committed.** It forks per input because the
failure mode under test kills the process, and stdio buffers duplicate across
`fork` + `abort` — the child inherits the parent's unflushed buffer, so the crash
log over-reports. The parent's own count is the one to trust. Worth remembering
before writing another harness like it.

## Not done

**Debug builds are viable again but still slow** — 555s against 9.5s optimised.
The default stays `RelWithDebInfo`; only the toml++ half of that rationale was
removed from `CMakeLists.txt`, since the performance half still stands.

**The fuzzer is throwaway.** Making it a permanent target would mean committing a
harness that deliberately crashes, and the value here was the measurement, not
the tool. If content loading grows a hand-written parser, revisit that.

**UI verification debt is unchanged** — eleventh round with the display locked.
Settings panel, radial-menu spec numbers, and the 29-node global tree remain
unlooked-at.
