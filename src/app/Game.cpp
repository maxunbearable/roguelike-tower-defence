#include "app/Game.h"

#include "content/SpecFacts.h"
#include "core/Settings.h"
#include "sim/Tutorial.h"

#include <cstdio>

#include "raylib.h"

#include "render/Palette.h"
#include "render/Renderer.h"
#include "ui/Hud.h"
#include "core/Progression.h"
#include "ui/RadialMenu.h"
#include "ui/Screens.h"

namespace td::app {
namespace {

const char* describe(sim::World::PlaceResult r) {
    switch (r) {
        case sim::World::PlaceResult::Ok: return "";
        case sim::World::PlaceResult::NotBuildable: return "cannot build there";
        case sim::World::PlaceResult::Occupied: return "tile already occupied";
        case sim::World::PlaceResult::TooPoor: return "not enough gold";
        case sim::World::PlaceResult::OutOfBounds: return "outside the map";
        case sim::World::PlaceResult::UnknownTower: return "unknown tower";
    }
    return "";
}

}  // namespace

Game::Game(const content::Registry& registry) : registry_(&registry) { reloadSlots(); }

void Game::say(const std::string& msg) {
    hud_.message = msg;
    hud_.messageAge = 0.0f;
}

void Game::hintOnce(const char* id, const std::string& text) {
    auto& seen = active().meta.seenHints;
    if (!seen.insert(id).second) return;  // already shown to this profile
    say(text);
}

// A wave used to begin in silence: the only sign was a counter ticking over in
// the corner. Bosses arrived the same way as trash. Announcing them is the
// cheapest structural feedback in the game.
bool Game::tutorialActive() const {
    return sim::tutorialFromIndex(activeConst().meta.tutorialStep) != sim::TutorialStep::Done;
}

void Game::updateTutorial() {
    if (!world_ || !tutorialActive()) return;
    auto& step = active().meta.tutorialStep;
    const auto current = sim::tutorialFromIndex(step);

    // The menu being open ON A TOWER is the one thing the simulation cannot see.
    const bool menuOnTower =
        menu_.isOpen() && selX_ >= 0 && world_->towerAt(selX_, selY_) != entt::null;

    if (sim::tutorialSatisfied(current, *world_, menuOnTower)) {
        step = sim::tutorialToIndex(sim::tutorialNext(current));
        sfx_.play(audio::Cue::Buy, 0.02f, 0.7f);
        // Advance through anything the player has ALREADY done -- someone who
        // levelled a tower before being asked to should not be told to do it.
        int guard = 0;
        while (guard++ < 8) {
            const auto next = sim::tutorialFromIndex(step);
            if (next == sim::TutorialStep::Done) break;
            if (!sim::tutorialSatisfied(next, *world_, menuOnTower)) break;
            step = sim::tutorialToIndex(sim::tutorialNext(next));
        }
    }
}

void Game::applySettings() {
    const auto& m = activeConst().meta;
    renderer_.setShakeScale(m.shake);
    render::PixelCanvas::setIntegerScaling(m.integerScaling);
    hud_.colorAlternatives = m.colorAlternatives;
    jukebox_.setVolume(m.musicVolume);
    sfx_.setVolume(m.sfxVolume);
}

void Game::announceWaves() {
    if (!world_) return;
    // One voice at a time. The tutorial and the wave announcer share the bottom
    // of the screen, and the tutorial is mid-sentence.
    if (tutorialActive()) return;
    const int wi = world_->waveIndex();
    if (wi == announcedWave_ || world_->phase() != sim::Phase::Wave) return;
    announcedWave_ = wi;

    // Name the boss if this wave carries one -- "BOSS" alone tells the player
    // less than the thing's name, and every boss here resists something.
    if (wi >= 0 && wi < static_cast<int>(world_->map().waves.size())) {
        for (const auto& g : world_->map().waves[static_cast<size_t>(wi)].groups) {
            if (!registry_->hasEnemy(g.enemyId)) continue;
            const auto& def = registry_->enemy(g.enemyId);
            if (!def.boss) continue;
            say(def.name + " approaches");
            return;
        }
    }
    say(TextFormat("Wave %d", wi + 1));
}

void Game::updateHints() {
    if (!world_) return;
    // The tutorial owns the player's attention while it is running; two
    // instruction systems at once is worse than either alone.
    if (tutorialActive()) return;
    const bool building = world_->phase() == sim::Phase::Build;

    if (world_->towerCount() == 0) {
        hintOnce("build", "Click any green tile to build a tower.");
        return;  // one lesson at a time; the rest mean nothing without a tower
    }
    hintOnce("targeting", "Click a tower to set what it shoots first.");

    if (building && world_->canCallWave()) {
        hintOnce("callearly", "Call the wave early for bonus gold - the timer is money.");
    }
    if (!building) {
        hintOnce("pause", "P pauses the wave. You can still build while paused.");
    }
    if (world_->atMaxLevel(selX_, selY_)) {
        hintOnce("specialise", "Max level: this tower can now be specialised.");
    }
}

core::Vec2 Game::mouseVirtual() const {
    const Vector2 m = GetMousePosition();
    return canvas_.windowToVirtual({m.x, m.y});
}

// Dev capture only: a throwaway funded run with a part-built defence, so
// screenshots show real gameplay instead of an empty board.
// Dev capture only: fields all three specialisations with all three element
// powers, which the uniqueness rule now permits, so a screenshot shows the
// whole tower line at once.
void Game::requestStart(bool demoTowers, const std::string& mapId) {
    if (!mapId.empty() && registry_->hasMap(mapId)) runMapId_ = mapId;
    startRun(demoTowers ? 4000 : -1);
    if (!demoTowers) return;

    struct Build { int x, y; };
    // y=0 and y=2 flank the long y=1 route; picked so the dev layout survives
    // a change to the map's path.
    const Build spots[] = {{3, 0}, {8, 2}, {13, 0}, {16, 2}};
    // Buildable tiles differ per map, so skip any spot this map does not allow
    // rather than silently ending up with fewer towers than the loop assumes.
    std::vector<Build> placed;
    for (const auto& b : spots) {
        if (world_->placeTower(b.x, b.y, "arrow") == sim::World::PlaceResult::Ok) {
            placed.push_back(b);
        }
    }

    // Three specialised towers, each with a different element power. Levelling
    // to max first, because specialising now requires it.
    for (int i = 0; i < static_cast<int>(placed.size()) && i < 3; ++i) {
        const auto& b = placed[static_cast<size_t>(i)];
        while (world_->upgradeCost(b.x, b.y) > 0) world_->upgradeTower(b.x, b.y);
        world_->attachElement(b.x, b.y, "earth");
        const auto ts = world_->availableTowerSpecs(b.x, b.y);
        if (!ts.empty()) world_->specialiseTower(b.x, b.y, ts.front());
        const auto es = world_->availableElementSpecs(b.x, b.y);
        if (!es.empty()) world_->specialiseElement(b.x, b.y, es.front());
        world_->upgradeTower(b.x, b.y);
    }
    world_->upgradeTower(3, 0);   // a gold-tier tower, to show the trim

    // The menu is NOT opened here. It used to be, which meant every --autostart
    // capture had a radial menu sitting over the board; --menu opens it
    // deliberately, with coordinates.
    world_->startNextWave();
}

void Game::openHub(int slot) {
    reloadSlots();
    activeSlot_ = slot;
    if (!active().used) {
        active().used = true;
        active().profileName = "Slot " + std::to_string(slot + 1);
    }
    screen_ = Screen::Hub;
}

void Game::reloadSlots() {
    slots_.clear();
    for (int i = 0; i < core::kSlotCount; ++i) slots_.push_back(core::loadSlot(i));
}

core::SaveSlot& Game::active() {
    // With no slot selected this used to index slots_[static_cast<size_t>(-1)],
    // i.e. slots_[SIZE_MAX] -- undefined behaviour. Reachable from the dev
    // capture paths, which start a run without choosing a profile. A detached
    // scratch slot keeps callers honest without every one of them branching.
    if (activeSlot_ < 0 || activeSlot_ >= static_cast<int>(slots_.size())) {
        static core::SaveSlot scratch;
        return scratch;
    }
    return slots_[static_cast<size_t>(activeSlot_)];
}

// The permanent half of a run's power: which skill nodes the profile owns.
core::Loadout Game::metaLoadout() const {
    core::Loadout lo;
    lo.ownAll = false;  // Plan 2 owned everything; a real profile owns what it bought
    if (activeSlot_ >= 0) lo.ownedNodes = slots_[static_cast<size_t>(activeSlot_)].meta.ownedNodes;
    return lo;
}

// Writes the run whenever it has changed and can be written. Gated on the run's
// actual state rather than on the wave index, which only ever captured the start
// of a build phase and lost everything the player did during it.
void Game::maybeAutosave() {
    if (activeSlot_ < 0 || !world_) return;
    if (!sim::shouldAutosave(*world_, savedMark_)) return;
    savedMark_ = sim::saveMarkOf(*world_);
    active().run = world_->snapshot();
    persist();
}

void Game::persist() {
    if (activeSlot_ < 0) return;
    core::writeSlot(activeSlot_, active());
}

void Game::beginRun(bool resume, const std::string& mapId) {
    auto& slot = active();
    // Resuming uses the map the run was saved on; a new run uses the chosen one.
    runMapId_ = resume && slot.run && !slot.run->mapId.empty() ? slot.run->mapId
                : !mapId.empty()                               ? mapId
                                                               : runMapId_;
    if (!registry_->hasMap(runMapId_)) runMapId_ = "greenfields";
    world_ = std::make_unique<sim::World>(*registry_, registry_->map(runMapId_),
                                          resume && slot.run ? slot.run->seed : 20260830u,
                                          metaLoadout(), -1,
                                          core::difficultyFromIndex(slot.meta.difficulty));
    if (resume && slot.run) {
        world_->restore(*slot.run);
    } else {
        slot.meta.runsPlayed += 1;
        slot.run.reset();
        persist();
    }

    screen_ = Screen::Playing;
    accumulator_ = 0.0f;
    hud_ = ui::HudState{};
    applySettings();
    jukebox_.setVolume(slot.meta.musicVolume);
    sfx_.setVolume(slot.meta.sfxVolume);
    closeMenu();
}

void Game::finishRun() {
    auto& slot = active();
    lastAward_ = world_->shardsForRun();
    slot.meta.shards += lastAward_;
    slot.meta.bestWave = std::max(slot.meta.bestWave, world_->waveIndex());

    // Per-map progress, which is what unlocks the next map.
    auto& mp = slot.meta.mapProgress[runMapId_];
    mp.bestWave = std::max(mp.bestWave, world_->waveIndex());
    if (world_->phase() == sim::Phase::Cleared) mp.cleared = true;

    slot.run.reset();  // the run is over; nothing left to resume
    persist();
    sfx_.play(world_->phase() == sim::Phase::Cleared ? audio::Cue::Victory : audio::Cue::Defeat,
              0.0f, 0.8f);
    screen_ = Screen::Results;
}

void Game::startRun(int goldOverride) {
    // Retained for the dev capture path, which wants a throwaway run with money.
    world_ = std::make_unique<sim::World>(*registry_, registry_->map(runMapId_), 20260830,
                                          core::Loadout{}, goldOverride);
    screen_ = Screen::Playing;
    accumulator_ = 0.0f;
    hud_ = ui::HudState{};
    applySettings();
    closeMenu();
}

void Game::updateSlots() {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    const auto act = ui::slotHitTest(slots_, mouseVirtual());
    if (act.kind == ui::SlotAction::Kind::Delete) {
        core::deleteSlot(act.slot);
        reloadSlots();
        return;
    }
    if (act.kind != ui::SlotAction::Kind::Open) return;

    activeSlot_ = act.slot;
    if (!active().used) {
        active() = core::SaveSlot{};
        active().used = true;
        active().profileName = "Slot " + std::to_string(act.slot + 1);
        persist();
    }
    hubTab_ = 0;
    hubMessage_.clear();
    screen_ = Screen::Hub;
}

void Game::updateHub() {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    const auto act = ui::hubHitTest(*registry_, active(), hubTab_, mouseVirtual());
    switch (act.kind) {
        case ui::HubAction::Kind::SwitchTab:
            hubTab_ = act.tab;
            hubMessage_.clear();
            break;
        case ui::HubAction::Kind::Buy: {
            const int before = active().meta.shards;
            const auto tabs = ui::hubTabs(*registry_);
            if (tabs.empty()) break;
            const auto& tree = registry_->tree(
                tabs[static_cast<size_t>(std::clamp(hubTab_, 0,
                                                    static_cast<int>(tabs.size()) - 1))].treeId);
            const auto r = core::buyNode(tree, active().meta, act.nodeId);
            hubMessage_ = core::describe(r);
            if (r == core::BuyResult::Ok) {
                persist();
                sfx_.play(audio::Cue::Buy, 0.03f, 0.7f);
            } else {
                sfx_.play(audio::Cue::Click, 0.02f, 0.35f);
            }
            (void)before;
            break;
        }
        case ui::HubAction::Kind::ContinueRun: beginRun(true); break;
        case ui::HubAction::Kind::NewRun: screen_ = Screen::Maps; break;
        case ui::HubAction::Kind::BackToSlots:
            reloadSlots();
            screen_ = Screen::Slots;
            break;
        case ui::HubAction::Kind::None: break;
    }
}

void Game::updateMaps() {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    const auto act = ui::mapHitTest(*registry_, active(), mouseVirtual());
    switch (act.kind) {
        case ui::MapAction::Kind::Play: beginRun(false, act.mapId); break;
        case ui::MapAction::Kind::Back: screen_ = Screen::Hub; break;
        case ui::MapAction::Kind::SetDifficulty:
            active().meta.difficulty = act.difficulty;
            persist();
            sfx_.play(audio::Cue::Click, 0.02f, 0.6f);
            break;
        case ui::MapAction::Kind::None: break;
    }
}

void Game::updateResults() {
    if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        reloadSlots();
        screen_ = Screen::Hub;
    }
}

// A specialisation's description is authored on its tree's `<tree>.<spec>.core`
// node -- every one of the 33 specs has a real one. The menu used to replace all
// of them with a single identical sentence about uniqueness, so choosing between
// sniper, elf and hunter told the player nothing about any of them.
std::string specDesc(const content::Registry& reg, const std::string& treeId,
                     const std::string& spec) {
    if (reg.hasTree(treeId)) {
        const auto& tree = reg.tree(treeId);
        const std::string id = treeId + "." + spec + ".core";
        if (const auto* n = tree.find(id)) {
            // Prose says what it IS; the derived numbers say how much. Choosing a
            // specialisation used to be done on flavour text alone -- not one of
            // the 33 said a single figure -- and it is the decision that defines
            // a whole build.
            std::string out = n->desc;
            const auto nums = content::specNumbersLine(tree, id);
            if (!nums.empty()) out += out.empty() ? nums : "  (" + nums + ")";
            if (!out.empty()) return out;
        }
    }
    return "One of each specialisation may exist on the map at a time.";
}

// Builds one page of the menu from what is actually legal right now, so the
// menu can never offer something the world would reject.
namespace {

// The five targeting modes, in the order they are offered. Each detail line says
// what the mode is FOR, not what it literally does -- "first" is meaningless
// until you know it is the leak-stopper.
struct TargetingMode {
    sim::TargetPriority mode;
    const char* label;
    const char* icon;
    const char* detail;
};
constexpr TargetingMode kTargetingModes[] = {
    {sim::TargetPriority::First, "First", "icon_tgt_first",
     "The enemy nearest your goal. The safe default: it stops leaks."},
    {sim::TargetPriority::Last, "Last", "icon_tgt_last",
     "The enemy furthest from your goal. Softens a wave before it arrives."},
    {sim::TargetPriority::Strongest, "Strongest", "icon_tgt_strongest",
     "Most health remaining. Aims at the thing that actually threatens you."},
    {sim::TargetPriority::Weakest, "Weakest", "icon_tgt_weakest",
     "Least health remaining. Finishes the wounded and thins a crowd fast."},
    {sim::TargetPriority::Closest, "Closest", "icon_tgt_closest",
     "Nearest to this tower. Least travel time, so the fewest wasted shots."},
};

const char* targetingName(sim::TargetPriority p) {
    for (const auto& m : kTargetingModes) {
        if (m.mode == p) return m.label;
    }
    return "First";
}

}  // namespace

std::vector<ui::RadialItem> Game::buildMenuItems(int tileX, int tileY, MenuPage page) const {
    using A = ui::RadialItem::Action;
    std::vector<ui::RadialItem> items;

    const auto tower = world_->towerAt(tileX, tileY);

    // Empty ground: the choice is which tower to build. Kingdom Rush shows the
    // tower types directly rather than behind a category, so we do too.
    if (tower == entt::null) {
        // Every tower the content declares, not a hardcoded one. Kingdom Rush
        // shows the tower types directly rather than behind a category, so the
        // ring is the tower list.
        for (const auto& [id, def] : registry_->towers()) {
            const std::string art =
                renderer_.atlas().has("tower_" + id) ? "tower_" + id : "tower_plain";
            const bool open = world_->towerUnlocked(id);
            // Locked towers still show, greyed: seeing what the tree would buy
            // you is the point of having a tree.
            items.push_back({A::Build, art, def.name,
                             open ? def.desc
                                  : "Locked. Buy the charter at the root of the " + def.name +
                                        " tree to unlock it.",
                             id, def.buildCost, true, open});
        }
        return items;
    }

    const auto& tag = world_->reg().get<sim::TowerTag>(tower);
    const int upCost = world_->upgradeCost(tileX, tileY);
    const auto towerSpecs = world_->availableTowerSpecs(tileX, tileY);
    const auto elemSpecs = world_->availableElementSpecs(tileX, tileY);
    const bool canAttach = tag.elementId.empty();

    switch (page) {
        case MenuPage::Root: {
            items.push_back({A::LevelUp,
                             renderer_.atlas().has("ui_icon_08") ? "ui_icon_08" : "icon_level",
                             upCost > 0 ? "Level " + std::to_string(tag.level + 1)
                                        : std::string("Max level"),
                             upCost > 0 ? "More damage, range and fire rate."
                                        : "This tower is fully levelled.",
                             "", upCost > 0 ? upCost : 0, true, upCost > 0});
            // The reason it is unavailable matters more than the fact, so the
            // player knows what to do about it.
            std::string specWhy = "Choose how this tower fights.";
            if (!tag.towerSpec.empty()) specWhy = "This tower is already specialised.";
            else if (!world_->atMaxLevel(tileX, tileY))
                specWhy = "Reach max level first.";
            else if (towerSpecs.empty()) {
                specWhy = "No specialisation unlocked yet. Buy one in the " +
                          registry_->tower(tag.defId).name + " tree.";
            }
            items.push_back({A::OpenSpec, "icon_spec", "Specialise", specWhy, "", 0, true,
                             !towerSpecs.empty()});
            const bool elementAvailable = canAttach || !elemSpecs.empty();
            items.push_back({A::OpenElement, "icon_gem", "Element",
                             canAttach ? "Imbue this tower with an element."
                                       : (elemSpecs.empty() ? "Element fully specialised."
                                                            : "Choose the element's power."),
                             "", 0, true, elementAvailable});
            // Targeting was implemented in the sim and unreachable from the UI.
            // It is the one decision a player can revisit mid-wave without
            // spending anything, which is what makes a wave something to play
            // rather than something to watch.
            items.push_back({A::OpenTargeting, "icon_target", "Targeting",
                             std::string("Currently: ") +
                                 targetingName(world_->towerPriority(tileX, tileY)) +
                                 ". Choose which enemy this tower shoots.",
                             "", 0, true, true});
            items.push_back({A::Sell, "icon_sell", "Remove",
                             "Refunds " + std::to_string(world_->sellValue(tileX, tileY)) +
                                 " gold, " +
                                 std::to_string(static_cast<int>(
                                     registry_->tower(tag.defId).sellRefundPct * 100.0f)) +
                                 "% of everything invested.",
                             "", 0, true, true});
            break;
        }
        case MenuPage::Targeting: {
            const auto current = world_->towerPriority(tileX, tileY);
            for (const auto& [mode, label, icon, detail] : kTargetingModes) {
                const bool active = mode == current;
                items.push_back({A::SetTargeting, icon,
                                 active ? std::string("> ") + label : std::string(label),
                                 detail, sim::World::priorityLabel(mode), 0, true, !active});
            }
            items.push_back({A::Back, "icon_back", "Back", "", "", 0, true, true});
            break;
        }
        case MenuPage::Spec: {
            const int cost = world_->towerSpecCost(tileX, tileY);
            for (const auto& spec : towerSpecs) {
                // The button shows the actual building, so the menu and the
                // board agree about what each specialisation is.
                items.push_back({A::TowerSpec, "tower_" + spec, spec,
                                 specDesc(*registry_, tag.defId, spec), spec, cost, true,
                                 true});
            }
            items.push_back({A::Back, "icon_back", "Back", "", "", 0, true, true});
            break;
        }
        case MenuPage::Element: {
            if (canAttach) {
                // Every element the content declares.
                for (const auto& [eid, edef] : registry_->elements()) {
                    const std::string art =
                        renderer_.atlas().has("icon_" + eid) ? "icon_" + eid : "icon_gem";
                    const bool open = world_->elementUnlocked(eid);
                    // Locked elements still show, so the player can see what
                    // the tree would buy them, with the reason attached.
                    items.push_back({A::AttachElement, art, edef.name,
                                     open ? edef.desc
                                          : "Locked. Buy a node in the " + edef.name +
                                                " tree to unlock it.",
                                     eid, world_->attachElementCost(eid), true, open});
                }
            } else {
                const int cost = world_->elementSpecCost(tileX, tileY);
                for (const auto& spec : elemSpecs) {
                    items.push_back({A::ElementSpec, "icon_" + spec, spec,
                                     specDesc(*registry_, tag.elementId, spec), spec, cost,
                                     true, true});
                }
            }
            items.push_back({A::Back, "icon_back", "Back", "", "", 0, true, true});
            break;
        }
    }
    return items;
}

void Game::showPage(MenuPage page) {
    // No selection means there is nothing to show a menu ABOUT. Without this the
    // ring opened at tile (-1,-1), i.e. pixel (-32,-32), and the new clamping
    // dutifully parked it in the top-left corner -- a menu about nothing.
    if (selX_ < 0 || selY_ < 0) {
        closeMenu();
        return;
    }
    menuPage_ = page;
    auto items = buildMenuItems(selX_, selY_, page);
    if (items.empty()) {
        closeMenu();
        return;
    }
    menu_.open(selX_, selY_,
               {(selX_ + 0.5f) * render::kTile, (selY_ + 0.5f) * render::kTile},
               std::move(items));
}

namespace {

std::string num(float v, int dp = 1) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), dp == 0 ? "%.0f" : "%.1f", v);
    return buf;
}

// Expected damage per second before armour and resistance: what a player
// actually compares two towers on.
float dpsOf(const sim::TowerStats& s) {
    const float critBonus = 1.0f + s.critChance * std::max(0.0f, s.critMult - 1.0f);
    return s.damage * s.fireRate * static_cast<float>(std::max(1, s.projectileCount)) * critBonus;
}

}  // namespace

void Game::drawHoveredEnemy() {
    if (!world_) return;
    const auto m = mouseVirtual();
    if (m.y >= static_cast<float>(render::kPlayH)) return;  // in the HUD band
    if (menu_.isOpen() && menu_.contains(m)) return;        // the menu owns the cursor

    entt::entity best = entt::null;
    float bestDist = 24.0f;  // generous, because enemies are small and moving
    world_->reg().view<const sim::Position, const sim::EnemyTag>().each(
        [&](entt::entity e, const sim::Position& p, const sim::EnemyTag&) {
            const float d = core::distance({p.v.x * render::kTile, p.v.y * render::kTile}, m);
            if (d < bestDist) {
                bestDist = d;
                best = e;
            }
        });
    if (best == entt::null) return;
    const auto& tag = world_->reg().get<sim::EnemyTag>(best);
    ui::drawEnemyDossier(renderer_.atlas(), registry_->enemy(tag.defId), m);
}

void Game::refreshMenuInfo() {
    if (!menu_.isOpen() || !world_) {
        menu_.clearInfo();
        return;
    }
    const auto tower = world_->towerAt(menu_.tileX(), menu_.tileY());
    if (tower == entt::null) {
        menu_.clearInfo();  // empty ground: the build ring speaks for itself
        return;
    }

    const auto& tag = world_->reg().get<sim::TowerTag>(tower);
    const auto& st = world_->reg().get<sim::TowerStats>(tower);
    const auto& def = registry_->tower(tag.defId);

    // Only when the upgrade item is hovered, so the panel is a readout the rest
    // of the time and a comparison exactly when it matters.
    const int hovered = menu_.hitTest(mouseVirtual());
    bool previewing = false;
    sim::TowerStats next;
    if (hovered >= 0 && menu_.items()[static_cast<size_t>(hovered)].action ==
                            ui::RadialItem::Action::LevelUp) {
        previewing = world_->previewUpgrade(menu_.tileX(), menu_.tileY(), next);
    }
    const auto row = [&](const char* label, float now, float nxt, int dp) {
        ui::StatRow r{label, num(now, dp), {}};
        if (previewing && std::abs(nxt - now) > 1e-3f) r.next = num(nxt, dp);
        return r;
    };

    std::vector<ui::StatRow> rows;
    rows.push_back(row("damage", st.damage, next.damage, 0));
    rows.push_back(row("fire rate", st.fireRate, next.fireRate, 1));
    rows.push_back(row("dps", dpsOf(st), previewing ? dpsOf(next) : 0.0f, 0));
    rows.push_back(row("range", st.range, next.range, 1));
    // Text, not a number, so it cannot use row(); the panel is the only place a
    // player can see how this tower is aimed without opening the menu.
    rows.push_back(ui::StatRow{"targeting", targetingName(world_->towerPriority(selX_, selY_)), {}});
    if (st.projectileCount > 1) {
        rows.push_back(row("shots", static_cast<float>(st.projectileCount),
                           static_cast<float>(next.projectileCount), 0));
    }
    if (st.pierce > 0) {
        rows.push_back(row("pierce", static_cast<float>(st.pierce),
                           static_cast<float>(next.pierce), 0));
    }
    if (st.critChance > 0.0f) {
        rows.push_back(row("crit %", st.critChance * 100.0f, next.critChance * 100.0f, 0));
    }
    if (st.armorPen > 0.0f) {
        rows.push_back(row("armour pen", st.armorPen, next.armorPen, 0));
    }
    // The damage type is what interacts with enemy resistance, so it belongs
    // beside the numbers rather than buried in content.
    rows.push_back(ui::StatRow{"damage type", st.damageType, {}});
    if (!tag.elementSpec.empty()) {
        rows.push_back(ui::StatRow{"element", tag.elementSpec, {}});
    } else if (!tag.elementId.empty()) {
        rows.push_back(ui::StatRow{"element", tag.elementId + " (unspecialised)", {}});
    }

    std::string title = def.name;
    if (!tag.towerSpec.empty()) title = tag.towerSpec;
    title += "  L" + std::to_string(tag.level);
    menu_.setInfo(title, std::move(rows));
}

bool Game::openMenuAt(int tileX, int tileY) {
    if (world_->towerAt(tileX, tileY) == entt::null &&
        !world_->map().buildableAt(tileX, tileY)) {
        return false;
    }
    selX_ = tileX;
    selY_ = tileY;
    showPage(MenuPage::Root);
    return menu_.isOpen();
}

void Game::applyMenuItem(const ui::RadialItem& item) {
    using A = ui::RadialItem::Action;
    const int x = menu_.tileX(), y = menu_.tileY();

    // Categories just change page; they cost nothing and can always be taken.
    switch (item.action) {
        case A::OpenSpec:    if (item.enabled) showPage(MenuPage::Spec); return;
        case A::OpenElement: if (item.enabled) showPage(MenuPage::Element); return;
        case A::OpenTargeting: showPage(MenuPage::Targeting); return;
        case A::Back:        showPage(MenuPage::Root); return;
        case A::SetTargeting:
            // Free and instant: retargeting is a tactical decision, not a
            // purchase, so it must never cost gold or a build slot.
            for (const auto& m : kTargetingModes) {
                if (item.arg == sim::World::priorityLabel(m.mode)) {
                    world_->setTowerPriority(x, y, m.mode);
                    break;
                }
            }
            sfx_.play(audio::Cue::Click, 0.02f, 0.6f);
            showPage(MenuPage::Targeting);  // stay put, so the tick moves visibly
            return;
        case A::Sell:
            world_->sellTower(x, y);
            sfx_.play(audio::Cue::Sell);
            closeMenu();
            return;
        default: break;
    }

    bool ok = false;
    switch (item.action) {
        case A::Build:
            ok = world_->placeTower(x, y, item.arg) == sim::World::PlaceResult::Ok;
            break;
        case A::LevelUp: ok = world_->upgradeTower(x, y); break;
        case A::AttachElement: ok = world_->attachElement(x, y, item.arg); break;
        case A::TowerSpec: ok = world_->specialiseTower(x, y, item.arg); break;
        case A::ElementSpec: ok = world_->specialiseElement(x, y, item.arg); break;
        default: break;
    }

    if (!ok) {
        say("not enough gold");
        sfx_.play(audio::Cue::Click, 0.02f, 0.35f);
        return;
    }
    sfx_.play(item.action == A::Build ? audio::Cue::Build : audio::Cue::Buy, 0.04f, 0.55f);
    // Back to the root so the newly-unlocked categories are visible at once.
    showPage(MenuPage::Root);
}

void Game::closeMenu() {
    menu_.close();
    selX_ = selY_ = -1;
}

// One click, one decision. This used to be spread across three overlapping
// `if` blocks, which let a single click be consumed dismissing the old menu
// without opening the new one -- the reason building appeared to need a
// double click. The whole flow is now a single ordered decision tree.
void Game::handleBuildInput() {
    const core::Vec2 v = mouseVirtual();
    const bool inPlay = v.x >= 0 && v.y >= 0 && v.x < render::kPlayW && v.y < render::kPlayH;
    const int tx = inPlay ? static_cast<int>(v.x) / render::kTile : -1;
    const int ty = inPlay ? static_cast<int>(v.y) / render::kTile : -1;

    // Hover stays live even with the menu open, so the next target is visible.
    hoverX_ = tx;
    hoverY_ = ty;

    if (IsKeyPressed(KEY_ESCAPE)) closeMenu();
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    // 1. The HUD owns clicks outside the playfield.
    if (!inPlay) {
        switch (ui::hudHitTest(v)) {
            case ui::HudButton::NextWave:
                if (world_->canCallWave()) sfx_.play(audio::Cue::WaveStart, 0.02f);
                world_->startNextWave();
                return;
            case ui::HudButton::Speed:
                hud_.speedIndex = (hud_.speedIndex + 1) % 4;
                sfx_.play(audio::Cue::Click);
                return;
            case ui::HudButton::Strike:
                hud_.armed = hud_.armed == 0 ? -1 : 0;
                return;
            case ui::HudButton::Ward:
                hud_.armed = hud_.armed == 1 ? -1 : 1;
                return;
            case ui::HudButton::Pause:
                hud_.paused = !hud_.paused;
                sfx_.play(audio::Cue::Click);
                return;
            case ui::HudButton::Quit:
                // Leaving mid-run keeps the autosave, so Continue picks it up.
                sfx_.play(audio::Cue::Click);
                reloadSlots();
                screen_ = Screen::Hub;
                return;
            case ui::HudButton::Mute:
                sfx_.setMuted(!sfx_.muted());
                hud_.muted = sfx_.muted();
                sfx_.play(audio::Cue::Click);
                return;
            case ui::HudButton::None: closeMenu(); return;
        }
    }

    // 2. A click on one of the open menu's buttons is that button's click.
    if (menu_.isOpen()) {
        const int hit = menu_.hitTest(v);
        if (hit >= 0) {
            applyMenuItem(menu_.items()[static_cast<size_t>(hit)]);
            return;
        }
    }

    // 3. Otherwise the click targets a tile. Always opens on the FIRST press.
    closeMenu();
    if (!openMenuAt(tx, ty)) {
        if (world_->towerAt(tx, ty) == entt::null && !world_->map().buildableAt(tx, ty)) {
            say("cannot build there");
        }
    }
}

void Game::updatePlaying(float frameDt) {
    // P is the tactical pause; ESC is the settings modal. They used to be the
    // same key and the same screen, which meant the only way to stop and think
    // was to open a menu that covered the board and refused all input.
    if (IsKeyPressed(KEY_P)) hud_.paused = !hud_.paused;
    if (IsKeyPressed(KEY_ESCAPE)) hud_.menuOpen = !hud_.menuOpen;

    // The settings modal owns the cursor while it is up: without this, clicking
    // RESUME would also place a tower on the tile underneath it.
    if (hud_.menuOpen) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const auto act = ui::pauseHitTest(mouseVirtual());
            switch (act.kind) {
                // Sliders respond to HELD, not pressed, so they can be dragged.
                case ui::PauseAction::Kind::SetMusic:
                    active().meta.musicVolume = act.value;
                    jukebox_.setVolume(act.value);
                    break;
                case ui::PauseAction::Kind::SetSfx:
                    active().meta.sfxVolume = act.value;
                    sfx_.setVolume(act.value);
                    break;
                default: break;
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const auto act = ui::pauseHitTest(mouseVirtual());
            if (act.kind == ui::PauseAction::Kind::ToggleColorAlt) {
                active().meta.colorAlternatives = !active().meta.colorAlternatives;
                applySettings();
                persist();
            } else if (act.kind == ui::PauseAction::Kind::ToggleScaling) {
                active().meta.integerScaling = !active().meta.integerScaling;
                applySettings();
                persist();
            } else if (act.kind == ui::PauseAction::Kind::CycleShake) {
                const int next = (core::shakeIndexOf(active().meta.shake) + 1) % 3;
                active().meta.shake = core::kShakeLevels[next];
                applySettings();
                persist();
            } else if (act.kind == ui::PauseAction::Kind::Resume) {
                hud_.menuOpen = false;
            } else if (act.kind == ui::PauseAction::Kind::Quit) {
                hud_.menuOpen = false;
                persist();  // volumes and the autosaved run
                reloadSlots();
                screen_ = Screen::Hub;
                return;
            }
        }
        return;  // frozen and modal: settings only
    }

    announceWaves();
    updateTutorial();
    updateHints();

    // The tutorial's SKIP button owns its own rectangle, so clicking it never
    // also builds a tower underneath.
    if (tutorialActive() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const auto m = mouseVirtual();
        if (m.x >= tutorialBox_.skipX && m.x < tutorialBox_.skipX + tutorialBox_.skipW &&
            m.y >= tutorialBox_.skipY && m.y < tutorialBox_.skipY + tutorialBox_.skipH) {
            active().meta.tutorialStep = sim::tutorialToIndex(sim::TutorialStep::Done);
            persist();
            sfx_.play(audio::Cue::Click, 0.02f, 0.5f);
            return;
        }
    }

    // Abilities. Q and W arm; the next click on the board casts. Escape or a
    // right-click disarms, so an armed ability is never a trap.
    if (IsKeyPressed(KEY_Q)) hud_.armed = hud_.armed == 0 ? -1 : 0;
    if (IsKeyPressed(KEY_W)) hud_.armed = hud_.armed == 1 ? -1 : 1;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) hud_.armed = -1;
    if (hud_.armed >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const auto m = mouseVirtual();
        if (m.y < render::kPlayH) {
            const core::Vec2 tile{m.x / render::kTile, m.y / render::kTile};
            const auto which = static_cast<sim::Ability>(hud_.armed);
            if (world_->castAbility(which, tile)) {
                sfx_.play(which == sim::Ability::Strike ? audio::Cue::Quake : audio::Cue::Buy);
                hud_.armed = -1;
                return;  // the click was the cast, not a build
            }
        }
    }

    handleBuildInput();
    if (IsKeyPressed(KEY_SPACE)) world_->startNextWave();
    if (IsKeyPressed(KEY_F)) hud_.speedIndex = (hud_.speedIndex + 1) % 4;

    const float scaled =
        (hud_.paused || hud_.menuOpen) ? 0.0f : frameDt * kSpeeds[hud_.speedIndex];
    accumulator_ += scaled;
    if (accumulator_ > 0.25f) accumulator_ = 0.25f;  // spiral-of-death guard
    while (accumulator_ >= sim::kFixedDt) {
        world_->tick(sim::kFixedDt);
        accumulator_ -= sim::kFixedDt;
    }

    hud_.messageAge += frameDt;
    {
        const auto events = world_->drainEvents();
        renderer_.update(scaled, events, *registry_);
        sfx_.handle(events);
    }

    if (world_->phase() == sim::Phase::Cleared || world_->phase() == sim::Phase::Defeated) {
        if (activeSlot_ >= 0) finishRun();
        return;
    }

    maybeAutosave();
}

std::vector<render::PostFx::Light> Game::collectLights() const {
    std::vector<render::PostFx::Light> out;
    if (!world_) return out;

    // Element powers colour their tower's light, so the board tells you what is
    // fielded without reading the HUD.
    const auto tintFor = [](const std::string& spec) -> Color {
        if (spec == "poison") return Color{150, 230, 130, 255};
        if (spec == "rock") return Color{192, 176, 228, 255};
        if (spec == "quake") return Color{246, 182, 112, 255};
        return Color{255, 208, 142, 255};  // plain torchlight
    };

    world_->reg().view<const sim::Position, const sim::TowerTag>().each(
        [&](const sim::Position& pos, const sim::TowerTag& tag) {
            if (static_cast<int>(out.size()) >= render::PostFx::kMaxLights) return;
            render::PostFx::Light l;
            l.pos = {pos.v.x * render::kTile, pos.v.y * render::kTile};
            l.color = tintFor(tag.elementSpec);
            // Tighter than before: at 132+20/level a cluster of towers sat
            // entirely inside every neighbour's pool.
            l.radius = 96.0f + 16.0f * static_cast<float>(tag.level);
            out.push_back(l);
        });
    return out;
}

void Game::renderCanvas(float alpha) {
    canvas_.begin();
    switch (screen_) {
        case Screen::ArtCompare:
            renderer_.drawArtCompare();
            break;
        case Screen::SpriteSheet:
            renderer_.drawSpriteSheet();
            break;
        case Screen::Slots:
            ui::drawSlots(renderer_.atlas(), slots_, mouseVirtual());
            break;
        case Screen::Hub:
            ui::drawHub(renderer_.atlas(), *registry_, active(), hubTab_, hubMessage_,
                        mouseVirtual());
            break;
        case Screen::Maps:
            ui::drawMaps(renderer_.atlas(), *registry_, active(), mouseVirtual());
            break;

        case Screen::Playing: {
            render::Cursor cur;
            cur.hoverX = hoverX_;
            cur.hoverY = hoverY_;
            cur.selX = selX_;
            cur.selY = selY_;
            // Pads lift while the player is actually choosing a spot: during the
            // build phase, or whenever a menu is open on the board.
            cur.showSites = world_->phase() == sim::Phase::Build || menu_.isOpen();
            cur.armedAbility = hud_.armed;
            if (hoverX_ >= 0) {
                cur.hoverBuildable = world_->map().buildableAt(hoverX_, hoverY_) &&
                                     world_->towerAt(hoverX_, hoverY_) == entt::null;
                // The cheapest tower this profile can build, not always "arrow":
                // once the starting tower is no longer the only one, quoting its
                // price and its range is simply wrong.
                const std::string preview = world_->cheapestUnlockedTower();
                if (!preview.empty() && cur.hoverBuildable) {
                    cur.hoverAffordable = world_->gold() >= registry_->tower(preview).buildCost;
                    cur.previewRange = world_->buildRange(preview);
                }
            }
            renderer_.draw(*world_, alpha, cur);
            ui::drawBossBars(renderer_.atlas(), *world_);
            refreshMenuInfo();
            menu_.draw(renderer_.atlas(), mouseVirtual(), world_->gold());
            ui::drawHud(renderer_.atlas(), *world_, hud_, mouseVirtual());
            drawHoveredEnemy();
            if (tutorialActive()) {
                const auto pr = sim::tutorialPrompt(
                    sim::tutorialFromIndex(activeConst().meta.tutorialStep));
                tutorialBox_ = ui::drawTutorial(renderer_.atlas(), pr.title, pr.body,
                                                mouseVirtual());
            }
            if (hud_.menuOpen) {
                ui::drawPause(renderer_.atlas(), active().meta.musicVolume,
                              active().meta.sfxVolume, active().meta.colorAlternatives,
                              active().meta.shake, active().meta.integerScaling,
                              mouseVirtual());
            } else if (hud_.paused) {
                // A slim banner, not a curtain: the whole point of the tactical
                // pause is that the board stays readable and clickable.
                ui::drawPausedBanner();
            }
            break;
        }
        case Screen::Results:
            renderer_.draw(*world_, alpha, render::Cursor{});
            ui::drawResults(renderer_.atlas(), *world_, lastAward_,
                            activeSlot_ >= 0 ? slots_[static_cast<size_t>(activeSlot_)].meta.shards
                                             : 0);
            break;
    }
    canvas_.end();

    // The atmosphere pass grades and darkens the play field and lights it back
    // around the towers. Only the world screens want it; the slot select and the
    // skill trees are interface, and grading a parchment panel looks like a bug.
    const bool worldScreen = (screen_ == Screen::Playing || screen_ == Screen::Results);
    postfx_.setEnabled(worldScreen);
    if (worldScreen && world_) postfx_.setLights(collectLights());

    BeginDrawing();
    ClearBackground(BLACK);
    postfx_.beginPass();
    canvas_.blitToWindow();
    postfx_.endPass();
    EndDrawing();
}

void Game::frame(float frameDt) {
    // A run gets the battle track; every menu gets the hub track. Results keeps
    // the battle track, because the run has only just ended.
    const bool inRun = screen_ == Screen::Playing || screen_ == Screen::Results;
    jukebox_.setTrack(inRun ? core::Track::Battle : core::Track::Hub);
    jukebox_.update(frameDt);

    switch (screen_) {
        case Screen::Slots: updateSlots(); break;
        case Screen::Hub: updateHub(); break;
        case Screen::Playing: updatePlaying(frameDt); break;
        case Screen::Maps: updateMaps(); break;
        case Screen::Results: updateResults(); break;
        case Screen::ArtCompare: break;
        case Screen::SpriteSheet: break;
    }
    renderCanvas(screen_ == Screen::Playing ? accumulator_ / sim::kFixedDt : 0.0f);
}

}  // namespace td::app
