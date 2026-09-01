#include "content/WaveGen.h"

#include <algorithm>
#include <cmath>

namespace td::content {

std::vector<WaveDef> generateWaves(const WaveRecipe& r) {
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
        const auto* primary = eligible[static_cast<size_t>(w0) % n];
        def.groups.push_back(WaveGroup{primary->enemyId, count, interval, 0.0f, hpMult, armorAdd,
                                       bountyMult});

        // From secondaryFromWave onward, waves are mixed rather than pure.
        if (wave >= r.secondaryFromWave && n > 1) {
            const auto* secondary = eligible[(static_cast<size_t>(w0) + 1) % n];
            const int sCount =
                std::max(1, static_cast<int>(static_cast<float>(count) * r.secondaryFraction));
            def.groups.push_back(WaveGroup{secondary->enemyId, sCount, interval, r.secondaryDelay,
                                           hpMult, armorAdd, bountyMult});
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

}  // namespace td::content
