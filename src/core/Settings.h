#pragma once

namespace td::core {

// Player options that are not gameplay.
//
// Measured against Steam's published accessibility feature list, which now shows
// on a store page and which players can filter by, this game failed three
// categories outright: Color Alternatives, Camera Comfort (it shakes the screen
// with no way to turn that off) and Adjustable Text Size. Adjustable Difficulty
// and Custom Volume Controls were already satisfied.
struct Settings {
    // Color Alternatives. When on, anything encoded by hue also carries a text
    // tag. Off by default because the tags cost space, on is one click away.
    bool colorAlternatives = false;

    // Camera Comfort. Screen shake multiplier: 1 full, 0.45 reduced, 0 off.
    // A multiplier rather than a bool so "less" is available as well as "none" --
    // shake is feedback, and removing it entirely costs the player information.
    float shake = 1.0f;

    float music = 0.5f;
    float sfx = 0.85f;
};

inline constexpr float kShakeLevels[3] = {1.0f, 0.45f, 0.0f};
inline constexpr const char* kShakeNames[3] = {"Full", "Reduced", "Off"};

// Which preset a stored multiplier corresponds to, for drawing the option.
inline int shakeIndexOf(float v) {
    for (int i = 0; i < 3; ++i) {
        if (v >= kShakeLevels[i] - 0.01f && v <= kShakeLevels[i] + 0.01f) return i;
    }
    return 0;
}

}  // namespace td::core
