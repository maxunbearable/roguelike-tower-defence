#pragma once

#include <string>
#include <vector>

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

// Splits text into at most `maxLines` lines that each fit `maxW` pixels, broken
// on word boundaries, with an ellipsis if it still will not fit.
//
// The map cards drew their blurb with a single centred call and no width limit.
// Card is 250px; the blurbs run to 69 characters. Every one of them overflowed,
// and since neighbouring cards are 20px apart the text of five maps ran into
// each other -- invisible until a profile had unlocked more than one map, which
// is why it survived every screenshot taken from a fresh save.
std::vector<std::string> wrapToWidth(const std::string& text, int maxW, int size, int maxLines);

// The parchment the skill tree panel is painted with, measured off the rendered
// panel (the panel itself comes from the sprite atlas, so this is a reference
// value, not its source). Link colours must contrast against THIS, which is the
// thing that was never checked.
inline constexpr Color kTreePanel{204, 184, 141, 255};

// The colour a prerequisite link is drawn in. `walked` means the prerequisite is
// owned.
//
// Both states must read against kTreePanel. A walked link used to be drawn in
// the raw branch tint, and the trunk tint is (198,178,148) against a panel of
// (204,184,141) -- nine luma of contrast, which is invisible. Emphasis is
// carried by width and saturation instead, never by lightness.
Color linkColour(Color branchTint, bool walked);

// Branch colour coding. The eye groups by hue long before it reads a label,
// which is what makes three specialisation paths legible at a glance instead of
// one undifferentiated web. Lives here, beside `linkColour`, because a tint and
// the contrast it has to achieve are the same question.
Color branchTint(const std::string& branch);

// Every branch a tree can name, so a test can check all of them rather than the
// ones somebody remembered.
const std::vector<std::string>& branchNames();

}  // namespace td::ui::paint
