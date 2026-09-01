#include "core/Rng.h"

#include <sstream>

namespace td::core {

float Rng::unit() {
    // Taking the top 24 bits and dividing by 2^24 keeps the result strictly
    // below 1.0f. uniform_real_distribution<float> can round up to exactly 1.0f,
    // which would let a chance(0.999) roll fail its own bounds check.
    constexpr float kInv24 = 1.0f / 16777216.0f;  // 2^-24
    return static_cast<float>(eng_() >> 40) * kInv24;
}

bool Rng::chance(float p) {
    if (p <= 0.0f) return false;
    if (p >= 1.0f) return true;
    return unit() < p;
}

int Rng::range(int lo, int hiInclusive) {
    if (hiInclusive <= lo) return lo;
    // Mapped by hand rather than via uniform_int_distribution, whose consumption
    // pattern is implementation-defined and would make saved seeds replay
    // differently across platforms.
    const int span = hiInclusive - lo + 1;
    return lo + static_cast<int>(unit() * static_cast<float>(span));
}

}  // namespace td::core

namespace td::core {

std::string Rng::state() const {
    std::ostringstream os;
    os << eng_;
    return os.str();
}

void Rng::setState(const std::string& s) {
    std::istringstream is(s);
    is >> eng_;
}

}  // namespace td::core
