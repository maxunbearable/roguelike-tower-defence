#include "ui/Screens.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"

#include "core/Progression.h"
#include "render/Palette.h"
#include "render/PixelCanvas.h"
#include "ui/Paint.h"

namespace td::ui {
namespace {
using namespace td::render;
using namespace td::ui::paint;

// --- slot geometry --------------------------------------------------------
// One constant, so renaming the game is a one-line change.
constexpr const char* kGameTitle = "WARDSTONE";

constexpr int kSlotW = 330, kSlotH = 356;
constexpr int kSlotGap = 40;
constexpr int kSlotTop = 226;
int slotX(int i) {
    const int total = core::kSlotCount * kSlotW + (core::kSlotCount - 1) * kSlotGap;
    return (kVirtualW - total) / 2 + i * (kSlotW + kSlotGap);
}
constexpr int kDelW = 76, kDelH = 26;

// --- hub geometry ---------------------------------------------------------
// TWO ROWS of tabs. Twelve trees at one row of 118px would need 2064px on a
// 1408px board; six per row gives each tab room for a readable label and keeps
// the ribbon tails from colliding.
constexpr int kTabsPerRow = 6;
constexpr int kTabY = 62, kTabH = 32, kTabW = 200, kTabRowH = 38;
int tabX(int i) { return 24 + (i % kTabsPerRow) * (kTabW + 32); }
int tabY(int i) { return kTabY + (i / kTabsPerRow) * kTabRowH; }

// The tree panel, and the box inside it the tree is centred on.
constexpr int kPanelX = 24, kPanelY = 146;
constexpr int kPanelW = kVirtualW - 2 * kPanelX;
constexpr int kPanelH = 578;
constexpr int kTreeBoxY = kPanelY + 66;
constexpr int kTreeBoxH = kPanelH - 66 - 30;

// Roughly double the prototype's, which drew 23 named nodes as unlabelled
// 17px circles in the top quarter of the screen.
constexpr int kNodeSpacingX = 150, kNodeSpacingY = 128, kNodeR = 30;

constexpr int kBtnW = 240, kBtnH = 44, kBtnY = kVirtualH - 52;
constexpr int kBtnContinueX = 24;
constexpr int kBtnNewX = 24 + kBtnW + 12;
constexpr int kBtnBackW = 200;
constexpr int kBtnBackX = kVirtualW - kBtnBackW - 24;



bool inRect(core::Vec2 m, int x, int y, int w, int h) {
    return m.x >= x && m.y >= y && m.x < x + w && m.y < y + h;
}

// Where the tree's own coordinate origin lands on screen. Derived from the
// node extents so each of the three trees -- which differ in width and depth --
// sits centred in the panel, instead of every tree hanging off a shared
// top-left origin with the narrow ones stranded in a corner.
struct TreeLayout {
    float originX = 0, originY = 0;
    float stepX = kNodeSpacingX, stepY = kNodeSpacingY;
    float radius = kNodeR;
};

TreeLayout treeLayout(const core::SkillTree& tree) {
    TreeLayout L{kVirtualW / 2.0f, kTreeBoxY + kTreeBoxH / 2.0f};
    if (tree.nodes.empty()) return L;

    int minX = tree.nodes[0].x, maxX = minX, minY = tree.nodes[0].y, maxY = minY;
    for (const auto& n : tree.nodes) {
        minX = std::min(minX, n.x);
        maxX = std::max(maxX, n.x);
        minY = std::min(minY, n.y);
        maxY = std::max(maxY, n.y);
    }
    const int cols = maxX - minX;
    const int rows = maxY - minY;

    // Spacing SHRINKS to fit. The global tree grew from 2 rows to 6 and clipped
    // straight through the panel, so the layout adapts to the tree instead of
    // the tree having to fit a fixed grid. Node labels sit below each circle, so
    // a row needs a little more than its spacing.
    const float availW = static_cast<float>(kPanelW) - 90.0f;
    // The bottom row still needs its name and cost underneath, which sit about
    // 34px below the circle's centre.
    const float availH = static_cast<float>(kTreeBoxH) - 64.0f;
    if (cols > 0) L.stepX = std::min(L.stepX, availW / static_cast<float>(cols));
    if (rows > 0) L.stepY = std::min(L.stepY, availH / static_cast<float>(rows));
    // The circle has to leave room for the name and cost underneath it.
    L.radius = std::min(kNodeR * 1.0f, std::min(L.stepX * 0.34f, L.stepY * 0.30f));

    const float midX = (minX + maxX) / 2.0f;
    const float midY = (minY + maxY) / 2.0f;
    L.originX = kVirtualW / 2.0f - midX * L.stepX;
    L.originY = kTreeBoxY + kTreeBoxH / 2.0f - midY * L.stepY;
    return L;
}

core::Vec2 nodePos(const TreeLayout& L, const core::SkillNode& n) {
    return {L.originX + static_cast<float>(n.x) * L.stepX,
            L.originY + static_cast<float>(n.y) * L.stepY};
}

void centred(const char* t, int y, int size, Color c) {
    DrawText(t, (kVirtualW - MeasureText(t, size)) / 2, y, size, c);
}

// Branch colour coding. Path of Exile's trick: the eye groups by hue long
// before it reads any label, which is what makes three specialisation paths
// legible at a glance instead of one undifferentiated web.
Color mix(Color a, Color b, float t) {
    const auto k = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(x + (y - x) * t);
    };
    return {k(a.r, b.r), k(a.g, b.g), k(a.b, b.b), k(a.a, b.a)};
}

// An icon for EVERY node, not just the branch cores. Derived rather than
// authored: a tower tree's trunk shows its building, an element tree's trunk
// shows its orb, a branch node shows what that branch unlocks, and a global node
// shows the kind of stat it raises.
std::string nodeIcon(const render::SpriteAtlas& atlas, const core::SkillTree& tree,
                     const core::SkillNode& n) {
    const auto pick = [&atlas](std::initializer_list<std::string> ids) -> std::string {
        for (const auto& id : ids) {
            if (!id.empty() && atlas.has(id)) return id;
        }
        return {};
    };

    if (n.branch != "trunk") {
        // What this branch is: the specialised building, or the element power.
        return pick({"tower_" + n.branch, "icon_" + n.branch});
    }

    if (tree.kind == core::SkillTree::Kind::Tower) return pick({"tower_" + tree.id});
    if (tree.kind == core::SkillTree::Kind::Element) return pick({"icon_" + tree.id});

    // Global: read the stat it touches. Modifiers on a global node all move the
    // same stat across every tower, so the first one is representative.
    const std::string target = n.modifiers.empty() ? std::string{} : n.modifiers.front().target;
    const auto ends = [&target](const char* suffix) {
        const std::string s(suffix);
        return target.size() >= s.size() &&
               target.compare(target.size() - s.size(), s.size(), s) == 0;
    };
    if (target.rfind("global.unlock", 0) == 0) return pick({"icon_level", "ui_icon_08"});
    if (target == "global.startGold") return pick({"icon_coin"});
    if (target == "global.lives") return pick({"icon_heart"});
    if (ends(".damage")) return pick({"icon_spec", "ui_icon_08"});
    if (ends(".fireRate")) return pick({"icon_ffwd"});
    if (ends(".range")) return pick({"ui_icon_09", "icon_build"});
    if (ends(".critChance") || ends(".critMult")) return pick({"ui_icon_01", "icon_spec"});
    if (ends(".armorPen")) return pick({"ui_icon_08", "icon_spec"});
    return pick({"icon_gem"});
}

Color branchTint(const std::string& branch) {
    if (branch == "sniper") return Color{126, 170, 214, 255};
    if (branch == "elf") return Color{132, 202, 130, 255};
    if (branch == "hunter") return Color{226, 168, 88, 255};
    if (branch == "poison") return Color{158, 204, 106, 255};
    if (branch == "rock") return Color{176, 162, 196, 255};
    if (branch == "quake") return Color{206, 156, 104, 255};
    return Color{198, 178, 148, 255};  // trunk
}

}  // namespace

// ==========================================================================
// Slot select
// ==========================================================================

SlotAction slotHitTest(const std::vector<core::SaveSlot>& slots, core::Vec2 m) {
    for (int i = 0; i < core::kSlotCount && i < static_cast<int>(slots.size()); ++i) {
        const int x = slotX(i);
        if (slots[static_cast<size_t>(i)].used &&
            inRect(m, x + kSlotW - kDelW - 10, kSlotTop + kSlotH - kDelH - 10, kDelW, kDelH)) {
            return {SlotAction::Kind::Delete, i};
        }
        if (inRect(m, x, kSlotTop, kSlotW, kSlotH)) return {SlotAction::Kind::Open, i};
    }
    return {};
}

void drawSlots(const render::SpriteAtlas& atlas, const std::vector<core::SaveSlot>& slots,
               core::Vec2 mouse) {
    DrawRectangle(0, 0, kVirtualW, kVirtualH, palette::kBackdrop);

    // The title was the placeholder string "TOWER DEFENSE". A ribbon is only
    // 32px tall, so a 40px title needs the banner plate instead.
    if (available(atlas) && atlas.has("ui_banner")) {
        atlas.drawNine("ui_banner", kVirtualW / 2.0f - 290.0f, 30.0f, 580.0f, 104.0f);
    }
    centredIn(kGameTitle, kVirtualW / 2, 58, 40, kInk);
    centred("choose a profile", 152, 20, palette::kHudDim);

    const auto hit = slotHitTest(slots, mouse);

    for (int i = 0; i < core::kSlotCount; ++i) {
        const int x = slotX(i);
        const bool hot = hit.kind != SlotAction::Kind::None && hit.slot == i;
        const auto& sl = slots[static_cast<size_t>(i)];
        const bool used = i < static_cast<int>(slots.size()) && sl.used;

        panel(atlas, x, kSlotTop, kSlotW, kSlotH);
        if (hot) {
            DrawRectangleLines(x + 4, kSlotTop + 4, kSlotW - 8, kSlotH - 8,
                               Color{255, 236, 170, 200});
        }

        // Slot heading on its own ribbon, so an occupied profile reads as a
        // real save rather than a label on a box.
        ribbon(atlas, used ? "ui_ribbon" : "ui_ribbon_blue", x + 22, kSlotTop + 14,
               kSlotW - 44);
        centredIn(TextFormat("SLOT %d", i + 1), x + kSlotW / 2, kSlotTop + 22, 20, kInk);

        if (!used) {
            centredIn("EMPTY", x + kSlotW / 2, kSlotTop + 92, 40, kInkDim);
            centredIn("click to begin a new game", x + kSlotW / 2, kSlotTop + 142, 10, kInkDim);
            continue;
        }

        atlas.drawFitted("icon_gem", static_cast<float>(x + 40), kSlotTop + 84.0f, 24);
        DrawText(TextFormat("%d shards", sl.meta.shards), x + 58, kSlotTop + 74, 20,
                 kInkWarn);

        struct Row { const char* label; const char* value; };
        const Row rows[] = {
            {"best wave", TextFormat("%d", sl.meta.bestWave)},
            {"runs played", TextFormat("%d", sl.meta.runsPlayed)},
            {"skills owned", TextFormat("%d", static_cast<int>(sl.meta.ownedNodes.size()))},
        };
        int ry = kSlotTop + 114;
        for (const auto& r : rows) {
            DrawText(r.label, x + 26, ry, 10, kInkDim);
            DrawText(r.value, x + kSlotW - 26 - MeasureText(r.value, 10), ry, 10, kInk);
            ry += 20;
        }

        // CONTINUE is the action the player wants on a slot mid-run, so it gets
        // the emphasis rather than being a line of status text.
        if (sl.hasRunInProgress()) {
            const int by = kSlotTop + kSlotH - 104;
            button(atlas, x + 24, by, kSlotW - 48, 36, hot, /*on=*/false);
            centredIn(TextFormat("CONTINUE  wave %d", sl.run->waveIndex + 1), x + kSlotW / 2,
                      by + 13, 10, kInk);
        } else {
            centredIn("no run in progress", x + kSlotW / 2, kSlotTop + kSlotH - 94, 10,
                      kInkDim);
        }

        const bool delHot = hit.kind == SlotAction::Kind::Delete && hit.slot == i;
        const int dx = x + kSlotW - kDelW - 20, dy = kSlotTop + kSlotH - kDelH - 24;
        button(atlas, dx, dy, kDelW, kDelH, delHot, /*on=*/true);
        centredIn("ERASE", dx + kDelW / 2, dy + 8, 10, kInk);
    }

    centred("click a profile to enter its skill trees", kVirtualH - 56, 10,
            palette::kHudDim);
}

// ==========================================================================
// Hub: skill trees plus the run controls
// ==========================================================================

std::vector<HubTab> hubTabs(const content::Registry& reg) {
    std::vector<HubTab> out;
    const auto label = [&](const core::SkillTree& t) -> std::string {
        // Prefer the tower/element display name, so the tab says "Arcane Spire"
        // rather than "ARCANE".
        if (reg.hasTower(t.id)) return reg.tower(t.id).name;
        if (reg.hasElement(t.id)) return reg.element(t.id).name;
        return t.id;
    };
    for (const auto& [id, t] : reg.trees()) {
        if (t.kind == core::SkillTree::Kind::Global) out.push_back({id, "GLOBAL"});
    }
    for (const auto& [id, t] : reg.trees()) {
        if (t.kind == core::SkillTree::Kind::Tower) out.push_back({id, label(t)});
    }
    for (const auto& [id, t] : reg.trees()) {
        if (t.kind == core::SkillTree::Kind::Element) out.push_back({id, label(t)});
    }
    return out;
}

HubAction hubHitTest(const content::Registry& reg, const core::SaveSlot& slot, int tab,
                     core::Vec2 m) {
    const auto tabs = hubTabs(reg);
    const int nTabs = static_cast<int>(tabs.size());
    for (int i = 0; i < nTabs; ++i) {
        if (inRect(m, tabX(i), tabY(i), kTabW, kTabH)) {
            return {HubAction::Kind::SwitchTab, {}, i};
        }
    }
    if (slot.hasRunInProgress() && inRect(m, kBtnContinueX, kBtnY, kBtnW, kBtnH)) {
        return {HubAction::Kind::ContinueRun};
    }
    if (inRect(m, kBtnNewX, kBtnY, kBtnW, kBtnH)) return {HubAction::Kind::NewRun};
    if (inRect(m, kBtnBackX, kBtnY, kBtnBackW, kBtnH)) return {HubAction::Kind::BackToSlots};

    if (nTabs == 0) return {};
    const std::string id = tabs[static_cast<size_t>(std::clamp(tab, 0, nTabs - 1))].treeId;
    if (reg.hasTree(id)) {
        const auto& tree = reg.tree(id);
        const auto L = treeLayout(tree);
        for (const auto& n : tree.nodes) {
            if (core::distance(nodePos(L, n), m) <= L.radius + 2) {
                return {HubAction::Kind::Buy, n.id, tab};
            }
        }
    }
    return {};
}

void drawHub(const render::SpriteAtlas& atlas, const content::Registry& reg,
             const core::SaveSlot& slot, int tab, const std::string& message, core::Vec2 mouse) {
    DrawRectangle(0, 0, kVirtualW, kVirtualH, palette::kBackdrop);
    const auto tabs = hubTabs(reg);
    if (tabs.empty()) return;
    tab = std::clamp(tab, 0, static_cast<int>(tabs.size()) - 1);
    const auto hit = hubHitTest(reg, slot, tab, mouse);

    // Heading on a ribbon, shard purse opposite it.
    ribbon(atlas, "ui_ribbon", kVirtualW / 2 - 170, 10, 340);
    centredIn("SKILL TREES", kVirtualW / 2, 18, 20, kInk);

    atlas.drawFitted("icon_gem", kVirtualW - 156.0f, 26.0f, 22);
    DrawText(TextFormat("%d shards", slot.meta.shards), kVirtualW - 140, 18, 20,
             palette::kHudWarn);

    for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
        const bool active = i == tab;
        const bool hot = hit.kind == HubAction::Kind::SwitchTab && hit.tab == i;
        if (available(atlas)) {
            ribbon(atlas, active ? "ui_ribbon" : "ui_ribbon_blue", tabX(i), tabY(i), kTabW);
        } else {
            frame(tabX(i), tabY(i), kTabW, kTabH, hot, active);
        }
        centredIn(tabs[static_cast<size_t>(i)].label.c_str(), tabX(i) + kTabW / 2, tabY(i) + 10,
                  10,
                  active || hot ? kInk : Color{38, 58, 66, 255});
    }

    panel(atlas, kPanelX, kPanelY, kPanelW, kPanelH);

    // Legend. Three ring colours already carry owned/affordable/locked; nothing
    // on screen said so.
    {
        struct Key { const char* label; Color c; };
        const Key keys[] = {{"OWNED", Color{214, 168, 62, 255}},
                            {"AFFORDABLE", Color{92, 152, 78, 255}},
                            {"LOCKED", Color{130, 118, 104, 255}}};
        int lx = kPanelX + 26;
        for (const auto& k : keys) {
            DrawCircle(lx + 6, kPanelY + 28, 7, k.c);
            DrawCircleLines(lx + 6, kPanelY + 28, 7, kInk);
            DrawText(k.label, lx + 20, kPanelY + 23, 10, kInkDim);
            lx += 30 + MeasureText(k.label, 10) + 26;
        }
    }

    const std::string id = tabs[static_cast<size_t>(tab)].treeId;
    if (reg.hasTree(id)) {
        const auto& tree = reg.tree(id);
        const auto L = treeLayout(tree);

        // Prerequisite links first, so nodes sit on top of their wiring. A lit
        // link means the prerequisite is owned, so the eye can trace how far a
        // path has actually been walked.
        for (const auto& n : tree.nodes) {
            const auto to = nodePos(L, n);
            for (const auto& req : n.prereqs) {
                const auto* pr = tree.find(req);
                if (!pr) continue;
                const auto from = nodePos(L, *pr);
                const bool lit = slot.meta.ownedNodes.count(req) > 0;
                const Color tint = branchTint(n.branch);
                DrawLineEx({from.x, from.y}, {to.x, to.y}, lit ? 5.0f : 3.0f,
                           lit ? tint : paint::mix(tint, Color{188, 170, 146, 255}, 0.62f));
            }
        }

        for (const auto& n : tree.nodes) {
            const auto p = nodePos(L, n);
            const bool owned = slot.meta.ownedNodes.count(n.id) > 0;
            const bool reachable = core::prereqsMet(tree, slot.meta, n.id);
            const bool affordable = reachable && !owned && slot.meta.shards >= n.cost;
            const bool hot = hit.kind == HubAction::Kind::Buy && hit.nodeId == n.id;
            const Color tint = branchTint(n.branch);

            const Color fill = owned       ? tint
                               : reachable ? Color{236, 224, 200, 255}
                                           : Color{198, 184, 162, 255};
            const Color ring = owned        ? Color{120, 82, 30, 255}
                               : affordable ? Color{92, 152, 78, 255}
                               : reachable  ? paint::mix(tint, Color{110, 98, 86, 255}, 0.45f)
                                            : paint::mix(tint, Color{150, 138, 122, 255}, 0.62f);

            DrawCircle(static_cast<int>(p.x), static_cast<int>(p.y) + 3, L.radius,
                       Color{92, 74, 56, 90});
            DrawCircle(static_cast<int>(p.x), static_cast<int>(p.y), L.radius, fill);
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), L.radius, ring);
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), L.radius - 1, ring);
            if (hot) {
                DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), L.radius + 3,
                                Color{255, 236, 170, 255});
            }

            // Every node carries an icon now, trunk included.
            const std::string art = nodeIcon(atlas, tree, n);
            if (!art.empty()) {
                atlas.drawFitted(art, p.x, p.y - 2, L.radius * 1.45f,
                                 owned ? WHITE : Color{206, 196, 180, 245}, /*grow=*/true);
            } else if (owned) {
                centredIn("*", static_cast<int>(p.x), static_cast<int>(p.y) - 10, 20,
                          Color{255, 244, 210, 255});
            }

            // The node's own name, always visible. This is authored in content
            // for all 23 nodes and used to appear only on hover, which meant
            // the tree read as anonymous numbered circles.
            centredIn(n.name.c_str(), static_cast<int>(p.x),
                      static_cast<int>(p.y + L.radius) + 6, 10, owned ? kInk : kInkDim);

            if (!owned) {
                const char* c = TextFormat("%d", n.cost);
                centredIn(c, static_cast<int>(p.x), static_cast<int>(p.y + L.radius) + 18, 10,
                          affordable ? Color{62, 122, 52, 255} : kInkWarn);
            }
        }

        // Hover detail. The name is on the board now, so this carries what the
        // node actually does.
        if (hit.kind == HubAction::Kind::Buy) {
            if (const auto* n = tree.find(hit.nodeId)) {
                const int w = std::max({MeasureText(n->name.c_str(), 20),
                                        MeasureText(n->desc.c_str(), 10), 220}) + 36;
                const int h = 78;
                const auto np = nodePos(L, *n);
                const int x = std::clamp(static_cast<int>(np.x) - w / 2, kPanelX + 8,
                                         kVirtualW - w - kPanelX - 8);
                // Flip above the node when there is no room below it.
                const bool below = np.y + L.radius + 34 + h < kPanelY + kPanelH - 8;
                const int y = below ? static_cast<int>(np.y + L.radius) + 34
                                    : static_cast<int>(np.y - L.radius) - 20 - h;
                panel(atlas, x, y, w, h);
                DrawText(n->name.c_str(), x + 18, y + 12, 20, kInk);
                DrawText(n->desc.c_str(), x + 18, y + 38, 10, kInkDim);
                DrawText(TextFormat("%d shards", n->cost), x + 18, y + 56, 10,
                         slot.meta.shards >= n->cost ? Color{62, 122, 52, 255} : kInkWarn);
                if (n->branch != "trunk") {
                    const char* b = n->branch.c_str();
                    DrawText(b, x + w - 18 - MeasureText(b, 10), y + 56, 10,
                             branchTint(n->branch));
                }
            }
        }
    }

    if (slot.hasRunInProgress()) {
        const bool hot = hit.kind == HubAction::Kind::ContinueRun;
        button(atlas, kBtnContinueX, kBtnY, kBtnW, kBtnH, hot, /*on=*/false);
        centredIn(TextFormat("CONTINUE  wave %d", slot.run->waveIndex + 1),
                  kBtnContinueX + kBtnW / 2, kBtnY + 17, 10, kInk);
    }
    {
        const bool hot = hit.kind == HubAction::Kind::NewRun;
        // Red only where the action destroys progress.
        button(atlas, kBtnNewX, kBtnY, kBtnW, kBtnH, hot, /*on=*/slot.hasRunInProgress());
        centredIn(slot.hasRunInProgress() ? "ABANDON & START NEW" : "START RUN",
                  kBtnNewX + kBtnW / 2, kBtnY + 17, 10, kInk);
    }
    {
        const bool hot = hit.kind == HubAction::Kind::BackToSlots;
        button(atlas, kBtnBackX, kBtnY, kBtnBackW, kBtnH, hot, /*on=*/false);
        centredIn("PROFILES", kBtnBackX + kBtnBackW / 2, kBtnY + 17, 10, kInk);
    }

    if (!message.empty()) {
        DrawText(message.c_str(), 24, kBtnY - 20, 10, Color{224, 108, 92, 255});
    }
}

// ==========================================================================
// Map select
// ==========================================================================

namespace {
// Five cards across the board area.
constexpr int kMapCardW = 250, kMapCardH = 300, kMapGap = 20;
constexpr int kMapTop = 210;
int mapCardX(int i) {
    const int total = 5 * kMapCardW + 4 * kMapGap;
    return (kVirtualW - total) / 2 + i * (kMapCardW + kMapGap);
}
constexpr int kMapBackW = 200, kMapBackH = 44;
}  // namespace

std::vector<std::string> mapOrder(const content::Registry& reg) {
    std::vector<std::pair<int, std::string>> byOrder;
    for (const auto& [id, m] : reg.maps()) byOrder.emplace_back(m.order, id);
    std::sort(byOrder.begin(), byOrder.end());
    std::vector<std::string> out;
    out.reserve(byOrder.size());
    for (const auto& [o, id] : byOrder) out.push_back(id);
    return out;
}

MapAction mapHitTest(const content::Registry& reg, const core::SaveSlot& slot, core::Vec2 m) {
    if (inRect(m, 24, kVirtualH - 60, kMapBackW, kMapBackH)) return {MapAction::Kind::Back};
    const auto order = mapOrder(reg);
    for (int i = 0; i < static_cast<int>(order.size()) && i < 5; ++i) {
        if (!core::mapUnlocked(slot.meta, order, i)) continue;  // locked cards are inert
        if (inRect(m, mapCardX(i), kMapTop, kMapCardW, kMapCardH)) {
            return {MapAction::Kind::Play, order[static_cast<size_t>(i)]};
        }
    }
    return {};
}

void drawMaps(const render::SpriteAtlas& atlas, const content::Registry& reg,
              const core::SaveSlot& slot, core::Vec2 mouse) {
    DrawRectangle(0, 0, kVirtualW, kVirtualH, palette::kBackdrop);
    ribbon(atlas, "ui_ribbon", kVirtualW / 2 - 200, 60, 400);
    centredIn("CHOOSE A MAP", kVirtualW / 2, 68, 20, kInk);
    centred("clear a map to unlock the next", 116, 10, palette::kHudDim);

    const auto hit = mapHitTest(reg, slot, mouse);
    const auto order = mapOrder(reg);

    for (int i = 0; i < static_cast<int>(order.size()) && i < 5; ++i) {
        const auto& id = order[static_cast<size_t>(i)];
        const auto& def = reg.map(id);
        const bool open = core::mapUnlocked(slot.meta, order, i);
        const bool hot = hit.kind == MapAction::Kind::Play && hit.mapId == id;
        const int x = mapCardX(i);

        panel(atlas, x, kMapTop, kMapCardW, kMapCardH);
        if (hot) {
            DrawRectangleLines(x + 4, kMapTop + 4, kMapCardW - 8, kMapCardH - 8,
                               Color{255, 236, 170, 210});
        }

        const auto it = slot.meta.mapProgress.find(id);
        const bool cleared = it != slot.meta.mapProgress.end() && it->second.cleared;

        ribbon(atlas, cleared ? "ui_ribbon" : "ui_ribbon_blue", x + 18, kMapTop + 14,
               kMapCardW - 36);
        centredIn(TextFormat("%d", i + 1), x + kMapCardW / 2, kMapTop + 22, 20, kInk);

        centredIn(def.name.c_str(), x + kMapCardW / 2, kMapTop + 62, 20,
                  open ? kInk : kInkDim);

        if (!open) {
            centredIn("LOCKED", x + kMapCardW / 2, kMapTop + 130, 40, kInkDim);
            centredIn("clear the map before it", x + kMapCardW / 2, kMapTop + 180, 10, kInkDim);
            continue;
        }

        // The blurb is the map's own, so what makes it different is readable
        // before committing a run to it.
        centredIn(def.blurb.c_str(), x + kMapCardW / 2, kMapTop + 96, 10, kInkDim);

        int ry = kMapTop + 140;
        const int best = it != slot.meta.mapProgress.end() ? it->second.bestWave : 0;
        DrawText("best wave", x + 24, ry, 10, kInkDim);
        DrawText(TextFormat("%d / %d", best, def.recipe.count),
                 x + kMapCardW - 24 - MeasureText(TextFormat("%d / %d", best, def.recipe.count), 10),
                 ry, 10, kInk);
        ry += 20;
        DrawText("waves", x + 24, ry, 10, kInkDim);
        DrawText(TextFormat("%d", def.recipe.count),
                 x + kMapCardW - 24 - MeasureText(TextFormat("%d", def.recipe.count), 10), ry, 10,
                 kInk);
        ry += 20;
        DrawText("bosses", x + 24, ry, 10, kInkDim);
        DrawText(TextFormat("%d", static_cast<int>(def.recipe.bosses.size())),
                 x + kMapCardW - 24 -
                     MeasureText(TextFormat("%d", static_cast<int>(def.recipe.bosses.size())), 10),
                 ry, 10, kInk);

        if (cleared) {
            centredIn("CLEARED", x + kMapCardW / 2, kMapTop + kMapCardH - 86, 20, kInkGood);
        }
        button(atlas, x + 30, kMapTop + kMapCardH - 58, kMapCardW - 60, 38, hot, false);
        centredIn("PLAY", x + kMapCardW / 2, kMapTop + kMapCardH - 44, 20, kInk);
    }

    const bool backHot = hit.kind == MapAction::Kind::Back;
    button(atlas, 24, kVirtualH - 60, kMapBackW, kMapBackH, backHot, false);
    centredIn("SKILL TREES", 24 + kMapBackW / 2, kVirtualH - 43, 10, kInk);
}

// ==========================================================================
// Pause overlay
// ==========================================================================

namespace {
constexpr int kPauseW = 420, kPauseH = 328;
int pauseX() { return (kVirtualW - kPauseW) / 2; }
int pauseY() { return (kPlayH - kPauseH) / 2; }
constexpr int kSliderH = 14;
int sliderX() { return pauseX() + 40; }
int sliderW() { return kPauseW - 80; }
int musicY() { return pauseY() + 110; }
int sfxY() { return pauseY() + 166; }
constexpr int kPauseBtnH = 40;
int resumeY() { return pauseY() + 210; }
int quitY() { return pauseY() + 254; }

// A slider's value comes from where in the track the cursor is.
float valueAt(core::Vec2 m) {
    return std::clamp((static_cast<float>(m.x) - static_cast<float>(sliderX())) /
                          static_cast<float>(sliderW()),
                      0.0f, 1.0f);
}
// Generous vertically: a 14px track is hard to hit precisely mid-game.
bool onSlider(core::Vec2 m, int y) {
    return m.x >= sliderX() - 8 && m.x <= sliderX() + sliderW() + 8 && m.y >= y - 10 &&
           m.y <= y + kSliderH + 10;
}
}  // namespace

PauseAction pauseHitTest(core::Vec2 m) {
    if (onSlider(m, musicY())) return {PauseAction::Kind::SetMusic, valueAt(m)};
    if (onSlider(m, sfxY())) return {PauseAction::Kind::SetSfx, valueAt(m)};
    if (inRect(m, pauseX() + 40, resumeY(), kPauseW - 80, kPauseBtnH)) {
        return {PauseAction::Kind::Resume};
    }
    if (inRect(m, pauseX() + 40, quitY(), kPauseW - 80, kPauseBtnH)) {
        return {PauseAction::Kind::Quit};
    }
    return {};
}

void drawPause(const render::SpriteAtlas& atlas, float musicVol, float sfxVol,
               core::Vec2 mouse) {
    DrawRectangle(0, 0, kVirtualW, kPlayH, Color{14, 12, 20, 170});
    const auto hit = pauseHitTest(mouse);

    panel(atlas, pauseX(), pauseY(), kPauseW, kPauseH);
    ribbon(atlas, "ui_ribbon", pauseX() + 60, pauseY() + 20, kPauseW - 120);
    centredIn("PAUSED", kVirtualW / 2, pauseY() + 28, 20, kInk);

    const auto slider = [&](const char* label, float v, int y, bool hot) {
        DrawText(label, sliderX(), y - 16, 10, kInkDim);
        const char* pctText = TextFormat("%d%%", static_cast<int>(v * 100.0f + 0.5f));
        DrawText(pctText, sliderX() + sliderW() - MeasureText(pctText, 10), y - 16, 10, kInk);
        DrawRectangle(sliderX(), y, sliderW(), kSliderH, Color{150, 128, 104, 220});
        DrawRectangle(sliderX(), y, static_cast<int>(sliderW() * v), kSliderH,
                      hot ? Color{132, 178, 108, 255} : Color{104, 150, 90, 255});
        DrawRectangleLines(sliderX(), y, sliderW(), kSliderH, kInk);
        // The knob, so it reads as draggable rather than as a progress bar.
        const int kx = sliderX() + static_cast<int>(sliderW() * v);
        DrawRectangle(kx - 3, y - 4, 7, kSliderH + 8, Color{242, 232, 210, 255});
        DrawRectangleLines(kx - 3, y - 4, 7, kSliderH + 8, kInk);
    };
    slider("MUSIC", musicVol, musicY(), hit.kind == PauseAction::Kind::SetMusic);
    slider("SOUND", sfxVol, sfxY(), hit.kind == PauseAction::Kind::SetSfx);

    button(atlas, pauseX() + 40, resumeY(), kPauseW - 80, kPauseBtnH,
           hit.kind == PauseAction::Kind::Resume, false);
    centredIn("RESUME", kVirtualW / 2, resumeY() + 14, 20, kInk);

    button(atlas, pauseX() + 40, quitY(), kPauseW - 80, kPauseBtnH,
           hit.kind == PauseAction::Kind::Quit, /*on=*/true);
    centredIn("QUIT TO SKILL TREES", kVirtualW / 2, quitY() + 15, 10, kInk);

    // Below the quit button, not on top of it: at kPauseH 300 this overlapped.
    centredIn("the run autosaves at every build phase", kVirtualW / 2,
              quitY() + kPauseBtnH + 12, 10, kInkDim);
}

// ==========================================================================
// Results
// ==========================================================================

void drawResults(const render::SpriteAtlas& atlas, const sim::World& w, int shardsAwarded,
                 int totalShards) {
    DrawRectangle(0, 0, kVirtualW, kVirtualH, Color{16, 14, 24, 210});
    const bool won = w.phase() == sim::Phase::Cleared;

    // The numbers used to sit as a loose centred column on the dimmed board.
    constexpr int kW = 660, kH = 420;
    const int px = (kVirtualW - kW) / 2;
    const int py = (kVirtualH - kH) / 2 - 20;
    panel(atlas, px, py, kW, kH);

    ribbon(atlas, won ? "ui_ribbon" : "ui_ribbon_red", px + 60, py + 26, kW - 120);
    centredIn(won ? "MAP CLEARED" : "DEFEATED", kVirtualW / 2, py + 34, 20, kInk);

    int y = py + 92;
    centredIn(TextFormat("wave %d of %d", w.waveIndex(), w.waveCount()), kVirtualW / 2, y, 40,
              kInk);
    y += 58;

    {
        auto specs = w.activeTowerSpecs();
        const auto elems = w.activeElementSpecs();
        specs.insert(specs.end(), elems.begin(), elems.end());
        centredIn("BUILD FIELDED", kVirtualW / 2, y, 10, kInkDim);
        y += 18;
        std::string build;
        for (const auto& sp : specs) build += (build.empty() ? "" : "   /   ") + sp;
        centredIn(build.empty() ? "none" : build.c_str(), kVirtualW / 2, y, 20, kInk);
        y += 44;
    }

    // The shard award is the reason this screen exists -- it is what turns a
    // lost run into permanent progress -- so it gets the emphasis.
    DrawRectangle(px + 60, y, kW - 120, 2, paint::mix(kInkDim, kInk, 0.2f));
    y += 22;
    {
        // Placed off the measured text, not a guessed offset -- a fixed offset
        // put the gem on top of the number.
        const char* award = TextFormat("+%d SHARDS", shardsAwarded);
        const int aw = MeasureText(award, 40);
        DrawText(award, kVirtualW / 2 - aw / 2 + 18, y, 40, kInkWarn);
        atlas.drawFitted("icon_gem", kVirtualW / 2.0f - aw / 2.0f - 16.0f,
                         static_cast<float>(y) + 18.0f, 28);
    }
    y += 52;
    centredIn(TextFormat("%d banked in total", totalShards), kVirtualW / 2, y, 10, kInkDim);

    centredIn("PRESS ENTER TO SPEND THEM", kVirtualW / 2, py + kH - 44, 20, kInk);
}

}  // namespace td::ui
