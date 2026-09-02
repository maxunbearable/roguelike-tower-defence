#include "content/WaveGen.h"

#include "content/Registry.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace td::content {

namespace {

// Draws the next primary from a bag holding one of each unlocked type, refilled
// and reshuffled when it empties. That keeps every type appearing exactly as
// often as the plain rotation did -- the property difficulty is calibrated on --
// while the order differs per run. `shuffled` false reproduces the original
// rotation exactly, which is what the canonical expansion needs.
struct Cycle {
    std::mt19937_64 rng;
    bool shuffled = false;
    std::vector<size_t> bag;
    size_t poolSize = 0;

    void refill(size_t n) {
        bag.resize(n);
        std::iota(bag.begin(), bag.end(), size_t{0});
        if (shuffled) std::shuffle(bag.begin(), bag.end(), rng);
        // Drawn from the back, so reverse to keep the unshuffled case in order.
        std::reverse(bag.begin(), bag.end());
    }
    // A newly unlocked type restarts the cycle, so it can appear immediately
    // rather than waiting out the tail of the previous one.
    void ensure(size_t n) {
        if (n != poolSize) {
            poolSize = n;
            bag.clear();
        }
        if (bag.empty()) refill(n);
    }
    // The unshuffled path is the ORIGINAL formula, not a bag that happens to
    // agree with it. Restarting a cycle when the pool grows is right for a run
    // -- a newly unlocked creature should appear promptly rather than waiting
    // out the tail of the previous cycle -- but it is a different sequence: on
    // greenfields it moves 39 of 50 waves. The canonical expansion is what every
    // difficulty measurement in this project was calibrated against, so it has
    // to come through untouched.
    size_t draw(size_t n, int w0) {
        if (!shuffled) return static_cast<size_t>(w0) % n;
        ensure(n);
        const size_t i = bag.back();
        bag.pop_back();
        return i;
    }
    size_t peek(size_t n, int w0) {
        if (!shuffled) return (static_cast<size_t>(w0) + 1) % n;
        ensure(n);
        return bag.back();
    }
};

std::vector<WaveDef> expand(const WaveRecipe& r, Cycle& cycle, const Registry* reg) {
    std::vector<WaveDef> out;
    out.reserve(static_cast<size_t>(std::max(0, r.count)));

    for (int wave = 1; wave <= r.count; ++wave) {
        // Which enemies have unlocked by now, in authored order.
        std::vector<const WavePoolEntry*> eligible;
        for (const auto& p : r.pool) {
            if (wave >= p.fromWave) eligible.push_back(&p);
        }
        if (eligible.empty()) continue;

        const int w0 = wave - 1;  // zero-based, so wave 1 has no scaling applied
        const float bent = std::pow(static_cast<float>(w0), r.hpCurveExp);
        const float hpMult = std::pow(r.hpPerWave, bent);
        const float armorAdd = r.armorPerWave * static_cast<float>(w0);
        const float bountyMult = std::pow(r.bountyPerWave, static_cast<float>(w0));
        const float interval =
            std::max(r.intervalMin, r.intervalBase * std::pow(r.intervalDecay,
                                                              static_cast<float>(w0)));
        const int count =
            r.countBase + static_cast<int>(r.countPerWave * static_cast<float>(w0));

        WaveDef def;
        def.delay = r.delay;

        const size_t n = eligible.size();

        // Holds a group's health and payout where the canonical expansion put
        // them, whichever creature the run happens to draw. Without a registry
        // (the canonical expansion itself) it is the identity.
        const auto matched = [&](const std::string& drawn, const std::string& canon, int c,
                                 float hp, float bounty) {
            struct Adjusted {
                int count;
                float hpMult;
                float bountyMult;
            } a{c, hp, bounty};
            if (reg == nullptr || drawn == canon) return a;
            const float hpDrawn = reg->enemy(drawn).maxHp;
            const float hpCanon = reg->enemy(canon).maxHp;
            if (hpDrawn <= 0.0f || hpCanon <= 0.0f) return a;

            // Count carries a little of the compensation and no more.
            //
            // Matching health exactly by count alone is a trap: lives are lost
            // per leaked ENEMY, so a wave of three heavy creatures costs a
            // fraction of what fourteen light ones cost even when they weigh the
            // same. Allowed a threefold swing, that turned a health lottery into
            // a leak lottery -- the campaign's opening map ranged from 10 to 18
            // waves survived depending only on the seed.
            //
            // A modest swing keeps a creature recognisably itself (a brute wave
            // is a smaller wave) while leaving leak pressure roughly fixed. The
            // rest goes into the health multiplier below.
            const float ratio = std::clamp(hpCanon / hpDrawn, 0.7f, 1.4f);
            const int wanted = static_cast<int>(std::lround(static_cast<float>(c) * ratio));
            a.count = std::clamp(wanted, 1, std::max(1, c * 2));

            // Whatever the count could not absorb goes into the health
            // multiplier, so the group brings EXACTLY the health the canonical
            // expansion would have brought. Approximate compensation left 14%
            // of difficulty riding on the seed over the first twelve waves.
            const float canonHealth = static_cast<float>(c) * hpCanon;
            const float drawnHealth = static_cast<float>(a.count) * hpDrawn;
            if (drawnHealth > 0.0f) a.hpMult = hp * (canonHealth / drawnHealth);

            // ...and the same for the purse, or a run that drew heavy creatures
            // would be poorer than one that drew light ones, which is the same
            // lottery wearing a different hat.
            const int bDrawn = reg->enemy(drawn).bounty;
            const int bCanon = reg->enemy(canon).bounty;
            if (bDrawn > 0 && bCanon > 0) {
                a.bountyMult = bounty * (static_cast<float>(c) * static_cast<float>(bCanon)) /
                               (static_cast<float>(a.count) * static_cast<float>(bDrawn));
            }
            return a;
        };

        const auto* primary = eligible[cycle.draw(n, w0)];
        const auto* canonPrimary = eligible[static_cast<size_t>(w0) % n];
        const auto pAdj =
            matched(primary->enemyId, canonPrimary->enemyId, count, hpMult, bountyMult);
        def.groups.push_back(WaveGroup{primary->enemyId, pAdj.count, interval, 0.0f, pAdj.hpMult,
                                       armorAdd, pAdj.bountyMult});

        // From secondaryFromWave onward, waves are mixed rather than pure.
        if (wave >= r.secondaryFromWave && n > 1) {
            // The type that leads the NEXT wave, exactly as the rotation did:
            // it keeps secondaries as evenly spread as primaries.
            const auto* secondary = eligible[cycle.peek(n, w0)];
            const auto* canonSecondary = eligible[(static_cast<size_t>(w0) + 1) % n];
            const int sCount =
                std::max(1, static_cast<int>(static_cast<float>(count) * r.secondaryFraction));
            const auto sAdj = matched(secondary->enemyId, canonSecondary->enemyId, sCount,
                                      hpMult, bountyMult);
            def.groups.push_back(WaveGroup{secondary->enemyId, sAdj.count, interval,
                                           r.secondaryDelay, sAdj.hpMult, armorAdd,
                                           sAdj.bountyMult});
        }

        // Bosses last, so they sit after the escort groups in spawn order.
        for (const auto& b : r.bosses) {
            if (b.wave != wave || b.enemyId.empty()) continue;
            def.groups.push_back(WaveGroup{b.enemyId, 1, 1.0f, r.bossDelay, hpMult, armorAdd,
                                           bountyMult});
        }

        out.push_back(std::move(def));
    }
    return out;
}

}  // namespace

std::vector<WaveDef> generateWaves(const WaveRecipe& r) {
    Cycle cycle{std::mt19937_64{0}, /*shuffled=*/false, {}, 0};
    return expand(r, cycle, nullptr);
}

std::vector<WaveDef> generateWaves(const WaveRecipe& r, uint64_t seed, const Registry& reg) {
    Cycle cycle{std::mt19937_64{seed}, /*shuffled=*/true, {}, 0};
    return expand(r, cycle, &reg);
}

}  // namespace td::content
