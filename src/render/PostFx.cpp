#include "render/PostFx.h"

#include <algorithm>
#include <cstdlib>

#include "render/PixelCanvas.h"

namespace td::render {
namespace {

// Tuned by looking. The field has to read as evening rather than midday without
// going so dark that the art turns to mud -- the first pass sat at ambient 0.58
// with a 0.52 vignette on top, which compounded into "too dark to enjoy".
constexpr float kDesat = 0.20f;
constexpr float kCool = 0.42f;
constexpr float kVignette = 0.26f;
constexpr float kAmbientBase = 0.94f;
// The pools are decoration, not visibility: the ambient barely dims the field,
// so a strong gain here only clips. Was effectively 1.0, which is what let a
// cluster of towers wash out.
constexpr float kLightGain = 0.34f;

// Tuning hook: these four can be overridden from the environment so the look can
// be swept without a rebuild. Read once. Absent variables leave the defaults.
float tunable(const char* name, float fallback) {
    if (const char* v = std::getenv(name)) {
        const float f = std::strtof(v, nullptr);
        if (f > 0.0f) return f;
    }
    return fallback;
}

}  // namespace

void PostFx::load(const std::filesystem::path& shaderDir) {
    const auto fs = shaderDir / "atmosphere.fs";
    if (!std::filesystem::exists(fs)) {
        TraceLog(LOG_WARNING, "postfx: %s missing, atmosphere disabled", fs.string().c_str());
        return;
    }
    // Null vertex shader: raylib substitutes its default, which is what a
    // full-screen 2D pass wants.
    shader_ = LoadShader(nullptr, fs.string().c_str());
    if (shader_.id == 0) {
        TraceLog(LOG_WARNING, "postfx: atmosphere shader failed to compile");
        return;
    }

    locHudFrac_ = GetShaderLocation(shader_, "uHudFrac");
    locVirtual_ = GetShaderLocation(shader_, "uVirtual");
    locDesat_ = GetShaderLocation(shader_, "uDesat");
    locCool_ = GetShaderLocation(shader_, "uCool");
    locVignette_ = GetShaderLocation(shader_, "uVignette");
    locAmbient_ = GetShaderLocation(shader_, "uAmbient");
    locGain_ = GetShaderLocation(shader_, "uLightGain");
    locCount_ = GetShaderLocation(shader_, "uLightCount");
    locPos_ = GetShaderLocation(shader_, "uLightPos");
    locColor_ = GetShaderLocation(shader_, "uLightColor");
    locRadius_ = GetShaderLocation(shader_, "uLightRadius");
    ready_ = true;
}

void PostFx::unload() {
    if (ready_) UnloadShader(shader_);
    ready_ = false;
}

void PostFx::setLights(std::vector<Light> lights) {
    if (static_cast<int>(lights.size()) > kMaxLights) lights.resize(kMaxLights);
    lights_ = std::move(lights);
}

void PostFx::beginPass() {
    if (!ready_ || !enabled_) return;

    const float hudFrac = static_cast<float>(kHudH) / static_cast<float>(kVirtualH);
    const float virt[2] = {static_cast<float>(kVirtualW), static_cast<float>(kVirtualH)};
    SetShaderValue(shader_, locHudFrac_, &hudFrac, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locVirtual_, virt, SHADER_UNIFORM_VEC2);
    static const float desat = tunable("TD_FX_DESAT", kDesat);
    static const float cool = tunable("TD_FX_COOL", kCool);
    static const float vignette = tunable("TD_FX_VIGNETTE", kVignette);
    static const float amb = tunable("TD_FX_AMBIENT", kAmbientBase);
    static const float gain = tunable("TD_FX_LIGHTGAIN", kLightGain);
    // Slightly cooler in the blue channel, so the darkening reads as dusk light
    // rather than a flat grey filter.
    const float ambient[3] = {amb, std::min(1.0f, amb * 1.04f),
                              std::min(1.0f, amb * 1.12f)};
    SetShaderValue(shader_, locDesat_, &desat, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locCool_, &cool, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locVignette_, &vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locAmbient_, ambient, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locGain_, &gain, SHADER_UNIFORM_FLOAT);

    const int count = static_cast<int>(lights_.size());
    SetShaderValue(shader_, locCount_, &count, SHADER_UNIFORM_INT);
    if (count > 0) {
        float pos[kMaxLights * 2] = {};
        float col[kMaxLights * 3] = {};
        float rad[kMaxLights] = {};
        for (int i = 0; i < count; ++i) {
            pos[i * 2 + 0] = lights_[static_cast<size_t>(i)].pos.x;
            pos[i * 2 + 1] = lights_[static_cast<size_t>(i)].pos.y;
            const Color c = lights_[static_cast<size_t>(i)].color;
            col[i * 3 + 0] = c.r / 255.0f;
            col[i * 3 + 1] = c.g / 255.0f;
            col[i * 3 + 2] = c.b / 255.0f;
            rad[i] = lights_[static_cast<size_t>(i)].radius;
        }
        SetShaderValueV(shader_, locPos_, pos, SHADER_UNIFORM_VEC2, count);
        SetShaderValueV(shader_, locColor_, col, SHADER_UNIFORM_VEC3, count);
        SetShaderValueV(shader_, locRadius_, rad, SHADER_UNIFORM_FLOAT, count);
    }

    BeginShaderMode(shader_);
}

void PostFx::endPass() {
    if (!ready_ || !enabled_) return;
    EndShaderMode();
}

}  // namespace td::render
