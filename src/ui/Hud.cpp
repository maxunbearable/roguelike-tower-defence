#include "ui/Hud.h"

#include "core/Difficulty.h"

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

// Long enough to read a teaching line, not so long it lingers over play.
constexpr float kMessageHold = 4.5f;

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
constexpr int kFieldedX = 940;
// The abilities sit in the band, not in the control cluster: they are things you
// DO, like building, rather than settings like mute and speed.
constexpr int kAbilW = 46, kAbilH = 46;
constexpr int kAbilX = 772, kAbilY = kHudY + 26;
int abilX(int i) { return kAbilX + i * (kAbilW + 12); }

// A hairline between zones. Research on HUD hierarchy is consistent that the
// fastest win is making groups LOOK like groups: without these the band reads as
// one row of floating text at four different x positions.
void zoneRule(int x) {
    DrawRectangle(x, kHudY + 14, 1, 62, Color{120, 100, 78, 90});
    DrawRectangle(x + 1, kHudY + 14, 1, 62, Color{250, 238, 214, 40});
}

// Every zone gets the same small caption in the same place, so the eye can scan
// the band by shape instead of reading it.
void caption(const char* text, int x, Color c) { DrawText(text, x, kHudY + 14, 10, c); }

bool inRect(core::Vec2 m, int x, int y, int w, int h) {
    return m.x >= x && m.y >= y && m.x < x + w && m.y < y + h;
}

}  // namespace

HudButton hudHitTest(core::Vec2 m) {
    if (inRect(m, kNextX, kNextY, kNextW, kNextH)) return HudButton::NextWave;
    if (inRect(m, kSpeedX, kBtnY, kBtn, kBtn)) return HudButton::Speed;
    if (inRect(m, abilX(0), kAbilY, kAbilW, kAbilH)) return HudButton::Strike;
    if (inRect(m, abilX(1), kAbilY, kAbilW, kAbilH)) return HudButton::Ward;
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
    caption("LIVES", 34, inkDim);
    atlas.drawFitted("icon_heart", 36, kHudY + 50, 24);
    DrawText(TextFormat("%d", w.lives()), 56, kHudY + 30, 40,
             w.lives() <= 5 ? Color{206, 62, 50, 255} : ink);
    caption("GOLD", 176, inkDim);
    atlas.drawFitted("icon_coin", 178, kHudY + 50, 24);
    DrawText(TextFormat("%d", w.gold()), 198, kHudY + 30, 40, ink);
    zoneRule(kWaveX - 26);

    caption("WAVE", kWaveX, inkDim);
    // Which difficulty this run is on. Without it the player has no way to tell
    // a hard wave from a hard SETTING, and the number they are proud of has no
    // context.
    {
        const auto d = w.difficulty();
        const char* dn = core::difficultyName(d);
        DrawText(dn, kWaveX + 52, kHudY + 14, 10,
                 d == core::Difficulty::Brutal    ? Color{206, 108, 84, 255}
                 : d == core::Difficulty::Relaxed ? Color{120, 158, 104, 255}
                                                  : inkDim);
    }
    DrawText(TextFormat("%d/%d", w.waveIndex() + 1, w.waveCount()), kWaveX, kHudY + 28, 32, ink);
    // Tower count belongs here: only one of each specialisation may be fielded,
    // so "how much board do I have" is a decision input, not trivia.
    DrawText(TextFormat("%d on the field   %d towers", w.aliveEnemies(), w.towerCount()),
             kWaveX, kHudY + 64, 10, inkDim);
    zoneRule(kIncomingX - 26);

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
    // --- abilities ---------------------------------------------------------
    zoneRule(kAbilX - 24);
    caption("ABILITIES", kAbilX, inkDim);
    {
        // hudHitTest is pure and cheap; the shared `hot` is computed further
        // down with the control cluster.
        const HudButton over = hudHitTest(mouse);
        struct Abil { sim::Ability id; const char* icon; const char* key; HudButton btn; };
        const Abil abils[] = {
            {sim::Ability::Strike, "icon_quake", "Q", HudButton::Strike},
            {sim::Ability::Ward, "icon_gem", "W", HudButton::Ward},
        };
        for (int i = 0; i < 2; ++i) {
            const auto& ab = abils[i];
            const float cd = w.abilityCooldown(ab.id);
            const bool ready = w.abilityReady(ab.id);
            const bool armed = st.armed == static_cast<int>(ab.id);
            const int bx = abilX(i);
            button(atlas, bx, kAbilY, kAbilW, kAbilH, over == ab.btn, armed, !ready);
            atlas.drawFitted(ab.icon, bx + kAbilW * 0.5f, kAbilY + kAbilH * 0.45f, kAbilW * 0.5f,
                             ready ? WHITE : Color{150, 142, 130, 220});
            if (!ready) {
                // A top-down sweep, so "how much longer" is readable without
                // reading the number.
                const float frac = cd / sim::World::abilityCooldownMax(ab.id);
                const int h = static_cast<int>(kAbilH * frac);
                DrawRectangle(bx, kAbilY + kAbilH - h, kAbilW, h, Color{18, 16, 26, 150});
                const char* secs = TextFormat("%d", static_cast<int>(cd + 0.99f));
                DrawText(secs, bx + (kAbilW - MeasureText(secs, 20)) / 2, kAbilY + 12, 20,
                         Color{246, 232, 200, 235});
            }
            DrawText(ab.key, bx + kAbilW - 9, kAbilY + kAbilH - 13, 10,
                     armed ? Color{255, 236, 170, 255} : Color{92, 74, 56, 220});
        }
    }

    zoneRule(kFieldedX - 22);
    caption("FIELDED", kFieldedX, inkDim);
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
                // A chip rather than loose text: at 10px these were five grey
                // words in a row. The fill is LIGHT with dark ink on it -- a dark
                // fill with the warm ink already used here rendered as a solid
                // block with the label invisible inside it.
                DrawRectangle(x - 4, y - 3, wpx + 8, 15, Color{226, 206, 172, 235});
                DrawRectangleLines(x - 4, y - 3, wpx + 8, 15, Color{132, 106, 76, 210});
                DrawText(sp.c_str(), x, y, 10, Color{72, 52, 36, 255});
                x += wpx + 16;
            }
        }
    }

    if (!st.message.empty() && st.messageAge < kMessageHold) {
        // Centred just above the band rather than tucked under the wave counter
        // at 10px, where the tutorial lines were effectively invisible. Fades out
        // over the last second so it never just blinks off.
        const float left = kMessageHold - st.messageAge;
        const float k = left < 1.0f ? left : 1.0f;
        const auto a = static_cast<unsigned char>(235.0f * k);
        const int tw = MeasureText(st.message.c_str(), 20);
        const int bx = (kVirtualW - tw) / 2 - 16, by = kHudY - 42;
        DrawRectangle(bx, by, tw + 32, 30, Color{24, 20, 30, static_cast<unsigned char>(190 * k)});
        DrawRectangle(bx, by + 28, tw + 32, 2, Color{224, 180, 96, a});
        DrawText(st.message.c_str(), bx + 16, by + 6, 20, Color{242, 230, 208, a});
    }

    const HudButton hot = hudHitTest(mouse);

    // The pack's dial glyphs read 1/2/3, so they cannot label 4x or 8x -- they
    // would state the wrong speed. Icon plus an explicit caption instead, which
    // is the one thing that stays true whatever the multipliers become.
    button(atlas, kSpeedX, kBtnY, kBtn, kBtn, hot == HudButton::Speed, st.speedIndex > 0);
    {
        static constexpr int kMult[4] = {1, 2, 4, 8};
        const int mult = kMult[std::clamp(st.speedIndex, 0, 3)];
        atlas.drawFitted(st.speedIndex == 0 ? "icon_play" : "icon_ffwd",
                         kSpeedX + kBtn * 0.5f, kBtnY + kBtn * 0.42f, kBtn * 0.5f);
        const char* cap = TextFormat("%dx", mult);
        DrawText(cap, kSpeedX + (kBtn - MeasureText(cap, 10)) / 2, kBtnY + kBtn - 13, 10,
                 st.speedIndex > 0 ? (flat ? palette::kHudWarn : kInkWarn)
                                   : (flat ? palette::kHudDim : kInkDim));
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

    // Call-the-wave button, the way Kingdom Rush pays you for impatience. It is
    // live during a running wave as well: that call stacks the next wave on top
    // of this one and pays for the risk rather than for skipped build time.
    const bool building = w.phase() == sim::Phase::Build;
    const bool callable = w.canCallWave();
    button(atlas, kNextX, kNextY, kNextW, kNextH, callable && hot == HudButton::NextWave,
           /*on=*/callable, /*off=*/!callable);
    if (building) {
        centredIn("NEXT WAVE", kNextX + kNextW / 2, kNextY + 8, 20, ink);
        centredIn(TextFormat("%.0fs   +%d gold", w.buildTimeRemaining(), w.earlyStartBonus()),
                  kNextX + kNextW / 2, kNextY + 32, 10,
                  flat ? palette::kHpFill : kInkGood);
    } else if (callable) {
        centredIn("CALL EARLY", kNextX + kNextW / 2, kNextY + 8, 20, ink);
        centredIn(TextFormat("stack next wave   +%d gold", w.overlapCallBonus()),
                  kNextX + kNextW / 2, kNextY + 32, 10,
                  flat ? palette::kHpFill : kInkGood);
    } else {
        centredIn("FINAL WAVE", kNextX + kNextW / 2, kNextY + 20, 10, inkDim);
    }

}

TutorialBox drawTutorial(const render::SpriteAtlas& atlas, const char* title, const char* body,
                         core::Vec2 mouse) {
    const int bw = std::max(MeasureText(body, 10) + 190, 520);
    const int bh = 62;
    // Bottom of the play field, not the top: on these maps the route runs along
    // the top row, and a panel there covers the enemies the tutorial is telling
    // the player to look at.
    const int bx = (kVirtualW - bw) / 2, by = kPlayH - bh - 12;
    TutorialBox box{bx, by, bw, bh, bx + bw - 84, by + 16, 68, 30};

    panel(atlas, bx, by, bw, bh);
    DrawText(title, bx + 20, by + 12, 20, kInk);
    DrawText(body, bx + 20, by + 38, 10, kInkDim);

    const bool hot = mouse.x >= box.skipX && mouse.x < box.skipX + box.skipW &&
                     mouse.y >= box.skipY && mouse.y < box.skipY + box.skipH;
    button(atlas, box.skipX, box.skipY, box.skipW, box.skipH, hot, false);
    centredIn("SKIP", box.skipX + box.skipW / 2, box.skipY + 9, 10, kInkDim);
    return box;
}

void drawPausedBanner() {
    constexpr int kH = 30;
    DrawRectangle(0, 0, kVirtualW, kH, Color{18, 16, 26, 205});
    DrawRectangle(0, kH, kVirtualW, 2, Color{224, 180, 96, 220});
    const char* a = "PAUSED";
    const char* b = "build, upgrade and re-target freely   -   P to resume";
    const int aw = MeasureText(a, 20), bw = MeasureText(b, 10);
    const int total = aw + 18 + bw;
    const int x = (kVirtualW - total) / 2;
    DrawText(a, x, 5, 20, Color{240, 214, 150, 255});
    DrawText(b, x + aw + 18, 11, 10, Color{198, 190, 178, 255});
}

}  // namespace td::ui
