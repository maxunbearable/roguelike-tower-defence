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
//     a "click to continue". A step the player cannot yet perform is skipped.
//  3. Always skippable, and never repeated once finished.
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

TutorialPrompt tutorialPrompt(TutorialStep s);

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
