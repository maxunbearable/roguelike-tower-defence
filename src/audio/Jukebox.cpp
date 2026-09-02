#include "audio/Jukebox.h"

#include "core/Paths.h"

#include <algorithm>
#include <filesystem>

namespace td::audio {
namespace {

// Slow enough to read as a transition rather than a cut, fast enough that
// entering a run feels immediate.
constexpr float kFadePerSecond = 1.6f;

}  // namespace

Jukebox::~Jukebox() { unload(); }

void Jukebox::load() {
    if (loaded_) return;

    const std::filesystem::path dir =
        core::assetDir() / "audio" / "music";

    const auto init = [&dir](Stream& s, core::Track t, const char* file) {
        // Prefer the recorded CC0 track. The composed one stays as the fallback
        // so a clone with no audio assets still has music rather than silence.
        const auto path = dir / file;
        if (std::filesystem::exists(path)) {
            s.music = LoadMusicStream(path.string().c_str());
            if (s.music.stream.buffer != nullptr) {
                s.ready = true;
                s.music.looping = true;
                SetMusicVolume(s.music, 0.0f);
                PlayMusicStream(s.music);
                return;
            }
        }
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
    init(hub_, core::Track::Hub, "hub.ogg");
    init(battle_, core::Track::Battle, "battle.ogg");

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
