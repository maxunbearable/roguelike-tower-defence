# Plan 24 — A double free in the sound pool

**Goal:** Audit the one subsystem twenty-three rounds never touched, and fix what
is in it.

**Why this one.** Three rounds in a row had gone into saving and startup, which
was becoming its own vein. Audio had never been examined at all — and it cannot
be *listened* to from here, which is probably why. But a sound system can be
wrong in ways that have nothing to do with how it sounds.

## The bug

`Sfx` gives each cue a pool of voices so a busy wave can overlap the same sound.
The pool is built the way raylib intends:

```cpp
const Sound base = LoadSound(path);                 // pool[0] owns the sample data
v.pool.push_back(base);
for (int i = 1; i < poolSizeFor(c); ++i)
    v.pool.push_back(LoadSoundAlias(base));         // pool[1..] share it
```

and then torn down the way it does not:

```cpp
for (auto& s : v.pool) UnloadSound(s);              // every one of them
```

raylib's own source settles it. `LoadSoundAlias` does
`audioBuffer->data = source.stream.buffer->data;` — the alias shares the exact
pointer. `UnloadSound` calls `UnloadAudioBuffer`, which does `RL_FREE(buffer->data)`.
And `UnloadSoundAlias` carries the comment that gives the game away: *"unload the
sound buffer, **not the sample data, it is shared with the source for the
alias**"*.

So a pool of one source and three aliases was four frees of one allocation, for
every cue loaded from a file — which is all thirteen of them. Every time the game
shut down.

## Proven, not argued

Reading raylib's source is strong evidence but not proof, so the exact pattern
was reproduced standalone under AddressSanitizer:

```
==38982==ERROR: AddressSanitizer: attempting double-free on 0x000105820800
SUMMARY: AddressSanitizer: double-free raudio.c:644 in UnloadAudioBuffer
```

and re-run after the fix, which reports **0 errors and exit 0**. The probe is
sixteen lines: init audio, load one of the game's own `.ogg` files, make three
aliases, free them the way the game did. Repeatable with

```sh
clang -fsanitize=address -g -I build/_deps/raylib-src/src probe.c \
  build/_deps/raylib-build/raylib/libraylib.a \
  -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT \
  -framework OpenGL -framework CoreAudio -framework AudioToolbox -o probe
```

This is the kind of fault that hides: heap corruption at shutdown often looks
like nothing, right up until it looks like a crash report from a player.

## The fix

`Voices` records whether its pool is alias-backed. Alias-backed pools free the
aliases with `UnloadSoundAlias` and the source with `UnloadSound`; synthesised
cues, whose voices are independent `Sound`s with no shared data, are unchanged.

---

## Execution log

**The rest of the subsystem is sound**, checked rather than assumed:

- Voice rotation wraps correctly (`next = (next + 1) % pool.size()`).
- Volume and mute are applied per play, so a volume change takes effect on the
  next sound rather than needing to reach into playing ones.
- `SetSoundPitch` and `SetSoundVolume` on an alias are safe, and are the reason
  aliases exist: an alias has its own `AudioBuffer` and only shares the samples.
- The pitch jitter uses a local LCG rather than the run's seeded RNG. That looked
  like an oversight and is the right call: audio is presentation, and drawing
  from `world.rng()` would let a sound effect perturb a simulation the whole
  project depends on being reproducible.

**No regression test, and the reason is structural.** `td_tests` does not link
raylib — that boundary is what makes the simulation headlessly testable and it is
worth more than this test would be. The verification is the sanitizer probe
above, which is why the plan records the command rather than a test name.

## Not done

`Jukebox` was read and looks correct — `UnloadMusicStream` is right for streams,
and the in-memory WAV buffer it keeps alive outlives the stream that references
it, which its own comment already explains. It was not put under the sanitizer;
only the pattern that was actually wrong was.
