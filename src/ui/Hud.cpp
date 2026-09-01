#include "ui/Hud.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"

#include "render/Palette.h"
#include "render/PixelCanvas.h"
#include "ui/Paint.h"

namespace td::ui {
namespace {
using namespace td::render;
using namespace td::ui::paint;

constexpr float kMessageHold = 2.0f;

// Kingdom Rush keeps its controls in fixed corners rather than following the
// cursor, so the player builds muscle memory for them. Same idea here.
constexpr int kNextW = 176, kNextH = 52;
constexpr int kNextX = kVirtualW - kNextW - 14;
constexpr int kNextY = kHudY + 22;
constexpr int kBtn = 36;
constexpr int kPauseX = kNextX - kBtn - 14;
constexpr int kSpeedX = kPauseX - kBtn - 8;
constexpr int kMuteX = kSpeedX - kBtn - 8;
constexpr int kQuitX = kMuteX - kBtn - 8;
constexpr int kBtnY = kHudY + 30;

// Column starts. The band is one row of zones read left to right: purse, wave,
// what is coming, what is fielded, controls.
constexpr int kWaveX = 348;  // clear of a four-digit gold total at 40px
constexpr int kIncomingX = 560;
constexpr int kFieldedX = 900;

bool inRect(core::Vec2 m, int x, int y, int w, int h) {
    return m.x >= x && m.y >= y && m.x < x + w && m.y < y + h;
}

}  // namespace

HudButton hudHitTest(core::Vec2 m) {
    if (inRect(m, kNextX, kNextY, kNextW, kNextH)) return HudButton::NextWave;
    if (inRect(m, kSpeedX, kBtnY, kBtn, kBtn)) return HudButton::Speed;
    if (inRect(m, kPauseX, kBtnY, kBtn, kBtn)) return HudButton::Pause;
    if (inRect(m, kMuteX, kBtnY, kBtn, kBtn)) return HudButton::Mute;
    if (inRect(m, kQuitX, kBtnY, kBtn, kBtn)) return HudButton::Quit;
    return HudButton::None;
}

namespace {

// Notable means "worth a pip": anything that is not neutral.
std::vector<std::pair<std::string, float>> notableResists(const content::EnemyDef& def) {
    std::vector<std::pair<std::string, float>> out;
    for (const auto& [type, mult] : def.resist) {
        if (std::abs(mult - 1.0f) > 0.01f) out.emplace_back(type, mult);
    }
    // Vulnerabilities first: that is the actionable half.
    std::sort(out.begin(), out.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    return out;
}

}  // namespace

void drawEnemyDossier(const render::SpriteAtlas& atlas, const content::EnemyDef& def,
                      core::Vec2 atMouse) {
    const auto resists = notableResists(def);
    const int rows = 3 + static_cast<int>(resists.size());
    int pw = 236;
    pw = std::max(pw, MeasureText(def.name.c_str(), 20) + 40);
    const int ph = 40 + rows * 15 + 10;

    int px = static_cast<int>(atMouse.x) + 22;
    if (px + pw > kVirtualW - 6) px = static_cast<int>(atMouse.x) - 22 - pw;
    px = std::clamp(px, 6, kVirtualW - pw - 6);
    const int py = std::clamp(static_cast<int>(atMouse.y) - ph / 2, 6, kPlayH - ph - 6);

    panel(atlas, px, py, pw, ph);
    DrawText(def.name.c_str(), px + 14, py + 10, 20, kInk);
    if (def.boss) {
        const char* tag = "BOSS";
        DrawText(tag, px + pw - 14 - MeasureText(tag, 10), py + 16, 10, kInkWarn);
    }

    int ry = py + 36;
    const auto plain = [&](const char* label, const char* value) {
        DrawText(label, px + 14, ry, 10, kInkDim);
        DrawText(value, px + pw - 14 - MeasureText(value, 10), ry, 10, kInk);
        ry += 15;
    };
    plain("health", TextFormat("%.0f", def.maxHp));
    plain("armour", TextFormat("%.0f", def.armor));
    plain("speed", TextFormat("%.1f", def.speed));

    if (resists.empty()) {
        DrawText("no resistances", px + 14, ry, 10, kInkDim);
        return;
    }
    for (const auto& [type, mult] : resists) {
        const bool weak = mult > 1.0f;
        const Color hue = render::palette::damageTypeColor(type);
        // Type hue on the left as a swatch, so the pips in the HUD and this row
        // are recognisably the same thing.
        DrawRectangle(px + 14, ry + 1, 8, 8, hue);
        DrawRectangleLines(px + 14, ry + 1, 8, 8, kInk);
        DrawText(type.c_str(), px + 28, ry, 10, kInk);
        const char* v = TextFormat("x%.2f  %s", mult, weak ? "WEAK" : "resists");
        DrawText(v, px + pw - 14 - MeasureText(v, 10), ry, 10,
                 weak ? render::palette::kVulnerable : render::palette::kResistant);
        ry += 15;
    }
}

void drawBossBars(const render::SpriteAtlas& atlas, const sim::World& w) {
    struct Live {
        std::string name;
        float frac;
    };
    std::vector<Live> live;
    auto& r = const_cast<entt::registry&>(w.reg());
    // Boss is an empty tag type, which EnTT omits from the each() parameters.
    r.view<const sim::Boss, const sim::Health, const sim::EnemyTag>().each(
        [&](const sim::Health& hp, const sim::EnemyTag& tag) {
            if (hp.hp <= 0.0f || hp.maxHp <= 0.0f) return;
            live.push_back({w.defs().enemy(tag.defId).name,
                            std::clamp(hp.hp / hp.maxHp, 0.0f, 1.0f)});
        });
    if (live.empty()) return;

    constexpr int kW = 620, kH = 46;
    const int x = (kVirtualW - kW) / 2;
    int y = 14;
    for (const auto& b : live) {
        panel(atlas, x, y, kW, kH);

        const int barX = x + 18, barY = y + 24, barW = kW - 36, barH = 12;
        DrawRectangle(barX, barY, barW, barH, Color{58, 40, 38, 230});
        DrawRectangle(barX, barY, static_cast<int>(barW * b.frac), barH,
                      b.frac > 0.5f   ? Color{188, 58, 48, 255}
                      : b.frac > 0.2f ? Color{206, 120, 44, 255}
                                      : Color{224, 186, 60, 255});
        DrawRectangleLines(barX, barY, barW, barH, kInk);

        centredIn(b.name.c_str(), kVirtualW / 2, y + 6, 20, kInk);
        const char* pctText = TextFormat("%d%%", static_cast<int>(b.frac * 100.0f));
        DrawText(pctText, barX + barW - MeasureText(pctText, 10) - 4, barY + 1, 10,
                 Color{255, 240, 220, 255});
        y += kH + 6;
    }
}

void drawHud(const render::SpriteAtlas& atlas, const sim::World& w, const HudState& st,
             core::Vec2 mouse) {
    if (available(atlas)) {
        atlas.drawNine("ui_panel", 0.0f, static_cast<float>(kHudY) - 6.0f,
                       static_cast<float>(kVirtualW), static_cast<float>(kHudH) + 6.0f);
    } else {
        DrawRectangle(0, kHudY, kVirtualW, kHudH, palette::kHudBg);
        DrawRectangle(0, kHudY, kVirtualW, 2, Color{70, 64, 92, 255});
    }
    const bool flat = !available(atlas);
    const Color ink = flat ? palette::kHudText : kInk;
    const Color inkDim = flat ? palette::kHudDim : kInkDim;

    // Lives and gold: the two values read under pressure, so they get the
    // largest numerals on screen rather than the same 20px as everything else.
    atlas.drawFitted("icon_heart", 36, kHudY + 46, 26);
    DrawText(TextFormat("%d", w.lives()), 58, kHudY + 26, 40,
             w.lives() <= 5 ? Color{176, 54, 44, 255} : ink);
    atlas.drawFitted("icon_coin", 178, kHudY + 46, 26);
    DrawText(TextFormat("%d", w.gold()), 200, kHudY + 26, 40, ink);

    DrawText(TextFormat("WAVE %d/%d", w.waveIndex() + 1, w.waveCount()), kWaveX, kHudY + 22, 20,
             ink);
    DrawText(TextFormat("%d on the field", w.aliveEnemies()), kWaveX, kHudY + 52, 10, inkDim);

    // What is coming. Kingdom Rush always tells you what you are about to face,
    // which is what makes committing gold before a wave a real decision.
    struct IconRect {
        int x, y, w, h;
    };
    std::vector<std::pair<IconRect, std::string>> incomingRects;

    const int wi = w.waveIndex();
    if (wi >= 0 && wi < static_cast<int>(w.map().waves.size())) {
        DrawText("INCOMING", kIncomingX, kHudY + 14, 10, inkDim);
        DrawText("hover for weaknesses", kIncomingX + 62, kHudY + 14, 10,
                 flat ? palette::kHudDim : paint::mix(kInkDim, kInk, 0.1f));
        int ix = kIncomingX + 8;
        for (const auto& g : w.map().waves[static_cast<size_t>(wi)].groups) {
            // Same sprite indirection as the board, so a map's tinted roster
            // shows correctly in the incoming preview too.
            const auto& ed = w.defs().enemy(g.enemyId);
            atlas.drawFitted(ed.spriteId() + "_0", static_cast<float>(ix), kHudY + 48.0f, 26.0f,
                             Color{static_cast<unsigned char>(ed.tintR),
                                   static_cast<unsigned char>(ed.tintG),
                                   static_cast<unsigned char>(ed.tintB), 255});
            DrawText(TextFormat("x%d", g.count), ix + 11, kHudY + 30, 10, ink);

            // A pip per notable resistance: type hue, with a bar above for
            // vulnerable and below for resistant, so direction survives without
            // relying on hue alone.
            int qx = ix - 18;
            for (const auto& [type, mult] : notableResists(ed)) {
                const bool weak = mult > 1.0f;
                const Color hue = render::palette::damageTypeColor(type);
                DrawRectangle(qx, kHudY + 64, 7, 7, hue);
                DrawRectangle(qx, weak ? kHudY + 61 : kHudY + 72, 7, 2,
                              weak ? render::palette::kVulnerable
                                   : render::palette::kResistant);
                qx += 9;
                if (qx > ix + 20) break;  // never spill into the next enemy
            }
            incomingRects.push_back({{ix - 20, kHudY + 26, 44, 52}, g.enemyId});
            ix += 52;
        }
    }

    // Hovering an incoming enemy opens its dossier. The pips say "something is
    // here"; this says exactly what.
    for (const auto& [rect, enemyId] : incomingRects) {
        if (!inRect(mouse, rect.x, rect.y, rect.w, rect.h)) continue;
        drawEnemyDossier(atlas, w.defs().enemy(enemyId), mouse);
        break;
    }

    // Which specialisations are fielded right now. One of each may exist, so
    // this doubles as a reminder of what is still available to build.
    DrawText("FIELDED", kFieldedX, kHudY + 14, 10, inkDim);
    {
        auto specs = w.activeTowerSpecs();
        const auto elems = w.activeElementSpecs();
        specs.insert(specs.end(), elems.begin(), elems.end());
        if (specs.empty()) {
            DrawText("none yet", kFieldedX, kHudY + 34, 10, inkDim);
        } else {
            // Four specs at 20px wrapped to three rows and spilled out of the
            // band, so these stay small: the HUD already shows each spec's
            // tower on the board.
            int x = kFieldedX, y = kHudY + 34;
            for (const auto& sp : specs) {
                const int wpx = MeasureText(sp.c_str(), 10);
                if (x + wpx > kQuitX - 16) {
                    x = kFieldedX;
                    y += 18;
                }
                if (y > kHudY + 70) break;  // never draw past the band
                DrawText(sp.c_str(), x, y, 10, flat ? palette::kHudWarn : kInkWarn);
                x += wpx + 14;
            }
        }
    }

    if (!st.message.empty() && st.messageAge < kMessageHold) {
        DrawText(st.message.c_str(), kWaveX, kHudY + 72, 10, Color{188, 62, 44, 255});
    }

    const HudButton hot = hudHitTest(mouse);

    // The pack ships 1/2/3 glyphs, which say the speed outright instead of a
    // play/fast-forward icon plus a "2x" caption in the corner.
    button(atlas, kSpeedX, kBtnY, kBtn, kBtn, hot == HudButton::Speed, false);
    {
        const std::string dial = TextFormat("ui_icon_%02d", 4 + std::clamp(st.speedIndex, 0, 2));
        if (atlas.has(dial)) {
            atlas.drawFitted(dial, kSpeedX + kBtn * 0.5f, kBtnY + kBtn * 0.5f, kBtn * 0.52f,
                             ink);
        } else {
            atlas.drawFitted(st.speedIndex == 0 ? "icon_play" : "icon_ffwd",
                             kSpeedX + kBtn * 0.5f, kBtnY + kBtn * 0.5f, kBtn * 0.62f);
            if (st.speedIndex > 0) {
                DrawText(TextFormat("%dx", st.speedIndex + 1), kSpeedX + 3, kBtnY + kBtn - 12,
                         10, flat ? palette::kHudWarn : kInkWarn);
            }
        }
    }
    button(atlas, kPauseX, kBtnY, kBtn, kBtn, hot == HudButton::Pause, st.paused);
    atlas.drawFitted("icon_pause", kPauseX + kBtn * 0.5f, kBtnY + kBtn * 0.5f, kBtn * 0.62f);

    // Mute, and a way out of a run that is not "die". The run autosaves at every
    // build phase, so leaving loses nothing.
    button(atlas, kMuteX, kBtnY, kBtn, kBtn, hot == HudButton::Mute, false, st.muted);
    {
        // A real speaker glyph, and a close cross when muted, rather than the
        // ASCII "))" and "x" this used to draw.
        const char* art = st.muted ? "ui_icon_01" : "ui_icon_03";
        if (atlas.has(art)) {
            atlas.drawFitted(art, kMuteX + kBtn * 0.5f, kBtnY + kBtn * 0.5f, kBtn * 0.52f,
                             st.muted ? Color{150, 70, 60, 255} : ink);
        } else {
            centredIn(st.muted ? "x" : "))", kMuteX + kBtn / 2, kBtnY + 13, 10,
                      st.muted ? Color{150, 70, 60, 255} : ink);
        }
    }
    button(atlas, kQuitX, kBtnY, kBtn, kBtn, hot == HudButton::Quit, false);
    atlas.drawFitted("icon_back", kQuitX + kBtn * 0.5f, kBtnY + kBtn * 0.5f, kBtn * 0.62f);

    // Call-the-wave button, the way Kingdom Rush pays you for impatience.
    const bool building = w.phase() == sim::Phase::Build;
    button(atlas, kNextX, kNextY, kNextW, kNextH, building && hot == HudButton::NextWave,
           /*on=*/building, /*off=*/!building);
    if (building) {
        centredIn("NEXT WAVE", kNextX + kNextW / 2, kNextY + 8, 20, ink);
        centredIn(TextFormat("%.0fs   +%d gold", w.buildTimeRemaining(), w.earlyStartBonus()),
                  kNextX + kNextW / 2, kNextY + 32, 10,
                  flat ? palette::kHpFill : kInkGood);
    } else {
        centredIn("WAVE IN PROGRESS", kNextX + kNextW / 2, kNextY + 20, 10, inkDim);
    }

    if (st.paused) {
        const char* p = "PAUSED";
        DrawText(p, (kVirtualW - MeasureText(p, 40)) / 2, 200, 40, palette::kHudText);
    }
}

}  // namespace td::ui
