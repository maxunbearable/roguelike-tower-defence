#pragma once

#include <filesystem>
#include <map>
#include <vector>
#include <string>

#include "raylib.h"

#include "content/SpriteDef.h"
#include "core/NinePatch.h"

namespace td::render {

// Uploads decoded sprites to GPU textures with point filtering. Owns them.
class SpriteAtlas {
public:
    SpriteAtlas() = default;
    ~SpriteAtlas();
    SpriteAtlas(const SpriteAtlas&) = delete;
    SpriteAtlas& operator=(const SpriteAtlas&) = delete;

    void load(const std::map<std::string, content::SpriteDef>& sprites);

    // Replaces generated sprites with hand-made PNGs, keyed by filename stem:
    // assets/sprites/tower_base.png overrides the generated "tower_base".
    // This is what lets real artwork be dropped in piece by piece, with no code
    // change, and lets a partial pack coexist with whatever is still generated.
    // Returns how many overrides were applied.
    int loadOverrides(const std::filesystem::path& dir);


    bool has(const std::string& id) const { return textures_.count(id) > 0; }
    // How many frames "<base>_0", "<base>_1", ... exist. Read from the art
    // rather than hardcoded, so adding frames is a content change.
    int frameCount(const std::string& base) const;
    std::vector<std::string> ids() const;
    const Texture2D* get(const std::string& id) const;

    // Draws with the sprite's CENTRE at (cx, cy), rounded to whole pixels so
    // sprites never land on a half-pixel and shimmer.
    void draw(const std::string& id, float cx, float cy, Color tint = WHITE) const;
    // Draws with the sprite's BOTTOM-CENTRE at (cx, by): the anchor towers use,
    // so a taller tower grows upward from its tile instead of sinking into it.
    void drawFoot(const std::string& id, float cx, float by, Color tint = WHITE) const;
    // Foot-anchored at a WHOLE-number magnification, for bosses.
    void drawFootScaled(const std::string& id, float cx, float by, int scale,
                        Color tint = WHITE) const;
    void drawRotated(const std::string& id, float cx, float cy, float degrees,
                     Color tint = WHITE) const;
    // Draws fitted inside a box, preserving aspect. The HUD needs this now that
    // sprites vary from 12px icons to 90px buildings.
    // grow=true lets a sprite smaller than the box scale UP, by an INTEGER
    // factor only -- a 12px icon in a 43px node circle otherwise stays 12px,
    // and a fractional upscale of pixel art shimmers.
    void drawFitted(const std::string& id, float cx, float cy, float maxSide,
                    Color tint = WHITE, bool grow = false) const;
    // Draws a painted panel at an arbitrary size: corners stay pixel-exact,
    // edges stretch along one axis, the centre along both. Without this a
    // panel's border scales with the box and the outline smears.
    void drawNine(const std::string& id, float x, float y, float w, float h,
                  float inset = kUiInset, Color tint = WHITE) const;

    // The pack's "9Slides" sources are a 3x3 grid of 64px cells, halved on
    // import, so every imported UI panel slices at 32.
    static constexpr float kUiInset = 32.0f;

private:
    std::map<std::string, Texture2D> textures_;
};

}  // namespace td::render
