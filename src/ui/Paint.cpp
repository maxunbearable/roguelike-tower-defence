#include "ui/Paint.h"

namespace td::ui::paint {

bool available(const render::SpriteAtlas& a) { return a.has("ui_panel"); }

Color mix(Color a, Color b, float t) {
    const auto k = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(x + (y - x) * t);
    };
    return {k(a.r, b.r), k(a.g, b.g), k(a.b, b.b), k(a.a, b.a)};
}

void frame(int x, int y, int w, int h, bool hot, bool bright) {
    DrawRectangle(x, y, w, h, hot ? Color{48, 42, 66, 255} : Color{28, 24, 40, 245});
    DrawRectangleLines(x, y, w, h,
                       bright ? Color{255, 226, 140, 255}
                              : (hot ? Color{190, 180, 220, 255} : Color{92, 84, 118, 255}));
}

void panel(const render::SpriteAtlas& a, int x, int y, int w, int h) {
    if (available(a)) {
        a.drawNine("ui_panel", static_cast<float>(x), static_cast<float>(y),
                   static_cast<float>(w), static_cast<float>(h));
    } else {
        frame(x, y, w, h, false);
    }
}

void button(const render::SpriteAtlas& a, int x, int y, int w, int h, bool hot, bool on,
            bool off) {
    if (!available(a)) {
        frame(x, y, w, h, hot, on);
        return;
    }
    const char* art = off ? "ui_btn_off" : (hot ? "ui_btn_hover" : (on ? "ui_btn_red" : "ui_btn"));
    a.drawNine(art, static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
               static_cast<float>(h));
}

int ribbonH(const render::SpriteAtlas& a, const char* art) {
    const auto* t = a.get(art);
    return t ? t->height : 32;
}

void ribbon(const render::SpriteAtlas& a, const char* art, int x, int y, int w) {
    if (available(a) && a.has(art)) {
        a.drawNine(art, static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                   static_cast<float>(ribbonH(a, art)));
    } else {
        frame(x, y, w, 32, false, true);
    }
}

void centredIn(const char* t, int cx, int y, int size, Color c) {
    DrawText(t, cx - MeasureText(t, size) / 2, y, size, c);
}

}  // namespace td::ui::paint
