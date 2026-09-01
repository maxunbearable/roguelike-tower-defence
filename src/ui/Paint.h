#pragma once

#include <string>

#include "raylib.h"

#include "render/SpriteAtlas.h"

// Painted UI primitives shared by every screen: the hub, the slot select, the
// results panel, the HUD band and the radial menu all draw the same imported
// nine-slice panels, buttons and ribbons.
//
// These live together because the fallback behaviour has to be consistent.
// assets/sprites is gitignored -- the pack licence forbids redistribution -- so
// a fresh checkout has no UI art, and every helper degrades to the flat
// prototype look rather than drawing nothing.
namespace td::ui::paint {

// The panels are light parchment and the buttons pale blue, so text on them
// needs dark ink rather than the HUD's light-on-dark palette.
inline constexpr Color kInk{54, 42, 36, 255};
inline constexpr Color kInkDim{116, 96, 78, 255};
inline constexpr Color kInkWarn{140, 62, 40, 255};
inline constexpr Color kInkGood{62, 122, 52, 255};

bool available(const render::SpriteAtlas& a);

Color mix(Color a, Color b, float t);

// Flat prototype fallback, also used directly where a plain box is wanted.
void frame(int x, int y, int w, int h, bool hot, bool bright = false);

void panel(const render::SpriteAtlas& a, int x, int y, int w, int h);

// hot = hovered, on = destructive/primary accent, off = unavailable.
void button(const render::SpriteAtlas& a, int x, int y, int w, int h, bool hot, bool on,
            bool off = false);

// A ribbon's source is only as tall as its slice inset, so its middle row has
// zero source height and drawing one taller than its art leaves an unpainted
// band. Height always comes from the texture, never the caller, which also
// keeps it at 1:1 -- stretching 32px to 44px is a fractional scale and shimmers.
int ribbonH(const render::SpriteAtlas& a, const char* art);
void ribbon(const render::SpriteAtlas& a, const char* art, int x, int y, int w);

void centredIn(const char* t, int cx, int y, int size, Color c);

}  // namespace td::ui::paint
