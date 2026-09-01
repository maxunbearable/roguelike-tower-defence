#pragma once

#include <filesystem>
#include <vector>

#include "raylib.h"

#include "core/Vec2.h"

namespace td::render {

// The atmosphere pass. Wraps the canvas blit in a fragment shader that grades
// and darkens the play field and lights it back up around the towers.
//
// Applied at blit time rather than inside the virtual canvas because raylib
// cannot nest render-texture modes, and the HUD is drawn into the same canvas.
// The shader masks the HUD band out by texture coordinate instead.
class PostFx {
public:
    // A warm pool of light. Positions are in VIRTUAL pixels, matching the
    // coordinate space the renderer already draws towers in.
    struct Light {
        core::Vec2 pos;
        Color color{255, 214, 150, 255};
        float radius = 150.0f;
    };

    static constexpr int kMaxLights = 12;

    // Silently leaves the effect disabled if the shader is missing or fails to
    // compile, so the game still runs and still looks like itself.
    void load(const std::filesystem::path& shaderDir);
    void unload();
    bool ready() const { return ready_; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool on) { enabled_ = on; }

    void setLights(std::vector<Light> lights);

    // Bracket the canvas blit with these. Both are no-ops when disabled, so the
    // caller does not branch.
    void beginPass();
    void endPass();

private:
    Shader shader_{};
    bool ready_ = false;
    bool enabled_ = true;
    std::vector<Light> lights_;

    int locHudFrac_ = -1, locVirtual_ = -1, locDesat_ = -1, locCool_ = -1;
    int locVignette_ = -1, locGain_ = -1;
    int locAmbient_ = -1, locCount_ = -1, locPos_ = -1, locColor_ = -1, locRadius_ = -1;
};

}  // namespace td::render
