#include "render/TileGen.h"

#include <cmath>
#include <cstdint>

#include "render/Palette.h"

namespace td::render {
namespace {

// Cheap deterministic hash. Not high quality, but stable across platforms and
// runs, which is all terrain detail needs.
uint32_t hash2(uint32_t x, uint32_t y, uint32_t salt) {
    uint32_t h = x * 374761393u + y * 668265263u + salt * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float unit(uint32_t h) { return static_cast<float>(h & 0xFFFFFF) / 16777216.0f; }

Color mix(Color a, Color b, float t) {
    return Color{static_cast<unsigned char>(a.r + (b.r - a.r) * t),
                 static_cast<unsigned char>(a.g + (b.g - a.g) * t),
                 static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255};
}

void put(Image& img, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    ImageDrawPixel(&img, x, y, c);
}

Texture2D finish(Image& img) {
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return t;
}

// Measured against a reference tileset: its grass is #7bad2c, which is far
// brighter and more saturated than the near-olive this used to be. A dark
// board makes every sprite on it look dirty.
// Sampled directly from the imported ground tiles, so the generated road
// fringes sit in the same palette as the art they overlay. Guessing these is
// what made the transitions read as a different green laid on top.
const Color kGrassDark{128, 172, 94, 255};
const Color kGrassMid{155, 185, 78, 255};
const Color kGrassLit{184, 185, 88, 255};
const Color kGrassHi{198, 205, 110, 255};

const Color kDirtDark{203, 152, 104, 255};
const Color kDirtMid{209, 156, 108, 255};
const Color kDirtLit{213, 160, 110, 255};

Image makeGrass(int s, uint32_t variant) {
    // Each variant carries a different BASE tone, not just different noise.
    // Per-pixel noise alone averages out to one flat colour at a distance; it is
    // the tonal difference between variants that creates visible patches of
    // meadow and makes the field stop looking like painted cardboard.
    // Kept SUBTLE on purpose. A strong tonal spread turns a 4-tile set into a
    // visible checkerboard, which looks worse than flat colour. The variation
    // has to be barely perceptible per tile and only register as texture across
    // the whole field.
    const float lean = (static_cast<float>(variant % 6) / 5.0f - 0.5f) * 0.14f;
    const Color base = lean < 0.0f ? mix(kGrassMid, kGrassDark, -lean * 2.0f)
                                   : mix(kGrassMid, kGrassLit, lean * 2.0f);

    Image img = GenImageColor(s, s, base);
    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const float n = unit(hash2(static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                                       variant * 7919u + 11u));
            if (n > 0.86f) put(img, x, y, mix(base, kGrassLit, 0.7f));
            else if (n < 0.14f) put(img, x, y, mix(base, kGrassDark, 0.7f));
        }
    }
    // Upright tufts give the ground a direction and a sense of scale.
    const int tufts = 3 + static_cast<int>(unit(hash2(variant, 5u, 3u)) * 3.0f);
    for (int i = 0; i < tufts; ++i) {
        const uint32_t h = hash2(variant, static_cast<uint32_t>(i), 91u);
        const int tx = 2 + static_cast<int>(unit(h) * (s - 5));
        const int ty = 3 + static_cast<int>(unit(h >> 8) * (s - 7));
        put(img, tx, ty - 1, kGrassHi);
        put(img, tx, ty, kGrassHi);
        put(img, tx - 1, ty, kGrassLit);
        put(img, tx + 1, ty, kGrassLit);
        put(img, tx - 1, ty + 1, kGrassDark);
        put(img, tx + 1, ty + 1, kGrassDark);
    }
    return img;
}

Image makeDirt(int s, uint32_t variant) {
    Image img = GenImageColor(s, s, kDirtMid);
    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            const float n = unit(hash2(static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                                       variant * 6151u + 29u));
            if (n > 0.88f) put(img, x, y, kDirtLit);
            else if (n < 0.16f) put(img, x, y, kDirtDark);
        }
    }
    // Scattered pebbles so the road reads as trodden ground, not flat paint.
    const int stones = 1 + static_cast<int>(unit(hash2(variant, 2u, 17u)) * 3.0f);
    for (int i = 0; i < stones; ++i) {
        const uint32_t h = hash2(variant, static_cast<uint32_t>(i), 313u);
        const int sx = 3 + static_cast<int>(unit(h) * (s - 7));
        const int sy = 3 + static_cast<int>(unit(h >> 9) * (s - 7));
        put(img, sx, sy, kDirtLit);
        put(img, sx + 1, sy, kDirtLit);
        put(img, sx, sy + 1, kDirtDark);
        put(img, sx + 1, sy + 1, kDirtDark);
    }
    return img;
}

}  // namespace

TileSet::~TileSet() {
    if (!loaded_) return;
    for (auto& t : grass_) UnloadTexture(t);
    for (auto& t : dirt_) UnloadTexture(t);
    UnloadTexture(spawn_);
    UnloadTexture(exit_);
    UnloadTexture(edge_);
    UnloadTexture(outer_);
    UnloadTexture(inner_);
}

void TileSet::generate(int s) {
    for (uint32_t v = 0; v < 8; ++v) {
        Image g = makeGrass(s, v);
        grass_.push_back(finish(g));
    }
    for (uint32_t v = 0; v < 5; ++v) {
        Image d = makeDirt(s, v);
        dirt_.push_back(finish(d));
    }

    // Spawn: a stone-ringed mouth the enemies pour out of.
    {
        Image img = makeDirt(s, 0);
        const Color stone{92, 84, 78, 255};
        const Color stoneLit{132, 122, 112, 255};
        for (int y = 0; y < s; ++y) {
            for (int x = 0; x < s; ++x) {
                const float dx = (x - s * 0.5f) / (s * 0.44f);
                const float dy = (y - s * 0.5f) / (s * 0.44f);
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d < 0.72f) {
                    put(img, x, y, mix(Color{16, 10, 20, 255}, Color{54, 30, 38, 255},
                                       d / 0.72f));
                } else if (d < 1.0f) {
                    put(img, x, y, (x + y) % 3 == 0 ? stoneLit : stone);
                }
            }
        }
        spawn_ = finish(img);
    }
    // Exit: a stone-lined goal.
    {
        Image img = makeDirt(s, 1);
        for (int y = 0; y < s; ++y) {
            for (int x = 0; x < s; ++x) {
                const float dx = (x - s * 0.5f) / (s * 0.42f);
                const float dy = (y - s * 0.5f) / (s * 0.42f);
                const float d = dx * dx + dy * dy;
                if (d < 1.0f) put(img, x, y, mix(Color{74, 143, 196, 255}, Color{43, 95, 138, 255}, d));
            }
        }
        const Color stone{90, 96, 114, 255};
        for (int i = 0; i < s; ++i) {
            put(img, i, 0, stone);
            put(img, i, s - 1, stone);
            put(img, 0, i, stone);
            put(img, s - 1, i, stone);
        }
        exit_ = finish(img);
    }
    // --- transition pieces, all authored for the TOP edge and rotated -----
    const Color lip = kGrassMid;
    const Color lipDark = kGrassDark;
    const Color lipLit = kGrassLit;

    auto grassNoise = [&](Image& img, int x, int y) {
        const float n = unit(hash2(static_cast<uint32_t>(x), static_cast<uint32_t>(y), 4242u));
        put(img, x, y, n > 0.75f ? lipLit : (n < 0.25f ? lipDark : lip));
    };

    {   // straight edge: an irregular grass fringe hanging into the road
        Image img = GenImageColor(s, s, BLANK);
        for (int x = 0; x < s; ++x) {
            const float n = unit(hash2(static_cast<uint32_t>(x), 0u, 77u));
            // Kept shallow: a deep fringe narrows the road until it stops
            // reading as something an army would march down.
            const int depth = 2 + static_cast<int>(n * 3.0f);
            for (int y = 0; y < depth; ++y) grassNoise(img, x, y);
            put(img, x, depth, Color{0, 0, 0, 60});      // contact shadow under the lip
            put(img, x, depth + 1, Color{0, 0, 0, 30});
        }
        edge_ = finish(img);
    }
    {   // outer corner: grass wrapping around the corner of the road
        Image img = GenImageColor(s, s, BLANK);
        for (int y = 0; y < s; ++y) {
            for (int x = 0; x < s; ++x) {
                const float n = unit(hash2(static_cast<uint32_t>(x), static_cast<uint32_t>(y), 91u));
                const float r = std::sqrt(static_cast<float>(x * x + y * y));
                const float edge = s * 0.42f + n * 2.5f;
                if (r < edge) grassNoise(img, x, y);
                else if (r < edge + 1.6f) put(img, x, y, Color{0, 0, 0, 55});
            }
        }
        outer_ = finish(img);
    }
    {   // inner corner: a small grass nub where two road edges meet
        Image img = GenImageColor(s, s, BLANK);
        for (int y = 0; y < s; ++y) {
            for (int x = 0; x < s; ++x) {
                const float n = unit(hash2(static_cast<uint32_t>(x), static_cast<uint32_t>(y), 55u));
                const float dx = static_cast<float>(x);
                const float dy = static_cast<float>(y);
                const float r = std::sqrt(dx * dx + dy * dy);
                if (r < s * 0.30f + n * 2.5f) grassNoise(img, x, y);
            }
        }
        inner_ = finish(img);
    }

    loaded_ = true;
}

const Texture2D& TileSet::grass(int tx, int ty) const {
    const uint32_t h = hash2(static_cast<uint32_t>(tx + 1000), static_cast<uint32_t>(ty + 1000), 1u);
    return grass_[h % grass_.size()];
}

bool TileSet::flipX(int tx, int ty) {
    return (hash2(static_cast<uint32_t>(tx + 500), static_cast<uint32_t>(ty + 500), 7u) & 1u) != 0u;
}
bool TileSet::flipY(int tx, int ty) {
    return (hash2(static_cast<uint32_t>(tx + 500), static_cast<uint32_t>(ty + 500), 13u) & 1u) != 0u;
}

const Texture2D& TileSet::dirt(int tx, int ty) const {
    const uint32_t h = hash2(static_cast<uint32_t>(tx + 1000), static_cast<uint32_t>(ty + 1000), 2u);
    return dirt_[h % dirt_.size()];
}

}  // namespace td::render
