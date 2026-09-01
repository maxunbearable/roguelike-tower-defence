#pragma once

#include <string>
#include <vector>

#include "content/Registry.h"
#include "core/SaveGame.h"
#include "core/Vec2.h"
#include "render/SpriteAtlas.h"
#include "sim/World.h"

namespace td::ui {

// --- slot select ----------------------------------------------------------

struct SlotAction {
    enum class Kind { None, Open, Delete };
    Kind kind = Kind::None;
    int slot = -1;
};

void drawSlots(const render::SpriteAtlas& atlas, const std::vector<core::SaveSlot>& slots,
               core::Vec2 mouse);
SlotAction slotHitTest(const std::vector<core::SaveSlot>& slots, core::Vec2 mouse);

// --- between-run hub: the skill trees, and where a run is launched from ----

struct HubAction {
    enum class Kind { None, Buy, SwitchTab, ContinueRun, NewRun, BackToSlots };
    Kind kind = Kind::None;
    std::string nodeId;
    int tab = 0;
};

// The hub's tab order, derived from the registry rather than hardcoded: global
// first, then every tower tree, then every element tree. Adding a tower or an
// element must not require touching the hub.
struct HubTab {
    std::string treeId;
    std::string label;
};
std::vector<HubTab> hubTabs(const content::Registry& reg);

void drawHub(const render::SpriteAtlas& atlas, const content::Registry& reg,
             const core::SaveSlot& slot, int tab, const std::string& message, core::Vec2 mouse);
HubAction hubHitTest(const content::Registry& reg, const core::SaveSlot& slot, int tab,
                     core::Vec2 mouse);

// --- map select -----------------------------------------------------------

struct MapAction {
    enum class Kind { None, Play, Back, SetDifficulty };
    Kind kind = Kind::None;
    std::string mapId;
    int difficulty = -1;
};

// The campaign order, authored via each map's `order` field rather than the
// registry's alphabetical storage.
std::vector<std::string> mapOrder(const content::Registry& reg);

void drawMaps(const render::SpriteAtlas& atlas, const content::Registry& reg,
              const core::SaveSlot& slot, core::Vec2 mouse);
MapAction mapHitTest(const content::Registry& reg, const core::SaveSlot& slot, core::Vec2 mouse);

// --- pause overlay --------------------------------------------------------

struct PauseAction {
    enum class Kind { None, Resume, Quit, SetMusic, SetSfx, ToggleColorAlt, CycleShake };
    Kind kind = Kind::None;
    float value = 0.0f;  // for the slider kinds, 0..1
};

// `P` used to toggle a flag and draw the word PAUSED. There was no way to change
// volume and no way out of a run except a small icon in the HUD band.
void drawPause(const render::SpriteAtlas& atlas, float musicVol, float sfxVol,
               bool colorAlt, float shake, core::Vec2 mouse);
PauseAction pauseHitTest(core::Vec2 mouse);

// --- run results ----------------------------------------------------------

void drawResults(const render::SpriteAtlas& atlas, const sim::World& w,
                 int shardsAwarded, int totalShards);

}  // namespace td::ui
