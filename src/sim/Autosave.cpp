#include "sim/Autosave.h"

#include "sim/World.h"

namespace td::sim {

SaveMark saveMarkOf(const World& w) {
    return SaveMark{w.waveIndex(), w.towerCount(), w.gold()};
}

bool shouldAutosave(const World& w, const SaveMark& lastWritten) {
    // canSnapshot is the hard requirement: a run may only be written during a
    // build phase with nothing on the field, because enemies and projectiles are
    // not serialised.
    if (!w.canSnapshot()) return false;
    return saveMarkOf(w) != lastWritten;
}

}  // namespace td::sim
