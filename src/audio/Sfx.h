#pragma once

#include <map>
#include <string>
#include <vector>

#include "raylib.h"

#include "sim/VisualEvent.h"

namespace td::audio {

enum class Cue { Shoot, Hit, Crit, Death, Quake, Leak, Build, Sell, Click, Buy, WaveStart, Defeat,
                 Victory };

// Every cue is synthesised at startup from td::core::Synth. Each holds a small
// pool of voices so overlapping shots do not cut each other off -- raylib
// restarts a Sound from the beginning if it is already playing.
class Sfx {
public:
    ~Sfx();
    void load();
    void unload();

    void play(Cue cue, float pitchJitter = 0.06f, float volume = 1.0f);
    // Turns simulation events into sound, with per-frame limits so a busy wave
    // does not turn into a wall of noise.
    void handle(const std::vector<sim::VisualEvent>& events);

    void setMuted(bool m) { muted_ = m; }
    bool muted() const { return muted_; }

private:
    struct Voices {
        std::vector<Sound> pool;
        size_t next = 0;
    };
    std::map<Cue, Voices> cues_;
    bool loaded_ = false;
    bool muted_ = false;
};

}  // namespace td::audio
