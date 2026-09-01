#pragma once

#include <string>

#include "core/Vec2.h"
#include "render/SpriteAtlas.h"
#include "content/Defs.h"
#include "sim/World.h"

namespace td::ui {

// Clickable regions the HUD owns. Returned by hitTest so input handling lives
// with the game rather than being buried in drawing code.
enum class HudButton { None, NextWave, Speed, Pause, Quit, Mute, Strike, Ward };

struct HudState {
    std::string message;
    float messageAge = 99.0f;
    int speedIndex = 0;  // index into Game::kSpeeds: 1x / 2x / 4x / 8x
    // Two different things, deliberately. `paused` is a TACTICAL pause: the
    // simulation stops but the board stays live and the player can still build,
    // upgrade, sell and retarget. That is the single most-cited lesson in the
    // tower defence design writing -- a game that tests thinking rather than
    // reflexes should let you stop and think without also taking your hands off
    // the controls. `menuOpen` is the settings modal, which does block.
    bool paused = false;
    bool menuOpen = false;
    bool muted = false;
    // Which ability is armed and waiting for a target on the board, as an index
    // into sim::Ability; -1 for none.
    int armed = -1;
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

// The tactical pause indicator: a band across the top of the play field saying
// the game is stopped AND that the player can still act, because a frozen board
// with a live cursor is otherwise indistinguishable from a hang.
void drawPausedBanner();

// The guided first run: one instruction at a time, with a way out. Returns the
// SKIP button's rectangle so the caller can hit-test it; drawn at the top of the
// play field so it never covers the road or the controls it is talking about.
struct TutorialBox { int x, y, w, h, skipX, skipY, skipW, skipH; };
TutorialBox drawTutorial(const render::SpriteAtlas& atlas, const char* title, const char* body,
                         core::Vec2 mouse);


}  // namespace td::ui
