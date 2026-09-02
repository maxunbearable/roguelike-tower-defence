#include "render/Capture.h"

#include <algorithm>

namespace td::render {

Image captureScreen() {
    Image img = LoadImageFromScreen();
#ifdef TD_HEADLESS
    ImageFlipVertical(&img);
    auto* px = static_cast<unsigned char*>(img.data);
    for (int i = 0; i < img.width * img.height; ++i)
        std::swap(px[i * 4], px[i * 4 + 2]);
#endif
    return img;
}

}  // namespace td::render
