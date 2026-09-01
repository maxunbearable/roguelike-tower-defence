#include "sim/Tutorial.h"

#include "sim/Components.h"
#include "sim/World.h"

namespace td::sim {

TutorialPrompt tutorialPrompt(TutorialStep s) {
    switch (s) {
        case TutorialStep::Build:
            return {"Build a tower", "Click one of the marked plots beside the road."};
        case TutorialStep::StartWave:
            return {"Send the wave", "Press SPACE, or the button on the right. Calling it "
                                     "early pays you the time you skip."};
        case TutorialStep::Inspect:
            return {"Open your tower", "Click it. The panel shows exactly what it does."};
        case TutorialStep::Upgrade:
            return {"Spend your gold", "Level the tower up. Gold is scarce here, so every "
                                       "coin is a decision."};
        case TutorialStep::Ability:
            return {"Call down a Strike", "Press Q, then click the road. It is free and on a "
                                          "cooldown, so it is always yours."};
        case TutorialStep::Done:
            break;
    }
    return {"", ""};
}

TutorialStep tutorialNext(TutorialStep s) {
    switch (s) {
        case TutorialStep::Build: return TutorialStep::StartWave;
        case TutorialStep::StartWave: return TutorialStep::Inspect;
        case TutorialStep::Inspect: return TutorialStep::Upgrade;
        case TutorialStep::Upgrade: return TutorialStep::Ability;
        case TutorialStep::Ability: return TutorialStep::Done;
        case TutorialStep::Done: break;
    }
    return TutorialStep::Done;
}

bool tutorialSatisfied(TutorialStep s, const World& w, bool menuOnTower) {
    switch (s) {
        case TutorialStep::Build:
            return w.towerCount() > 0;
        case TutorialStep::StartWave:
            return w.phase() == Phase::Wave;
        case TutorialStep::Inspect:
            return menuOnTower;
        case TutorialStep::Upgrade: {
            // Any tower past level 1. Asking for a SPECIFIC tower would strand a
            // player who sold the one they started with.
            bool levelled = false;
            w.reg().view<const TowerTag>().each([&](const TowerTag& t) {
                if (t.level > 1) levelled = true;
            });
            return levelled;
        }
        case TutorialStep::Ability:
            // Strike has been fired: it is the only way the cooldown is not 0.
            return w.abilityCooldown(Ability::Strike) > 0.0f;
        case TutorialStep::Done:
            return true;
    }
    return false;
}

int tutorialToIndex(TutorialStep s) { return static_cast<int>(s); }

TutorialStep tutorialFromIndex(int i) {
    if (i < 0 || i > static_cast<int>(TutorialStep::Done)) return TutorialStep::Done;
    return static_cast<TutorialStep>(i);
}

}  // namespace td::sim
