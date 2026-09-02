#pragma once

#include <string>

namespace td::sim {

class World;

// The guided first run.
//
// The game had five one-shot hint lines and called it onboarding. It has five
// tower types, three specialisations each, six elements with three each, twelve
// skill trees, targeting priorities, two abilities and an early-call economy --
// and roughly ten minutes to explain any of it, because that is when most people
// who are going to quit have quit.
//
// Three rules taken from the onboarding research and enforced by the shape of
// this type:
//
//  1. ONE mechanic at a time, introduced when it becomes relevant. Hence a
//     single current step rather than a panel of instructions.
//  2. Learn by doing. Every step is gated on the player actually performing the
//     action -- `satisfied()` observes the world, it is never a timer and never
//     a "click to continue".
//  2b. A step the player cannot yet perform WAITS, and says what would unlock
//     it. It used to claim it was skipped, and was not: levelling a tower is
//     bought in the skill trees, so on a first run -- the only run a tutorial
//     ever sees -- that step could never be satisfied and the tutorial stalled
//     on it permanently. Anything sequenced behind it was never taught at all.
//     Steps that depend on a purchase therefore come LAST, so a first run is
//     taught everything it can actually do.
//  3. Always skippable, and never repeated once finished.
// The ORDER of this enum is the save format -- it is serialised as an integer
// into the profile -- so the sequence lives in `tutorialNext`, not here.
enum class TutorialStep {
    Build,      // place the first tower
    StartWave,  // send the wave
    Inspect,    // open a tower
    Upgrade,    // spend gold on a level
    Ability,    // use Strike
    Done,
};

struct TutorialPrompt {
    const char* title;
    const char* body;
};

// `levelsUnlocked` is whether this profile may raise a tower past level 1. Tower
// levels are bought in the skill trees, so on a first run they are not
// available, and the step that asks for one has to say where they come from
// instead of asking for something the player cannot do.
TutorialPrompt tutorialPrompt(TutorialStep s, bool levelsUnlocked = true);

// What the player has to be doing on screen for a step to count. `menuOnTower`
// is the one piece of UI state the simulation cannot see; passing it in keeps
// this raylib-free and therefore testable.
bool tutorialSatisfied(TutorialStep s, const World& w, bool menuOnTower);

TutorialStep tutorialNext(TutorialStep s);

// Serialised as a small integer in the profile, so a player who quits mid
// tutorial resumes where they were rather than starting again.
int tutorialToIndex(TutorialStep s);
TutorialStep tutorialFromIndex(int i);

}  // namespace td::sim
