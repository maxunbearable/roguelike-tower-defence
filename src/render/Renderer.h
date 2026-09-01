#pragma once

#include <vector>

#include "content/Registry.h"
#include "render/Effects.h"
#include "render/SpriteAtlas.h"
#include "render/TileGen.h"
#include "sim/World.h"

namespace td::render {

// What the player is pointing at and what they have selected. Hover stays
// deliberately quiet -- a range ring that follows the cursor everywhere is
// noise, so the ring belongs to the SELECTED tower only.
struct Cursor {
    int hoverX = -1, hoverY = -1;
    int selX = -1, selY = -1;
    bool hoverBuildable = false;
    bool hoverAffordable = false;
};

class Renderer {
public:
    void load(const content::Registry& reg);
    void update(float dt, const std::vector<sim::VisualEvent>& events,
                const content::Registry& reg);
    void draw(const sim::World& w, float alpha, const Cursor& cur);
    const SpriteAtlas& atlas() const { return atlas_; }
    const TileSet& tiles() const { return tiles_; }
    // Dev-only: old sprites beside new ones, on the real grass, at real scale.
    void drawArtCompare() const;
    // Dev-only: every sprite, labelled, so a change can be asked for by name.
    void drawSpriteSheet() const;

private:
    void drawTerrain(const content::MapDef& m) const;
    void drawGroundVariation(const content::MapDef& m) const;
    void drawProps(const content::MapDef& m) const;
    void drawGoal(const content::MapDef& m) const;
    void drawTowers(const sim::World& w) const;
    void drawEnemies(const sim::World& w, float alpha) const;
    void drawCorpses() const;
    void drawProjectiles(const sim::World& w, float alpha) const;
    void drawCursor(const sim::World& w, const Cursor& cur) const;
    void drawAtmosphere() const;

    SpriteAtlas atlas_;
    TileSet tiles_;
    Effects fx_;

    // Death animation. The enemy entity is destroyed the instant it dies, so the
    // corpse cannot live in the ECS -- it is a purely presentational copy of what
    // was on screen, kept here because this is the only place that has both the
    // atlas and the enemy definitions.
    struct Corpse {
        core::Vec2 pos;
        std::string base;    // sprite base id; frames are base_0..base_(frames-1)
        int frames = 1;      // >1 means the pack shipped a real death sequence
        int scale = 1;
        bool faceLeft = false;
        float life = 0.0f;
        float maxLife = 0.45f;
        Color tint{255, 255, 255, 255};
    };
    std::vector<Corpse> corpses_;
    float time_ = 0.0f;
};

}  // namespace td::render
