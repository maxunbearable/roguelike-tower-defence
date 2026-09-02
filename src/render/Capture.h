#pragma once

#include "raylib.h"

namespace td::render {

// Reads the current framebuffer back as an image, normalised so a capture looks
// the same whichever backend produced it. Callers own the returned Image.
//
// The software rasteriser used for display-free capture hands its framebuffer
// back bottom-up and with red and blue transposed, and raylib's readback does
// not correct either for that backend -- verified by drawing known colours,
// where RED came back blue and ORANGE came back cyan. The GL path needs no
// correction, so this is the one place that knows the difference.
Image captureScreen();

}  // namespace td::render
