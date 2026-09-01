#pragma once

#include <string>

#include "core/Vec2.h"
#include "render/SpriteAtlas.h"
#include "content/Defs.h"
#include "sim/World.h"

namespace td::ui {

// Clickable regions the HUD owns. Returned by hitTest so input handling lives
// with the game rather than being buried in drawing code.
enum class HudButton { None, NextWave, Speed, Pause, Quit, Mute };

struct HudState {
    std::string message;
    float messageAge = 99.0f;
    int speedIndex = 0;  // 0 = 1x, 1 = 2x, 2 = 3x
    bool paused = false;
    bool muted = false;
};

// A prominent bar per living boss, across the top of the play field. A boss has
// 200,000-odd health at wave 50; without this the player has no idea whether
// they are winning the fight or wasting the last of their gold.
// The enemy dossier: HP, armour, speed and the FULL resistance list for one
// enemy. The game's whole map-to-map replayability rests on those resistances
// and nothing displayed them, so a player could only learn them by losing.
// `atMouse` positions it near the cursor without leaving the play field.
void drawEnemyDossier(const render::SpriteAtlas& atlas, const content::EnemyDef& def,
                      core::Vec2 atMouse);

void drawBossBars(const render::SpriteAtlas& atlas, const sim::World& w);

void drawHud(const render::SpriteAtlas& atlas, const sim::World& w, const HudState& st,
             core::Vec2 mouse);
HudButton hudHitTest(core::Vec2 mouse);


}  // namespace td::ui
