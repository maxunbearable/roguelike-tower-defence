#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "content/SpriteLoader.h"
#include "render/Palette.h"
#include "render/PixelCanvas.h"

namespace td::render {
namespace {

constexpr float kFootDrop = 22.0f;   // how far below tile centre a tower stands
// The imported creature frames end in a ~6px baked shadow ellipse, so its
// centre sits about 3px above the art's bottom edge.
constexpr float kEnemyShadowInset = 3.0f;
constexpr float kCrownLift = 36.0f;  // the parapet the garrison stands on

// The art pack supplies WHOLE BUILDINGS, so each specialisation is a single
// sprite with its own silhouette and faction colour rather than a shared base
// plus a crown. Unspecialised towers get their own building too, so a tower
// never looks half-finished.
// Falls back down a chain rather than returning a missing id: with three tower
// types and nine specs, art lands piece by piece, and an unmatched id would draw
// a tower as nothing at all.
std::string towerSpriteFor(const SpriteAtlas& atlas, const std::string& towerId,
                           const std::string& spec) {
    if (!spec.empty() && atlas.has("tower_" + spec)) return "tower_" + spec;
    if (atlas.has("tower_" + towerId)) return "tower_" + towerId;
    return "tower_plain";
}

// The element grows ON the tower -- vines, crystals, runes -- rather than
// sitting beside it as a bead. Before a power is chosen there is nothing to
// show yet.
// Prefers per-SPEC overlay art, falling back to per-ELEMENT. Only earth's three
// specs have their own; for the rest the element is the readable fact anyway, and
// the spec is named on the tower's stats panel.
std::string enchantFor(const SpriteAtlas& atlas, const std::string& elementId,
                       const std::string& spec) {
    if (elementId.empty() || spec.empty()) return {};
    if (atlas.has("enchant_" + spec)) return "enchant_" + spec;
    if (atlas.has("enchant_" + elementId)) return "enchant_" + elementId;
    return {};
}

// Draws a tile, optionally mirrored, using a negative source rect.
void blitTile(const Texture2D& t, int px, int py, bool fx, bool fy) {
    const Rectangle src{0.0f, 0.0f, fx ? -static_cast<float>(t.width) : static_cast<float>(t.width),
                        fy ? -static_cast<float>(t.height) : static_cast<float>(t.height)};
    const Rectangle dst{static_cast<float>(px), static_cast<float>(py),
                        static_cast<float>(t.width), static_cast<float>(t.height)};
    DrawTexturePro(t, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

bool isPath(const content::MapDef& m, int x, int y) {
    const char c = m.tileAt(x, y);
    return c == '=' || c == 'S' || c == 'E';
}

}  // namespace

void Renderer::load(const content::Registry& reg) {
    (void)reg;
    atlas_.load(content::loadSprites(std::filesystem::path(TD_CONTENT_DIR) / "art" /
                                     "sprites.toml"));
    // Hand-made art wins over generated art, per sprite. Drop a PNG named after
    // any sprite id into assets/sprites/ and it takes over.
    atlas_.loadOverrides(std::filesystem::path(TD_ASSET_DIR) / "sprites");
    tiles_.generate(kTile);
}

void Renderer::update(float dt, const std::vector<sim::VisualEvent>& events,
                      const content::Registry& reg) {
    time_ += dt;
    fx_.consume(events);
    fx_.update(dt);

    // A death used to be particles only: the sprite vanished on the same frame it
    // died, so kills read as a puff with nothing dying in it. Keep a copy of the
    // sprite around to fall over and fade.
    for (const auto& e : events) {
        if (e.kind != sim::VisualEvent::Kind::Death) continue;
        if (!reg.hasEnemy(e.tag)) continue;
        const auto& def = reg.enemy(e.tag);
        Corpse c;
        c.pos = e.pos;
        // The monster pack ships an 18-frame Dying sequence per creature, cut to
        // 6 by tools/import_monsters.py. Prefer it; fall back to holding frame 0
        // of the walk cycle for the creatures that have no death art, which
        // still sinks and fades rather than vanishing.
        const std::string die = def.spriteId() + "_die";
        const int dieFrames = atlas_.frameCount(die);
        c.base = dieFrames > 0 ? die : def.spriteId();
        c.frames = dieFrames > 0 ? dieFrames : 1;
        c.maxLife = dieFrames > 0 ? 0.6f : 0.45f;
        c.scale = def.spriteScale;
        c.faceLeft = e.dir.x < 0.0f;
        c.tint = Color{static_cast<unsigned char>(def.tintR),
                       static_cast<unsigned char>(def.tintG),
                       static_cast<unsigned char>(def.tintB), 255};
        corpses_.push_back(c);
    }
    for (auto& c : corpses_) c.life += dt;
    corpses_.erase(std::remove_if(corpses_.begin(), corpses_.end(),
                                  [](const Corpse& c) { return c.life >= c.maxLife; }),
                   corpses_.end());
}

void Renderer::drawCorpses() const {
    for (const auto& c : corpses_) {
        const float t = std::clamp(c.life / c.maxLife, 0.0f, 1.0f);
        // Sink into the ground, drain to a bruised grey and fade. Sinking rather
        // than only fading is what makes it read as dying instead of teleporting.
        const float sink = (c.frames > 1 ? 1.5f : 6.0f) * t * static_cast<float>(c.scale);
        const unsigned char a = static_cast<unsigned char>(255.0f * (1.0f - t * t));
        const float drain = 1.0f - 0.45f * t;
        const Color tint{static_cast<unsigned char>(c.tint.r * drain),
                         static_cast<unsigned char>(c.tint.g * drain),
                         static_cast<unsigned char>(c.tint.b * drain), a};
        // Play the sequence once and hold the last frame; looping a death reads
        // as the creature dying repeatedly.
        const int f = std::min(c.frames - 1, static_cast<int>(t * static_cast<float>(c.frames)));
        atlas_.drawFootScaled(c.base + "_" + std::to_string(f), c.pos.x * kTile,
                              c.pos.y * kTile + sink, c.scale, tint, c.faceLeft);
    }
}

void Renderer::drawTerrain(const content::MapDef& m) const {
    for (int y = 0; y < m.gridH; ++y) {
        for (int x = 0; x < m.gridW; ++x) {
            const int px = x * kTile, py = y * kTile;
            const char c = m.tileAt(x, y);
            const bool fx = TileSet::flipX(x, y);
            const bool fy = TileSet::flipY(x, y);
            const bool road = (c == '=' || c == 'S' || c == 'E');

            // Imported ground wins over the generated ground, when present.
            const int nGrass = atlas_.frameCount("ground_grass");
            const int nDirt = atlas_.frameCount("ground_dirt");
            const int pick = std::abs((x * 31 + y * 17));
            const Texture2D* imported = nullptr;
            if (road && nDirt > 0) {
                imported = atlas_.get("ground_dirt_" + std::to_string(pick % nDirt));
            } else if (!road && nGrass > 0) {
                imported = atlas_.get("ground_grass_" + std::to_string(pick % nGrass));
            }
            if (imported) {
                blitTile(*imported, px, py, fx, fy);
            } else if (c == 'S') {
                DrawTexture(tiles_.spawn(), px, py, WHITE);
            } else if (c == 'E') {
                DrawTexture(tiles_.exitTile(), px, py, WHITE);
            } else if (road) {
                blitTile(tiles_.dirt(x, y), px, py, fx, fy);
            } else {
                blitTile(tiles_.grass(x, y), px, py, fx, fy);
            }
        }
    }

    // Autotiled transitions. Every piece is authored for the TOP or TOP-LEFT
    // and rotated into place, which covers all 47 blob cases with three
    // textures instead of forty-seven. Grass lipping over the road with real
    // corners is the difference between a road and a row of brown squares.
    const auto blit = [&](const Texture2D& t, int px, int py, float rot) {
        const Rectangle src{0, 0, static_cast<float>(t.width), static_cast<float>(t.height)};
        const Rectangle dst{static_cast<float>(px) + kTile * 0.5f,
                            static_cast<float>(py) + kTile * 0.5f,
                            static_cast<float>(kTile), static_cast<float>(kTile)};
        DrawTexturePro(t, src, dst, Vector2{kTile * 0.5f, kTile * 0.5f}, rot, WHITE);
    };

    for (int y = 0; y < m.gridH; ++y) {
        for (int x = 0; x < m.gridW; ++x) {
            if (!isPath(m, x, y)) continue;
            const int px = x * kTile, py = y * kTile;

            const bool n = isPath(m, x, y - 1), e = isPath(m, x + 1, y);
            const bool so = isPath(m, x, y + 1), w2 = isPath(m, x - 1, y);

            // Straight fringes on every side that meets grass.
            if (!n) blit(tiles_.edgeLip(), px, py, 0.0f);
            if (!e) blit(tiles_.edgeLip(), px, py, 90.0f);
            if (!so) blit(tiles_.edgeLip(), px, py, 180.0f);
            if (!w2) blit(tiles_.edgeLip(), px, py, 270.0f);

            // Outer corners where two fringes meet, so the road rounds off.
            if (!n && !w2) blit(tiles_.outerCorner(), px, py, 0.0f);
            if (!n && !e) blit(tiles_.outerCorner(), px, py, 90.0f);
            if (!so && !e) blit(tiles_.outerCorner(), px, py, 180.0f);
            if (!so && !w2) blit(tiles_.outerCorner(), px, py, 270.0f);

            // Inner corners: both sides are road but the diagonal is not, so a
            // nub of grass pokes into the bend. Without these, corners look cut
            // with scissors.
            if (n && w2 && !isPath(m, x - 1, y - 1)) blit(tiles_.innerCorner(), px, py, 0.0f);
            if (n && e && !isPath(m, x + 1, y - 1)) blit(tiles_.innerCorner(), px, py, 90.0f);
            if (so && e && !isPath(m, x + 1, y + 1)) blit(tiles_.innerCorner(), px, py, 180.0f);
            if (so && w2 && !isPath(m, x - 1, y + 1)) blit(tiles_.innerCorner(), px, py, 270.0f);
        }
    }
}

// Scenery scattered by world coordinate rather than baked into tiles, so the
// decoration carries no trace of the tile grid.
// The castle the enemies are marching on. Drawn behind everything else so
// units pass in front of it.
void Renderer::drawGoal(const content::MapDef& m) const {
    if (!atlas_.has("goal_castle") || m.pathWaypoints.empty()) return;
    const auto& e = m.pathWaypoints.back();
    atlas_.drawFoot("goal_castle", (e.x + 0.5f) * kTile, (e.y + 1.2f) * kTile);
}

namespace {

uint32_t hash2(int x, int y, uint32_t salt) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
                 salt;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float unitHash(int x, int y, uint32_t salt) {
    return static_cast<float>(hash2(x, y, salt) & 0xFFFFu) / 65535.0f;
}

// Smoothed value noise on the tile grid. Bilinear between lattice points, so
// the patches come out as soft blobs rather than a checkerboard of tiles.
float patchNoise(float x, float y, uint32_t salt) {
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float a = unitHash(x0, y0, salt), b = unitHash(x0 + 1, y0, salt);
    const float c = unitHash(x0, y0 + 1, salt), d = unitHash(x0 + 1, y0 + 1, salt);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

}  // namespace

// Patches of darker growth over the grass. The field was one flat tone across
// 22x11 tiles, which is what made it read as a lawn rather than a place.
//
// Drawn on a SUB-TILE cell grid. The first attempt shaded whole 64px tiles and
// the result was a visible checkerboard -- the same tile-lattice artefact the
// baked-in flowers produced. At 8px the noise gradient carries the edge instead.
void Renderer::drawGroundVariation(const content::MapDef& m) const {
    constexpr int kCell = 8;
    constexpr int kPerTile = kTile / kCell;
    for (int cy = 0; cy < m.gridH * kPerTile; ++cy) {
        for (int cx = 0; cx < m.gridW * kPerTile; ++cx) {
            if (m.tileAt(cx / kPerTile, cy / kPerTile) != '.') continue;  // grass only
            // Broad regions plus finer mottling, in CELL space so the lattice
            // spacing is several tiles wide.
            const float fx = static_cast<float>(cx), fy = static_cast<float>(cy);
            const float n = patchNoise(fx * 0.032f, fy * 0.032f, 7u) * 0.66f +
                            patchNoise(fx * 0.098f, fy * 0.098f, 23u) * 0.34f;
            if (n <= 0.46f) continue;
            const float k = std::clamp((n - 0.46f) / 0.30f, 0.0f, 1.0f);
            const auto a = static_cast<unsigned char>(78.0f * k);
            if (a == 0) continue;
            DrawRectangle(cx * kCell, cy * kCell, kCell, kCell, Color{34, 58, 38, a});
        }
    }
}

void Renderer::drawProps(const content::MapDef& m) const {
    static const char* kProps[] = {"prop_bush", "prop_rock",     "prop_flowers",
                                   "prop_stump", "prop_mushroom", "prop_bones"};
    constexpr uint32_t kPropCount = 6;
    // Scenery grows in thickets, not on an even grid. An independent 1-in-10
    // roll per tile produced a uniform sprinkle that read as debris; clustering
    // means open ground and dense corners, which is what makes a field look
    // like terrain.
    for (int y = 0; y < m.gridH; ++y) {
        for (int x = 0; x < m.gridW; ++x) {
            if (m.tileAt(x, y) != '.') continue;  // only bare grass
            const uint32_t h = hash2(x, y, 99u);

            // Thickets follow the same growth noise as the ground patches, so
            // the dense scenery sits on the dark ground rather than fighting it.
            const float growth = patchNoise(static_cast<float>(x * 8) * 0.032f,
                                            static_cast<float>(y * 8) * 0.032f, 7u);
            const uint32_t chance = growth > 0.55f ? 108u : (growth > 0.40f ? 46u : 14u);
            if ((h & 0xFFu) > chance) continue;

            const int which = static_cast<int>(((h >> 8) & 7u) % kPropCount);
            const float ox = static_cast<float>((h >> 10) & 15u) - 7.5f;
            const float oy = static_cast<float>((h >> 14) & 15u) - 7.5f;
            // The prop art carries its own shadow, so it is foot-anchored like
            // the creatures rather than centred with an ellipse drawn under it.
            // Still dimmed slightly: scenery must never compete with a monster.
            atlas_.drawFoot(kProps[which], (x + 0.5f) * kTile + ox,
                            (y + 0.5f) * kTile + oy + 2.0f,
                            Color{222, 228, 222, 255});
        }
    }
}

void Renderer::drawTowers(const sim::World& w) const {
    w.reg().view<const sim::Position, const sim::TowerTag, const sim::Cooldown,
                 const sim::TowerStats>()
        .each([&](const sim::Position& pos, const sim::TowerTag& tag, const sim::Cooldown& cd,
                  const sim::TowerStats& st) {
            const float cx = pos.v.x * kTile;
            const float footY = pos.v.y * kTile + kFootDrop;

            // Recoil, derived from the cooldown that already exists rather than
            // from new animation state: a tower that never moves when it fires
            // reads as scenery. The kick is sharp and the recovery is slow,
            // which is what makes it feel like a release rather than a wobble.
            const float period = 1.0f / std::max(0.0001f, st.fireRate);
            const float since = std::clamp((period - cd.remaining) / period, 0.0f, 1.0f);
            const float kick = since < 0.28f ? (1.0f - since / 0.28f) : 0.0f;
            const float recoil = std::round(kick * 3.0f);

            // Layered contact shadow, scaled to the bigger art.
            DrawEllipse(static_cast<int>(cx), static_cast<int>(footY - 4), 30, 12,
                        Color{0, 0, 0, 45});
            DrawEllipse(static_cast<int>(cx), static_cast<int>(footY - 6), 21, 8,
                        Color{0, 0, 0, 70});
            atlas_.drawFoot(towerSpriteFor(atlas_, tag.defId, tag.towerSpec), cx,
                            footY + recoil);

            // Enchantment climbs the shaft, behind the garrison.
            const auto ench = enchantFor(atlas_, tag.elementId, tag.elementSpec);
            if (!ench.empty()) {
                const float pulse = 0.82f + 0.18f * std::sin(time_ * 3.0f + cx);
                const auto k = static_cast<unsigned char>(255 * pulse);
                atlas_.drawFoot(ench, cx, footY - 6.0f, Color{k, k, k, 255});
            } else if (!tag.elementId.empty()) {
                // Imbued but not yet specialised: a faint bead, so the player
                // can see the tower is ready for a power.
                atlas_.draw("gem", cx, footY - 46.0f, Color{200, 210, 225, 190});
            }


            // Rank pips on the ground, one per level above base, so level reads
            // without opening a panel. This used to be a trim bar across the
            // plinth, but the buildings have no plinth to trim and a full-width
            // bar stuck to the base read as a progress meter.
            if (tag.level > 1) {
                static const Color kTier[3] = {
                    Color{198, 122, 58, 255}, Color{206, 214, 226, 255},
                    Color{242, 189, 51, 255}};
                const Color pip = kTier[std::min(tag.level - 2, 2)];
                const int n = std::min(tag.level - 1, 3);
                const int step = 7;
                const int x0 = static_cast<int>(cx) - (n - 1) * step / 2;
                const int y0 = static_cast<int>(footY) - 1;
                for (int i = 0; i < n; ++i) {
                    DrawRectangle(x0 + i * step - 2, y0 - 2, 5, 5, Color{26, 22, 34, 210});
                    DrawRectangle(x0 + i * step - 1, y0 - 1, 3, 3, pip);
                }
            }
        });
}

void Renderer::drawEnemies(const sim::World& w, float alpha) const {
    auto& r = const_cast<entt::registry&>(w.reg());

    // Painter's order by depth, so enemies lower on the screen overlap those
    // above them instead of flickering arbitrarily.
    std::vector<std::pair<float, entt::entity>> order;
    r.view<const sim::Position, const sim::EnemyTag>().each(
        [&](entt::entity e, const sim::Position& p, const sim::EnemyTag&) {
            order.emplace_back(p.v.y, e);
        });
    std::sort(order.begin(), order.end());

    for (const auto& [depth, e] : order) {
        const auto& pos = r.get<sim::Position>(e);
        const auto& prev = r.get<sim::PrevPosition>(e);
        const auto& hp = r.get<sim::Health>(e);
        const auto& tag = r.get<sim::EnemyTag>(e);
        const auto& pf = r.get<sim::PathFollower>(e);

        const core::Vec2 p = core::lerp(prev.v, pos.v, alpha);
        const float cx = p.x * kTile;
        const float cy = p.y * kTile;

        // Which way is this enemy facing? Every sprite in the pack is drawn
        // walking to the RIGHT, so on the leftward legs of a serpentine route an
        // unflipped sprite reads as walking backwards -- which is exactly what it
        // looked like. Sampled from the PATH rather than from the frame's
        // movement delta, because the delta is zero whenever an enemy is frozen
        // or fully slowed, and a stationary enemy would then flicker or face the
        // wrong way. The window straddles the enemy so corners resolve cleanly,
        // and a purely vertical leg leaves dx at 0, which keeps the last facing
        // rather than snapping.
        const auto& route = w.path();
        const float ahead = route.positionAt(pf.distance + 0.6f).x;
        const float behind = route.positionAt(pf.distance - 0.6f).x;
        const bool faceLeft = ahead < behind;

        const auto& edef = w.defs().enemy(tag.defId);
        const bool flying = edef.flying;
        const int frames = std::max(1, atlas_.frameCount(edef.spriteId()));
        // Ground units animate off DISTANCE TRAVELLED, so a frozen enemy stops
        // walking mid-stride; fliers animate off time, because they hover in
        // place and their wings should never stop.
        const float phase = flying ? time_ * 14.0f : pf.distance * 3.2f;
        int frame = static_cast<int>(phase) % frames;
        if (frame < 0) frame += frames;
        const std::string sprite = edef.spriteId() + "_" + std::to_string(frame);

        const float hover = flying ? std::sin(time_ * 5.0f + p.x) * 2.0f - 4.0f : 0.0f;

        // Every creature frame carries its own baked contact shadow, so drawing
        // one here too gave each enemy a doubled shadow. Fliers still need one:
        // their baked shadow lifts with the hover, which a real one would not.
        if (flying) {
            DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 2), 13, 5, Color{0, 0, 0, 45});
            DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 1), 8, 3, Color{0, 0, 0, 75});
        }

        // The def's own tint is the base, so a map's roster reads as that map's;
        // status effects then override it, because knowing an enemy is frozen
        // matters more than knowing which map it belongs to.
        Color tint{static_cast<unsigned char>(edef.tintR), static_cast<unsigned char>(edef.tintG),
                   static_cast<unsigned char>(edef.tintB), 255};
        if (r.all_of<sim::Petrified>(e)) tint = Color{170, 175, 195, 255};
        else if (r.all_of<sim::Frozen>(e)) tint = Color{170, 214, 255, 255};
        else if (r.all_of<sim::Burning>(e)) tint = Color{255, 186, 140, 255};
        else if (r.all_of<sim::Poisoned>(e)) tint = Color{196, 255, 190, 255};
        else if (r.all_of<sim::Slowed>(e)) tint = Color{190, 215, 255, 255};

        // Anchored so the baked shadow's centre lands on the path, which is
        // where the unit is standing; the feet sit at the top of that shadow.
        const float footY = cy + hover + kEnemyShadowInset * static_cast<float>(edef.spriteScale);
        atlas_.drawFootScaled(sprite, cx, footY, edef.spriteScale, tint, faceLeft);

        // Additive white pass reads as a flash without needing a shader.
        if (const auto* f = r.try_get<sim::HitFlash>(e)) {
            BeginBlendMode(BLEND_ADDITIVE);
            const float k = std::clamp(f->remaining / 0.08f, 0.0f, 1.0f);
            atlas_.drawFootScaled(sprite, cx, footY, edef.spriteScale,
                                  Color{255, 255, 255, static_cast<unsigned char>(190 * k)},
                                  faceLeft);
            EndBlendMode();
        }

        if (hp.hp < hp.maxHp && hp.maxHp > 0.0f) {
            const float frac = std::clamp(hp.hp / hp.maxHp, 0.0f, 1.0f);
            const auto* tex = atlas_.get(sprite);
            const float top = footY - static_cast<float>((tex ? tex->height : 24) *
                                                         edef.spriteScale);
            // A wider bar on a wider sprite, or a boss gets a 26px bar over a
            // 90px body.
            const int bw = 26 * edef.spriteScale;
            const int bx = static_cast<int>(cx) - bw / 2;
            const int by = static_cast<int>(top) - 8;
            DrawRectangle(bx - 1, by - 1, bw + 2, 6, Color{0, 0, 0, 180});
            DrawRectangle(bx, by, static_cast<int>(bw * frac), 4,
                          frac > 0.5f ? palette::kHpFill
                                      : (frac > 0.25f ? Color{224, 176, 72, 255}
                                                      : Color{200, 72, 60, 255}));
        }
    }
}

void Renderer::drawProjectiles(const sim::World& w, float alpha) const {
    w.reg().view<const sim::Position, const sim::PrevPosition, const sim::Projectile>().each(
        [&](const sim::Position& pos, const sim::PrevPosition& prev, const sim::Projectile& pr) {
            const core::Vec2 p = core::lerp(prev.v, pos.v, alpha);
            const float deg = std::atan2(pr.dir.y, pr.dir.x) * 180.0f / 3.14159265f;
            // Per-tower projectile art. A cannon shell drawn as an arrow was the
            // most obviously wrong thing on the board once there were five tower
            // types. Falls back to the arrow for anything without its own.
            std::string art = "arrow";
            if (pr.source != entt::null && w.reg().valid(pr.source)) {
                if (const auto* tag = w.reg().try_get<sim::TowerTag>(pr.source)) {
                    const std::string want = "proj_" + tag->defId;
                    if (atlas_.has(want)) art = want;
                }
            }
            atlas_.drawRotated(art, p.x * kTile, p.y * kTile, deg);
        });
}

// A vignette and a warm-to-cool wash. Flat ambient light everywhere is what
// makes a scene look like a spreadsheet of tiles; darkening the corners pulls
// the eye to the middle of the board and gives the field depth it cannot get
// from the tiles themselves.
void Renderer::drawAtmosphere() const {
    constexpr int kSteps = 5;
    for (int i = 0; i < kSteps; ++i) {
        const int inset = i * 5;
        const unsigned char a = static_cast<unsigned char>(16 - i * 3);
        DrawRectangleLinesEx(Rectangle{static_cast<float>(inset), static_cast<float>(inset),
                                       static_cast<float>(kPlayW - inset * 2),
                                       static_cast<float>(kPlayH - inset * 2)},
                             5.0f, Color{10, 8, 20, a});
    }
    // A cool cast in the upper left, warm in the lower right: the same light
    // direction the sprites are shaded for, applied to the whole scene.
    DrawRectangleGradientV(0, 0, kPlayW, kPlayH / 2, Color{120, 150, 200, 12},
                           Color{0, 0, 0, 0});
    DrawRectangleGradientV(0, kPlayH / 2, kPlayW, kPlayH / 2, Color{0, 0, 0, 0},
                           Color{220, 150, 90, 14});
}

void Renderer::drawCursor(const sim::World& w, const Cursor& cur) const {
    // Selection: the range ring belongs here and nowhere else.
    if (cur.selX >= 0) {
        const auto e = w.towerAt(cur.selX, cur.selY);
        if (e != entt::null) {
            const float range = w.reg().get<sim::TowerStats>(e).range * kTile;
            const int cx = static_cast<int>((cur.selX + 0.5f) * kTile);
            const int cy = static_cast<int>((cur.selY + 0.5f) * kTile);
            DrawCircle(cx, cy, range, Color{255, 255, 255, 18});
            DrawCircleLines(cx, cy, range, Color{255, 255, 255, 90});
            DrawRectangleLines(cur.selX * kTile, cur.selY * kTile, kTile, kTile,
                               Color{255, 236, 170, 220});
        }
    }

    // Hover: a quiet outline, plus a ghost of what would be built. No ring.
    if (cur.hoverX < 0) return;
    if (cur.hoverX == cur.selX && cur.hoverY == cur.selY) return;

    const int hx = cur.hoverX * kTile, hy = cur.hoverY * kTile;
    if (w.towerAt(cur.hoverX, cur.hoverY) != entt::null) {
        DrawRectangleLines(hx, hy, kTile, kTile, Color{255, 255, 255, 120});
        return;
    }
    // A quiet outline only. A ghost tower under the cursor at all times read as
    // "you are about to build", which is wrong when you are just moving the mouse.
    if (cur.hoverBuildable) {
        DrawRectangleLines(hx, hy, kTile, kTile,
                           cur.hoverAffordable ? Color{255, 255, 255, 80}
                                               : Color{200, 110, 100, 80});
    }
}

void Renderer::draw(const sim::World& w, float alpha, const Cursor& cur) {
    const core::Vec2 shake = fx_.shakeOffset();
    Camera2D cam{};
    cam.target = {0.0f, 0.0f};
    cam.offset = {shake.x, shake.y};
    cam.rotation = 0.0f;
    cam.zoom = 1.0f;

    BeginMode2D(cam);
    drawTerrain(w.map());
    drawGroundVariation(w.map());
    drawGoal(w.map());
    drawProps(w.map());
    drawCursor(w, cur);
    drawCorpses();
    drawEnemies(w, alpha);
    drawTowers(w);
    drawProjectiles(w, alpha);
    fx_.drawWorldLayer();
    EndMode2D();

    drawAtmosphere();
}

}  // namespace td::render

namespace td::render {

void Renderer::drawArtCompare() const {
    // Lay the samples on the actual terrain: art only matters in context, and a
    // sprite that reads on a flat swatch can still vanish against grass.
    for (int y = 0; y < 15; ++y) {
        for (int x = 0; x < 30; ++x) {
            blitTile(tiles_.grass(x, y), x * kTile, y * kTile, TileSet::flipX(x, y),
                     TileSet::flipY(x, y));
        }
    }
    for (int x = 0; x < 30; ++x) {
        blitTile(tiles_.dirt(x, 7), x * kTile, 7 * kTile, false, false);
        DrawRectangle(x * kTile, 7 * kTile, kTile, 2, Color{0, 0, 0, 70});
        DrawRectangle(x * kTile, 8 * kTile - 2, kTile, 2, Color{0, 0, 0, 70});
    }

    DrawRectangle(0, 0, kVirtualW, 34, Color{16, 14, 24, 220});
    DrawText("BEFORE", 150, 8, 20, Color{200, 130, 120, 255});
    DrawText("AFTER", 640, 8, 20, Color{150, 220, 140, 255});

    const auto pair = [&](const char* oldId, const char* newId, float y, const char* label,
                          bool foot) {
        DrawText(label, 12, static_cast<int>(y) - 8, 10, Color{200, 196, 210, 255});
        // 1x, then 2x and 3x so the detail is legible on this page.
        for (int i = 0; i < 2; ++i) {
            const char* id = i == 0 ? oldId : newId;
            const float baseX = i == 0 ? 150.0f : 640.0f;
            const auto* t = atlas_.get(id);
            if (!t) continue;
            for (int s = 1; s <= 3; ++s) {
                const float cx = baseX + (s - 1) * 90.0f;
                const Rectangle src{0, 0, static_cast<float>(t->width),
                                    static_cast<float>(t->height)};
                const Rectangle dst{cx - t->width * s * 0.5f,
                                    foot ? y - t->height * s : y - t->height * s * 0.5f,
                                    static_cast<float>(t->width * s),
                                    static_cast<float>(t->height * s)};
                DrawTexturePro(*t, src, dst, Vector2{0, 0}, 0.0f, WHITE);
            }
        }
    };

    pair("slime_0", "slime2_0", 250.0f, "SLIME   16px -> 24px, outlined, shaded", false);
    pair("tower_base", "tower2_base", 452.0f, "TOWER   24x32 -> 32x56, 3/4 view", true);

    DrawRectangle(0, kHudY, kVirtualW, kHudH, Color{16, 14, 24, 240});
    DrawText("shown at 1x, 2x and 3x on the real terrain", 12, kHudY + 14, 20,
             Color{232, 228, 217, 255});
    DrawText("outlines + a full shadow-to-highlight ramp are what make a sprite read",
             12, kHudY + 38, 10, Color{140, 136, 128, 255});
}

}  // namespace td::render

namespace td::render {

void Renderer::drawSpriteSheet() const {
    DrawRectangle(0, 0, kVirtualW, kVirtualH, Color{22, 20, 32, 255});
    DrawText("SPRITE SHEET  -  every sprite by name, at 2x", 12, 8, 20,
             Color{232, 228, 217, 255});
    DrawText("ask for a change by id, e.g. \"make goblin_0 angrier\"", 12, 32, 10,
             Color{140, 136, 128, 255});

    // Ordered so related sprites sit together and the set reads as a family.
    const auto kOrder = atlas_.ids();  // whatever exists, including every frame

    constexpr int kCell = 58, kCols = 16, kLeft = 12, kTop = 56;
    int i = 0;
    for (const auto& id : kOrder) {
        const auto* t = atlas_.get(id);
        if (!t) continue;
        const int col = i % kCols, row = i / kCols;
        const int cx = kLeft + col * kCell + kCell / 2;
        const int cy = kTop + row * (kCell + 34) + kCell / 2;

        DrawRectangle(cx - kCell / 2 + 2, cy - kCell / 2 + 2, kCell - 4, kCell - 4,
                      Color{34, 31, 48, 255});
        DrawRectangleLines(cx - kCell / 2 + 2, cy - kCell / 2 + 2, kCell - 4, kCell - 4,
                           Color{60, 56, 80, 255});

        const int s = (t->width > 26 || t->height > 26) ? 1 : 2;
        const Rectangle src{0, 0, static_cast<float>(t->width), static_cast<float>(t->height)};
        const Rectangle dst{static_cast<float>(cx - t->width * s / 2),
                            static_cast<float>(cy - t->height * s / 2),
                            static_cast<float>(t->width * s),
                            static_cast<float>(t->height * s)};
        DrawTexturePro(*t, src, dst, Vector2{0, 0}, 0.0f, WHITE);

        DrawText(id.c_str(), cx - MeasureText(id.c_str(), 10) / 2, cy + kCell / 2 - 2, 10,
                 Color{190, 186, 200, 255});
        DrawText(TextFormat("%dx%d", t->width, t->height),
                 cx - MeasureText(TextFormat("%dx%d", t->width, t->height), 10) / 2,
                 cy + kCell / 2 + 10, 10, Color{110, 106, 122, 255});
        ++i;
    }
}

}  // namespace td::render
