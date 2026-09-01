#include "audio/Jukebox.h"

#include <algorithm>

namespace td::audio {
namespace {

// Slow enough to read as a transition rather than a cut, fast enough that
// entering a run feels immediate.
constexpr float kFadePerSecond = 1.6f;

}  // namespace

Jukebox::~Jukebox() { unload(); }

void Jukebox::load() {
    if (loaded_) return;

    const auto init = [](Stream& s, core::Track t) {
        s.wav = core::toWav(core::composeTrack(t));
        s.music = LoadMusicStreamFromMemory(".wav", s.wav.data(),
                                            static_cast<int>(s.wav.size()));
        s.ready = s.music.stream.buffer != nullptr;
        if (!s.ready) {
            TraceLog(LOG_WARNING, "jukebox: a track failed to load; running silent");
            return;
        }
        s.music.looping = true;
        SetMusicVolume(s.music, 0.0f);
        PlayMusicStream(s.music);
    };
    init(hub_, core::Track::Hub);
    init(battle_, core::Track::Battle);

    // The starting screen is a menu, so the hub track begins already up.
    hub_.level = 1.0f;
    loaded_ = true;
}

void Jukebox::unload() {
    if (!loaded_) return;
    for (Stream* s : {&hub_, &battle_}) {
        if (s->ready) UnloadMusicStream(s->music);
        s->ready = false;
    }
    loaded_ = false;
}

void Jukebox::setTrack(core::Track track) { want_ = track; }

void Jukebox::setVolume(float v) { volume_ = std::clamp(v, 0.0f, 1.0f); }

void Jukebox::update(float dt) {
    if (!loaded_) return;

    for (Stream* s : {&hub_, &battle_}) {
        if (!s->ready) continue;
        UpdateMusicStream(s->music);
    }

    const auto step = [&](Stream& s, bool wanted) {
        if (!s.ready) return;
        const float target = wanted ? 1.0f : 0.0f;
        const float delta = kFadePerSecond * dt;
        s.level = s.level < target ? std::min(target, s.level + delta)
                                   : std::max(target, s.level - delta);
        SetMusicVolume(s.music, s.level * volume_);
    };
    step(hub_, want_ == core::Track::Hub);
    step(battle_, want_ == core::Track::Battle);
}

}  // namespace td::audio
