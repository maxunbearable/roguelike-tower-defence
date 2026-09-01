#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <string>

#include "content/Registry.h"
#include "render/PixelCanvas.h"
#include "render/Renderer.h"
#include "render/PostFx.h"
#include "audio/Sfx.h"
#include "audio/Jukebox.h"
#include "core/SaveGame.h"
#include "core/SaveIO.h"
#include "ui/Hud.h"
#include "ui/Screens.h"
#include "ui/RadialMenu.h"
#include "sim/World.h"

namespace td::app {

// Slots -> Hub (skill trees) -> Playing -> Results -> back to the Hub.
// The hub is where a defeated run turns into permanent progress.
enum class Screen { Slots, Hub, Maps, Playing, Results, ArtCompare, SpriteSheet };

// Owns the screen state machine, the fixed-timestep accumulator, and input.
// Everything it draws goes through PixelCanvas, so the whole game is rendered at
// the virtual resolution and integer-scaled exactly once.
class Game {
public:
    explicit Game(const content::Registry& registry);
    void loadArt() {  // needs a live GL and audio context, so not the constructor
        renderer_.load(*registry_);
        sfx_.load();
        postfx_.load(std::filesystem::path(TD_ASSET_DIR) / "shaders");
        jukebox_.load();
    }

    void frame(float frameDt);  // update + render one frame, including blit
    Screen screen() const { return screen_; }
    void requestStart(bool demoTowers = false, const std::string& mapId = {});
    void openHub(int slot);                      // dev capture: jump straight to the hub
    void showArtCompare() { screen_ = Screen::ArtCompare; }
    void showSpriteSheet() { screen_ = Screen::SpriteSheet; }
    // Dev capture: the two screens that otherwise need live input to reach.
    void devOpenMenu(int tileX, int tileY) { openMenuAt(tileX, tileY); }
    void devSetHubTab(int tab) { hubTab_ = tab; }
    // Dev capture: skip ahead to a wave, so a boss fight can be inspected
    // without playing 25 waves to reach it.
    // Dev capture: a dense block of towers, to reproduce light-pool stacking.
    void devCluster(int n) {
        if (!world_) return;
        int placed = 0;
        for (int y = 5; y <= 7 && placed < n; ++y) {
            for (int x = 6; x <= 10 && placed < n; ++x) {
                if (world_->placeTower(x, y, "arrow") != sim::World::PlaceResult::Ok) continue;
                while (world_->upgradeCost(x, y) > 0) world_->upgradeTower(x, y);
                ++placed;
            }
        }
    }

    void devPause() { hud_.paused = true; }

    void devJumpToWave(int wave) {
        if (world_) world_->devSetWave(wave);
    }
    void devShowMaps() { reloadSlots(); activeSlot_ = 0; active().used = true;
                         screen_ = Screen::Maps; }
    void devShowResults(int award) {
        reloadSlots();
        activeSlot_ = 0;
        active().used = true;
        requestStart(/*demoTowers=*/true);  // drawResults reports on a live world
        lastAward_ = award;
        screen_ = Screen::Results;
    }

private:
    void startRun(int goldOverride = -1);
    void reloadSlots();
    core::SaveSlot& active();
    core::Loadout metaLoadout() const;
    void persist();
    void beginRun(bool resume, const std::string& mapId = {});
    void updateMaps();
    void finishRun();
    void updateSlots();
    void updateHub();
    void updateResults();
    void updatePlaying(float frameDt);
    void handleBuildInput();
    // The radial menu is a shallow tree: a root of categories, and one page of
    // concrete choices under each.
    enum class MenuPage { Root, Spec, Element };

    bool openMenuAt(int tileX, int tileY);
    void closeMenu();
    void showPage(MenuPage page);
    // Fills the menu's side panel with the selected tower's live stats, and
    // the upgrade delta when that item is hovered.
    void refreshMenuInfo();
    // The dossier for an enemy under the cursor, drawn over the field where
    // the player's attention already is mid-wave.
    void drawHoveredEnemy();
    std::vector<ui::RadialItem> buildMenuItems(int tileX, int tileY, MenuPage page) const;
    void applyMenuItem(const ui::RadialItem& item);
    core::Vec2 mouseVirtual() const;
    // Towers are the only light sources: the field is dark and they reveal it.
    std::vector<render::PostFx::Light> collectLights() const;
    void renderCanvas(float alpha);
    void say(const std::string& msg);

    const content::Registry* registry_;
    render::PixelCanvas canvas_;
    render::Renderer renderer_;
    render::PostFx postfx_;
    audio::Sfx sfx_;
    audio::Jukebox jukebox_;
    std::unique_ptr<sim::World> world_;

    Screen screen_ = Screen::Slots;

    std::vector<core::SaveSlot> slots_;
    int activeSlot_ = -1;
    int hubTab_ = 0;
    // Which map the live run is on. Needed to record progress against
    // the right map when the run ends.
    std::string runMapId_ = "greenfields";
    std::string hubMessage_;
    int lastAward_ = 0;
    int savedWave_ = -1;
    // The tile the radial menu is anchored to, or -1 for none.
    int selX_ = -1;
    int selY_ = -1;
    ui::RadialMenu menu_;
    MenuPage menuPage_ = MenuPage::Root;
    float accumulator_ = 0.0f;
    int hoverX_ = -1;
    int hoverY_ = -1;
    ui::HudState hud_;
    // 50 waves is a long map, so a fast-forward is a playability requirement
    // rather than a luxury. Multipliers stay integral so the fixed timestep is
    // unaffected -- it simply runs more ticks per frame.
    static constexpr float kSpeeds[3] = {1.0f, 2.0f, 3.0f};
};

}  // namespace td::app
