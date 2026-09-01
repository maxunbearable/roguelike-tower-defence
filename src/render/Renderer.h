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
    void update(float dt, const std::vector<sim::VisualEvent>& events);
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
    void drawProjectiles(const sim::World& w, float alpha) const;
    void drawCursor(const sim::World& w, const Cursor& cur) const;
    void drawAtmosphere() const;

    SpriteAtlas atlas_;
    TileSet tiles_;
    Effects fx_;
    float time_ = 0.0f;
};

}  // namespace td::render
