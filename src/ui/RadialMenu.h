#pragma once

#include <string>
#include <vector>

#include "core/Vec2.h"
#include "render/SpriteAtlas.h"

namespace td::ui {

// A Kingdom Rush style context menu: circular icon buttons arranged around the
// thing you clicked, so the UI is attached to the object rather than floating in
// a panel over the playfield.
struct RadialItem {
    // The menu is hierarchical: the root offers CATEGORIES, and each category
    // opens its own ring of concrete choices. A flat ring stops working the
    // moment a tower has more than a handful of options.
    enum class Action {
        // categories
        OpenSpec,
        OpenElement,
        OpenTargeting,
        Back,
        // leaves
        Build,
        LevelUp,
        AttachElement,
        TowerSpec,
        ElementSpec,
        SetTargeting,
        Sell,
    };

    Action action = Action::Build;
    std::string icon;
    std::string label;
    std::string detail;   // one-line explanation shown while hovered
    std::string arg;      // spec id, where the action needs one
    int cost = 0;
    bool affordable = true;
    bool enabled = true;  // greyed-out categories still show, so the shape is stable
};

// One line of the selected tower's stat readout. `next` is filled only while the
// player is hovering the upgrade option, so the panel can show `current -> next`
// and an upgrade becomes a decision rather than a cost.
struct StatRow {
    std::string label;
    std::string value;
    std::string next;  // empty when not previewing
};

class RadialMenu {
public:
    void open(int tileX, int tileY, core::Vec2 centre, std::vector<RadialItem> items);
    // Exposed for tests: where the ring will actually be centred, given where it
    // was asked to appear.
    static core::Vec2 clampCentre(core::Vec2 c);
    static float ringRadius();
    void close();
    bool isOpen() const { return open_; }

    int tileX() const { return tileX_; }
    int tileY() const { return tileY_; }
    const std::vector<RadialItem>& items() const { return items_; }

    // Index under the cursor, or -1. Also reports whether the cursor is inside
    // the menu at all, which is what decides a click-outside dismissal.
    int hitTest(core::Vec2 mouse) const;
    bool contains(core::Vec2 mouse) const;

    // Set by the caller, which owns the world; the menu stays presentation-only.
    void setInfo(std::string title, std::vector<StatRow> rows) {
        infoTitle_ = std::move(title);
        info_ = std::move(rows);
    }
    void clearInfo() {
        infoTitle_.clear();
        info_.clear();
    }

    void draw(const render::SpriteAtlas& atlas, core::Vec2 mouse, int gold) const;

private:
    core::Vec2 slotOf(size_t i) const;

    bool open_ = false;
    int tileX_ = -1, tileY_ = -1;
    core::Vec2 centre_;
    std::vector<RadialItem> items_;
    std::string infoTitle_;
    std::vector<StatRow> info_;
};

}  // namespace td::ui
