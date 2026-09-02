#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Vec2.h"

namespace td::content {

struct EnemyDef {
    std::string id;
    std::string name;
    float maxHp = 1.0f;
    float armor = 0.0f;
    float speed = 1.0f;  // tiles per second
    int bounty = 0;
    int shardValue = 0;
    bool flying = false;
    // Bosses take hard crowd control as a heavy slow instead of a full stop, so
    // a single petrify or freeze cannot trivialise the fight the map builds to.
    bool boss = false;

    // Which sprite set to draw, when it differs from the id. This is what lets
    // each map field its own roster -- an ash-hardened slime, a frost-bitten
    // wolf -- with different resistances and a different tint, without needing
    // a new sprite sheet per map.
    std::string sprite;
    // Multiplied into the sprite's colour. 255,255,255 is untouched.
    int tintR = 255, tintG = 255, tintB = 255;
    // Whole-number sprite magnification. Bosses draw at 2 so they read as a
    // boss on sight; anything fractional would resample the pixel art.
    int spriteScale = 1;

    const std::string& spriteId() const { return sprite.empty() ? id : sprite; }

    // Damage-type multipliers. 1.0 is neutral, below 1 is resistant, above 1 is
    // vulnerable. Absent keys are neutral, so an enemy only declares what it
    // actually cares about.
    std::map<std::string, float> resist;

    float resistTo(const std::string& damageType) const {
        const auto it = resist.find(damageType);
        return it == resist.end() ? 1.0f : it->second;
    }
};

struct WaveGroup {
    std::string enemyId;
    int count = 0;
    float interval = 1.0f;
    float startDelay = 0.0f;
    // Per-wave scaling, baked in when waves are generated from a recipe.
    // Hand-authored waves leave these neutral.
    float hpMult = 1.0f;
    float armorAdd = 0.0f;
    float bountyMult = 1.0f;
};

struct WaveDef {
    float delay = 0.0f;
    std::vector<WaveGroup> groups;
};

// Multipliers are absolute against the tower's base stats, not cumulative.
struct TowerLevel {
    int cost = 0;
    float damageMult = 1.0f;
    float rangeMult = 1.0f;
    float fireRateMult = 1.0f;
};

struct TowerDef {
    std::string id;
    std::string name;
    // One line on what this tower is FOR, shown in the build ring. Content, not
    // a hardcoded table in the UI layer.
    std::string desc;
    int buildCost = 0;
    float sellRefundPct = 0.0f;
    float damage = 0.0f;
    float fireRate = 1.0f;
    float range = 1.0f;
    float projectileSpeed = 1.0f;
    int projectileCount = 1;
    int pierce = 0;
    float critChance = 0.0f;
    float critMult = 1.0f;
    float armorPen = 0.0f;
    std::string targetPriority = "first";
    std::string damageType = "physical";
    int specCost = 0;
    std::vector<TowerLevel> levels;  // levels[0] is level 2, levels[1] is level 3
};

// An element's stats resolve to paths "<id>.<key>" for base values and
// "<id>.<spec>.<key>" for per-spec values, so a new element is a TOML file and
// no new C++ on the stat side.
struct ElementDef {
    std::string id;
    std::string name;
    std::string desc;  // what imbuing this element buys you
    std::string damageType = "physical";
    int attachCost = 0;
    int specCost = 0;
    std::map<std::string, float> base;
    std::map<std::string, std::map<std::string, float>> specs;
};

// A map with 50 waves is authored as a recipe, not as 50 literal [[wave]]
// tables. Expansion is deterministic (no RNG), so a map always plays the same.
// A boss on a named wave. Bosses are an ADDITION to the wave they land on, not
// a replacement: a boss wave that dropped its escort would be easier than the
// wave before it.
struct BossWave {
    int wave = 0;  // 1-based
    std::string enemyId;
};

struct WavePoolEntry {
    std::string enemyId;
    int fromWave = 1;  // first wave this enemy can appear in (1-based)
};

struct WaveRecipe {
    int count = 0;
    int countBase = 8;
    float countPerWave = 0.55f;
    float intervalBase = 0.9f;
    float intervalDecay = 0.985f;
    float intervalMin = 0.30f;
    float hpPerWave = 1.05f;
    // Bends the HP curve. 1.0 is plain exponential; above 1.0 keeps the opening
    // waves gentle while making the late ones accelerate hard. A single flat
    // exponent cannot do both -- raising it to threaten wave 50 also crushes
    // wave 5, which is precisely what makes new players quit.
    float hpCurveExp = 1.0f;
    float armorPerWave = 0.08f;
    float bountyPerWave = 1.015f;
    int secondaryFromWave = 5;
    float secondaryFraction = 0.6f;
    float secondaryDelay = 2.5f;
    float delay = 5.0f;
    // How long after the escort starts the boss walks on, so the player fights
    // the wave first and the boss second.
    float bossDelay = 8.0f;
    std::vector<WavePoolEntry> pool;
    std::vector<BossWave> bosses;
};

struct MapDef {
    std::string id;
    std::string name;
    // Campaign position. The registry stores maps alphabetically, which is not
    // the order they are meant to be played in, so the sequence is authored.
    int order = 0;
    std::string blurb;
    int gridW = 0;
    int gridH = 0;
    int startGold = 0;
    float buildTime = 0.0f;
    std::vector<std::string> tileRows;
    std::vector<core::Vec2> pathWaypoints;
    std::vector<WaveDef> waves;
    bool hasRecipe = false;
    WaveRecipe recipe;

    char tileAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= gridW || y >= gridH) return '\0';
        return tileRows[static_cast<size_t>(y)][static_cast<size_t>(x)];
    }
    // 'o' only. Open ground used to be buildable everywhere, which made a plot
    // an unlimited resource and gold the single binding constraint -- so the
    // cheapest tower won every marginal build and every shard-unlocked tower was
    // a worse buy than the free one. Plots are now authored and finite.
    bool buildableAt(int x, int y) const { return tileAt(x, y) == 'o'; }
};

}  // namespace td::content
