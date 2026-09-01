#pragma once

#include <cstdint>
#include <random>
#include <string>

namespace td::core {

// One seeded generator per run. Injected everywhere randomness is needed so the
// whole simulation is reproducible from a seed. rand() and std::random_device
// are prohibited mid-run.
class Rng {
public:
    explicit Rng(uint64_t seed) : eng_(seed), seed_(seed) {}

    float unit();                        // [0,1)
    bool chance(float p);                // p<=0 and p>=1 short-circuit without drawing
    int range(int lo, int hiInclusive);  // inclusive at both ends

    uint64_t seed() const { return seed_; }

    // Full engine state, so a resumed run continues the exact sequence it was
    // saving mid-stream. Seed alone is not enough once draws have been made.
    std::string state() const;
    void setState(const std::string& s);

private:
    std::mt19937_64 eng_;
    uint64_t seed_;
};

}  // namespace td::core
