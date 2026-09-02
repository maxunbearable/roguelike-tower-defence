#include "audio/Sfx.h"

#include <algorithm>
#include <filesystem>

#include "core/Paths.h"
#include "core/Synth.h"

namespace td::audio {
namespace {
using core::Envelope;
using core::Pcm;

// How many simultaneous voices each cue needs. Arrows overlap constantly;
// a defeat sting never does.
int poolSizeFor(Cue c) {
    switch (c) {
        case Cue::Shoot: return 8;
        case Cue::Hit: return 8;
        case Cue::Death: return 5;
        case Cue::Crit: return 3;
        case Cue::Quake: return 3;
        default: return 2;
    }
}

Pcm synth(Cue c) {
    switch (c) {
        case Cue::Shoot:  // bowstring: a short bright snap falling away fast
            return core::mix(core::noise(0.055f, {0.001f, 0.05f, 0.0f, 0.005f}, 0.5f, 7u),
                             core::square(0.055f, 900.0f, 320.0f, 0.25f,
                                          {0.001f, 0.05f, 0.0f, 0.005f}, 0.35f));
        case Cue::Hit:  // a small dry tick
            return core::thud(0.045f, 260.0f, {0.001f, 0.04f, 0.0f, 0.005f}, 0.55f, 11u);
        case Cue::Crit:  // brighter, with a ring on top
            return core::mix(core::thud(0.09f, 340.0f, {0.001f, 0.08f, 0.0f, 0.01f}, 0.7f, 13u),
                             core::square(0.09f, 1400.0f, 900.0f, 0.5f,
                                          {0.001f, 0.085f, 0.0f, 0.005f}, 0.30f));
        case Cue::Death:  // a downward blip: something stopped existing
            return core::mix(core::square(0.13f, 420.0f, 120.0f, 0.5f,
                                          {0.002f, 0.12f, 0.0f, 0.01f}, 0.5f),
                             core::noise(0.09f, {0.001f, 0.08f, 0.0f, 0.01f}, 0.3f, 23u));
        case Cue::Quake:  // low rumble
            return core::thud(0.34f, 70.0f, {0.01f, 0.30f, 0.0f, 0.03f}, 0.85f, 29u);
        case Cue::Leak:  // ominous descending tone; losing a life must sting
            return core::square(0.42f, 300.0f, 90.0f, 0.5f, {0.01f, 0.36f, 0.0f, 0.05f}, 0.55f);
        case Cue::Build:  // rising three-note arpeggio
            return core::concat({core::square(0.05f, 440.0f, 440.0f, 0.5f,
                                              {0.001f, 0.045f, 0.0f, 0.005f}, 0.4f),
                                 core::square(0.05f, 587.0f, 587.0f, 0.5f,
                                              {0.001f, 0.045f, 0.0f, 0.005f}, 0.4f),
                                 core::square(0.09f, 880.0f, 880.0f, 0.5f,
                                              {0.001f, 0.08f, 0.0f, 0.01f}, 0.45f)});
        case Cue::Sell:  // the same idea, falling
            return core::concat({core::square(0.05f, 700.0f, 700.0f, 0.5f,
                                              {0.001f, 0.045f, 0.0f, 0.005f}, 0.4f),
                                 core::square(0.09f, 400.0f, 330.0f, 0.5f,
                                              {0.001f, 0.08f, 0.0f, 0.01f}, 0.4f)});
        case Cue::Click:
            return core::square(0.022f, 1200.0f, 900.0f, 0.5f, {0.001f, 0.02f, 0.0f, 0.002f},
                                0.30f);
        case Cue::Buy:  // a bright chime for permanent progress
            return core::concat({core::square(0.06f, 660.0f, 660.0f, 0.5f,
                                              {0.001f, 0.055f, 0.0f, 0.005f}, 0.4f),
                                 core::square(0.14f, 990.0f, 1320.0f, 0.5f,
                                              {0.001f, 0.13f, 0.0f, 0.01f}, 0.45f)});
        case Cue::WaveStart:  // a two-note horn
            return core::concat({core::square(0.12f, 330.0f, 330.0f, 0.35f,
                                              {0.004f, 0.11f, 0.0f, 0.01f}, 0.5f),
                                 core::square(0.20f, 440.0f, 440.0f, 0.35f,
                                              {0.004f, 0.18f, 0.0f, 0.02f}, 0.5f)});
        case Cue::Defeat:
            return core::concat({core::square(0.22f, 300.0f, 260.0f, 0.5f,
                                              {0.01f, 0.20f, 0.0f, 0.02f}, 0.5f),
                                 core::square(0.45f, 200.0f, 90.0f, 0.5f,
                                              {0.01f, 0.40f, 0.0f, 0.05f}, 0.55f)});
        case Cue::Victory:
            return core::concat({core::square(0.11f, 523.0f, 523.0f, 0.5f,
                                              {0.003f, 0.10f, 0.0f, 0.01f}, 0.45f),
                                 core::square(0.11f, 659.0f, 659.0f, 0.5f,
                                              {0.003f, 0.10f, 0.0f, 0.01f}, 0.45f),
                                 core::square(0.30f, 784.0f, 784.0f, 0.5f,
                                              {0.003f, 0.28f, 0.0f, 0.03f}, 0.5f)});
    }
    return {};
}

Sound soundFrom(const Pcm& pcm) {
    Wave w{};
    w.frameCount = static_cast<unsigned int>(pcm.size());
    w.sampleRate = static_cast<unsigned int>(core::kSampleRate);
    w.sampleSize = 16;
    w.channels = 1;
    w.data = const_cast<int16_t*>(pcm.data());
    return LoadSoundFromWave(w);  // copies the samples, so pcm may go out of scope
}

}  // namespace

Sfx::~Sfx() { unload(); }

namespace {

// The file each cue prefers, relative to assets/audio/sfx. These are CC0, so
// unlike the sprites they can live in the repository.
//
// I cannot audition audio, so this mapping is a considered guess rather than a
// mix decision. It is deliberately a lookup by FILENAME: swapping any cue is
// dropping a different .ogg in with that name, no rebuild and no code change.
const char* fileFor(Cue c) {
    switch (c) {
        case Cue::Shoot: return "shoot.ogg";
        case Cue::Hit: return "hit.ogg";
        case Cue::Crit: return "crit.ogg";
        case Cue::Death: return "death.ogg";
        case Cue::Quake: return "quake.ogg";
        case Cue::Leak: return "leak.ogg";
        case Cue::Build: return "build.ogg";
        case Cue::Sell: return "sell.ogg";
        case Cue::Click: return "click.ogg";
        case Cue::Buy: return "buy.ogg";
        case Cue::WaveStart: return "wavestart.ogg";
        case Cue::Defeat: return "defeat.ogg";
        case Cue::Victory: return "victory.ogg";
    }
    return nullptr;
}

}  // namespace

void Sfx::load() {
    if (loaded_) return;
    const std::filesystem::path dir = core::assetDir() / "audio" / "sfx";
    int fromFile = 0, fromSynth = 0;

    for (const Cue c : {Cue::Shoot, Cue::Hit, Cue::Crit, Cue::Death, Cue::Quake, Cue::Leak,
                        Cue::Build, Cue::Sell, Cue::Click, Cue::Buy, Cue::WaveStart, Cue::Defeat,
                        Cue::Victory}) {
        Voices v;
        const char* name = fileFor(c);
        const auto path = name ? dir / name : dir;

        // Prefer the recorded sound; fall back to the synthesiser so the game
        // still has audio with no assets present.
        if (name && std::filesystem::exists(path)) {
            const Sound base = LoadSound(path.string().c_str());
            if (base.frameCount > 0) {
                v.pool.push_back(base);
                for (int i = 1; i < poolSizeFor(c); ++i) v.pool.push_back(LoadSoundAlias(base));
                v.aliased = true;
                cues_[c] = std::move(v);
                ++fromFile;
                continue;
            }
        }
        const Pcm pcm = synth(c);
        if (pcm.empty()) continue;
        for (int i = 0; i < poolSizeFor(c); ++i) v.pool.push_back(soundFrom(pcm));
        cues_[c] = std::move(v);
        ++fromSynth;
    }
    TraceLog(LOG_INFO, "sfx: %d cues from files, %d synthesised", fromFile, fromSynth);
    loaded_ = true;
}

void Sfx::unload() {
    if (!loaded_) return;
    for (auto& [cue, v] : cues_) {
        if (v.aliased) {
            // Aliases first, then the source that owns the sample data. Order is
            // not strictly required -- UnloadSoundAlias never touches the data --
            // but releasing borrowers before the owner is the honest order.
            for (size_t i = v.pool.size(); i-- > 1;) UnloadSoundAlias(v.pool[i]);
            if (!v.pool.empty()) UnloadSound(v.pool.front());
        } else {
            for (auto& s : v.pool) UnloadSound(s);
        }
    }
    cues_.clear();
    loaded_ = false;
}

void Sfx::setVolume(float v) { master_ = std::clamp(v, 0.0f, 1.0f); }

void Sfx::play(Cue cue, float pitchJitter, float volume) {
    if (!loaded_ || muted_) return;
    const auto it = cues_.find(cue);
    if (it == cues_.end() || it->second.pool.empty()) return;

    auto& v = it->second;
    Sound& s = v.pool[v.next];
    v.next = (v.next + 1) % v.pool.size();

    // A touch of pitch variation stops repeated shots sounding like a machine.
    static uint32_t seed = 12345u;
    seed = seed * 1664525u + 1013904223u;
    const float r = static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
    SetSoundPitch(s, 1.0f + r * pitchJitter);
    SetSoundVolume(s, std::clamp(volume, 0.0f, 1.0f) * master_);
    PlaySound(s);
}

void Sfx::handle(const std::vector<sim::VisualEvent>& events) {
    if (!loaded_ || muted_) return;

    // Busy waves emit dozens of events per frame. Without caps this becomes a
    // grey wall of noise, and each new voice cuts off the previous one.
    int shots = 0, hits = 0, deaths = 0, quakes = 0, leaks = 0;
    for (const auto& e : events) {
        switch (e.kind) {
            case sim::VisualEvent::Kind::Shot:
                if (shots++ < 3) play(Cue::Shoot, 0.10f, 0.34f);
                break;
            case sim::VisualEvent::Kind::Hit:
                if (e.crit) play(Cue::Crit, 0.06f, 0.5f);
                else if (hits++ < 3) play(Cue::Hit, 0.12f, 0.26f);
                break;
            case sim::VisualEvent::Kind::Death:
                if (deaths++ < 3) play(Cue::Death, 0.10f, 0.34f);
                break;
            case sim::VisualEvent::Kind::Quake:
                if (quakes++ < 1) play(Cue::Quake, 0.08f, 0.55f);
                break;
            case sim::VisualEvent::Kind::Leak:
                if (leaks++ < 1) play(Cue::Leak, 0.03f, 0.7f);
                break;
            case sim::VisualEvent::Kind::Build:
                play(Cue::Build, 0.03f, 0.6f);
                break;
        }
    }
}

}  // namespace td::audio
