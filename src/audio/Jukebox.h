#pragma once

#include <cstdint>
#include <vector>

#include "raylib.h"

#include "core/Music.h"

namespace td::audio {

// Plays the procedurally composed tracks and cross-fades between them.
//
// Both streams are kept playing at all times and only their VOLUMES are
// cross-faded. Stopping and restarting a stream on every screen change pops, and
// restarting loses the loop position, so a player bouncing between the hub and a
// run would hear the same four bars forever.
class Jukebox {
public:
    ~Jukebox();
    void load();   // needs a live audio device, so not the constructor
    void unload();

    void setTrack(core::Track track);
    void update(float dt);

    // 0..1. Applied on top of the per-track cross-fade level.
    void setVolume(float v);
    float volume() const { return volume_; }

private:
    struct Stream {
        Music music{};
        std::vector<uint8_t> wav;  // raylib may reference this, so it outlives load()
        float level = 0.0f;        // current cross-fade level, 0..1
        bool ready = false;
    };

    Stream& streamFor(core::Track t) {
        return t == core::Track::Hub ? hub_ : battle_;
    }

    Stream hub_;
    Stream battle_;
    core::Track want_ = core::Track::Hub;
    float volume_ = 0.5f;
    bool loaded_ = false;
};

}  // namespace td::audio
