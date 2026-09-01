#pragma once

#include <vector>

#include "raylib.h"

namespace td::render {

// Terrain tiles are GENERATED rather than authored: grass wants noise and
// scattered detail, which is tedious to hand-place and trivial to synthesise,
// and generating several variants kills the visible repetition that makes a
// flat-coloured grid look cheap. Generation is deterministic, so the map looks
// identical every run.
class TileSet {
public:
    TileSet() = default;
    ~TileSet();
    TileSet(const TileSet&) = delete;
    TileSet& operator=(const TileSet&) = delete;

    void generate(int tileSize);

    // Deterministic per-coordinate variant choice.
    const Texture2D& grass(int tx, int ty) const;
    const Texture2D& dirt(int tx, int ty) const;
    // Per-tile mirroring, which multiplies apparent variety fourfold for free
    // and is the cheapest way to break a visible tile repeat.
    static bool flipX(int tx, int ty);
    static bool flipY(int tx, int ty);
    const Texture2D& spawn() const { return spawn_; }
    const Texture2D& exitTile() const { return exit_; }

    // Transition pieces. A path drawn as bare squares reads as a grid; grass
    // lipping over its edge, with proper corners, is what makes it a road.
    // One edge piece plus two corner pieces cover every case once rotated,
    // instead of authoring the full 47-tile blob set.
    const Texture2D& edgeLip() const { return edge_; }
    const Texture2D& outerCorner() const { return outer_; }
    const Texture2D& innerCorner() const { return inner_; }

private:
    std::vector<Texture2D> grass_;
    std::vector<Texture2D> dirt_;
    Texture2D spawn_{};
    Texture2D exit_{};
    Texture2D edge_{};
    Texture2D outer_{};
    Texture2D inner_{};
    bool loaded_ = false;
};

}  // namespace td::render
