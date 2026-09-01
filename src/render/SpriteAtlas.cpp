#include "render/SpriteAtlas.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace td::render {

SpriteAtlas::~SpriteAtlas() {
    for (auto& [id, tex] : textures_) UnloadTexture(tex);
}

void SpriteAtlas::load(const std::map<std::string, content::SpriteDef>& sprites) {
    for (const auto& [id, sp] : sprites) {
        // SpriteDef stores 0xRRGGBBAA; raylib wants little-endian RGBA bytes.
        std::vector<unsigned char> bytes;
        bytes.reserve(sp.pixels.size() * 4);
        for (const uint32_t px : sp.pixels) {
            bytes.push_back(static_cast<unsigned char>((px >> 24) & 0xFF));
            bytes.push_back(static_cast<unsigned char>((px >> 16) & 0xFF));
            bytes.push_back(static_cast<unsigned char>((px >> 8) & 0xFF));
            bytes.push_back(static_cast<unsigned char>(px & 0xFF));
        }
        Image img{bytes.data(), sp.w, sp.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Texture2D tex = LoadTextureFromImage(img);
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        textures_[id] = tex;
    }
}

int SpriteAtlas::frameCount(const std::string& base) const {
    int n = 0;
    while (textures_.count(base + "_" + std::to_string(n)) > 0) ++n;
    return n;
}

int SpriteAtlas::loadOverrides(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return 0;

    int applied = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext != ".png" && ext != ".PNG") continue;

        const std::string id = entry.path().stem().string();
        Texture2D tex = LoadTexture(entry.path().string().c_str());
        if (tex.id == 0) {
            TraceLog(LOG_WARNING, "art: could not load override %s",
                     entry.path().string().c_str());
            continue;
        }
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);

        const auto it = textures_.find(id);
        if (it != textures_.end()) {
            UnloadTexture(it->second);   // the generated sprite is replaced
            it->second = tex;
        } else {
            textures_[id] = tex;         // or it is simply new art
        }
        ++applied;
    }
    if (applied > 0) TraceLog(LOG_INFO, "art: %d sprite override(s) loaded", applied);
    return applied;
}

std::vector<std::string> SpriteAtlas::ids() const {
    std::vector<std::string> out;
    out.reserve(textures_.size());
    for (const auto& [id, tex] : textures_) out.push_back(id);
    return out;
}

const Texture2D* SpriteAtlas::get(const std::string& id) const {
    const auto it = textures_.find(id);
    return it == textures_.end() ? nullptr : &it->second;
}

void SpriteAtlas::draw(const std::string& id, float cx, float cy, Color tint) const {
    const auto* t = get(id);
    if (!t) return;
    DrawTexture(*t, static_cast<int>(std::lround(cx - t->width * 0.5f)),
                static_cast<int>(std::lround(cy - t->height * 0.5f)), tint);
}

void SpriteAtlas::drawFoot(const std::string& id, float cx, float by, Color tint,
                           bool flipX) const {
    const auto* t = get(id);
    if (!t) return;
    const float w = static_cast<float>(t->width);
    const float h = static_cast<float>(t->height);
    // A negative source WIDTH is how raylib mirrors; it keeps the destination
    // rectangle and therefore the foot anchor unchanged.
    const Rectangle src{0, 0, flipX ? -w : w, h};
    const Rectangle dst{std::round(cx - w * 0.5f), std::round(by - h), w, h};
    DrawTexturePro(*t, src, dst, Vector2{0, 0}, 0.0f, tint);
}

void SpriteAtlas::drawFitted(const std::string& id, float cx, float cy, float maxSide,
                             Color tint, bool grow) const {
    const auto* t = get(id);
    if (!t) return;
    const float longest = static_cast<float>(std::max(t->width, t->height));
    float k = longest > maxSide ? maxSide / longest : 1.0f;
    if (grow && longest > 0.0f && longest < maxSide) {
        k = std::max(1.0f, std::floor(maxSide / longest));
    }
    const float w = t->width * k, h = t->height * k;
    const Rectangle src{0, 0, static_cast<float>(t->width), static_cast<float>(t->height)};
    const Rectangle dst{std::round(cx - w * 0.5f), std::round(cy - h * 0.5f), w, h};
    DrawTexturePro(*t, src, dst, Vector2{0, 0}, 0.0f, tint);
}

void SpriteAtlas::drawFootScaled(const std::string& id, float cx, float by, int scale,
                                 Color tint, bool flipX) const {
    const auto* t = get(id);
    if (!t) return;
    if (scale <= 1) {
        drawFoot(id, cx, by, tint, flipX);
        return;
    }
    const float w = static_cast<float>(t->width * scale);
    const float h = static_cast<float>(t->height * scale);
    const float sw = static_cast<float>(t->width);
    const Rectangle src{0, 0, flipX ? -sw : sw, static_cast<float>(t->height)};
    const Rectangle dst{std::round(cx - w * 0.5f), std::round(by - h), w, h};
    DrawTexturePro(*t, src, dst, Vector2{0, 0}, 0.0f, tint);
}

void SpriteAtlas::drawRotated(const std::string& id, float cx, float cy, float degrees,
                              Color tint) const {
    const auto* t = get(id);
    if (!t) return;
    const Rectangle src{0, 0, static_cast<float>(t->width), static_cast<float>(t->height)};
    const Rectangle dst{std::round(cx), std::round(cy), static_cast<float>(t->width),
                        static_cast<float>(t->height)};
    const Vector2 origin{t->width * 0.5f, t->height * 0.5f};
    DrawTexturePro(*t, src, dst, origin, degrees, tint);
}

void SpriteAtlas::drawNine(const std::string& id, float x, float y, float w, float h,
                           float inset, Color tint) const {
    const auto* tex = get(id);
    if (!tex) return;
    const auto quads = core::ninePatch(static_cast<float>(tex->width),
                                       static_cast<float>(tex->height), inset,
                                       std::round(x), std::round(y), std::round(w),
                                       std::round(h));
    for (size_t i = 0; i < quads.size(); ++i) {
        const auto& q = quads[i];
        if (q.dst.w <= 0.0f || q.dst.h <= 0.0f) continue;
        if (q.src.w <= 0.0f || q.src.h <= 0.0f) continue;

        // Corners draw once at source size; everything else tiles. Scaling a
        // patterned cell to panel size turns the parchment hatching into giant
        // blocks, which is exactly what it looked like.
        const bool corner = (i == 0 || i == 2 || i == 6 || i == 8);
        if (corner) {
            DrawTexturePro(*tex, Rectangle{q.src.x, q.src.y, q.src.w, q.src.h},
                           Rectangle{q.dst.x, q.dst.y, q.dst.w, q.dst.h}, Vector2{0, 0}, 0.0f,
                           tint);
            continue;
        }
        for (const auto& [ox, tw] : core::tileRuns(q.src.w, q.dst.w)) {
            for (const auto& [oy, th] : core::tileRuns(q.src.h, q.dst.h)) {
                DrawTexturePro(*tex, Rectangle{q.src.x, q.src.y, tw, th},
                               Rectangle{q.dst.x + ox, q.dst.y + oy, tw, th}, Vector2{0, 0},
                               0.0f, tint);
            }
        }
    }
}

}  // namespace td::render
