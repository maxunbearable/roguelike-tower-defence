#pragma once

#include "raylib.h"

namespace td::render {

// The framebuffer as an image, normalised across backends: the software
// rasteriser used for display-free capture returns it bottom-up with red and
// blue transposed. Callers own the result.
Image captureScreen();

}  // namespace td::render
