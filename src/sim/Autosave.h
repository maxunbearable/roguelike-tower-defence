#pragma once

namespace td::sim {

class World;

// When a run is worth writing to disk.
//
// The autosave used to fire on one condition -- the wave index changing -- which
// meant it captured the state at the START of a build phase and nothing after.
// Everything the player then did in that phase, which is the entire moment they
// spend their gold, stayed unwritten until the NEXT phase began. Close the window
// in between and it was all gone:
//
//   build phase N begins ............ autosave
//   build 3 towers, upgrade 2, specialise 1 ... not saved
//   fight wave N .................... not saved
//   close the window ................ all of it lost
//
// A run is snapshottable only during a build phase with an empty field, so gold
// cannot drift from bounties while this is being compared -- which makes gold,
// tower count and wave index together enough to notice every purchase, upgrade,
// specialisation and sale.
struct SaveMark {
    int waveIndex = -1;
    int towerCount = -1;
    int gold = -1;

    bool operator==(const SaveMark& o) const {
        return waveIndex == o.waveIndex && towerCount == o.towerCount && gold == o.gold;
    }
    bool operator!=(const SaveMark& o) const { return !(*this == o); }
};

SaveMark saveMarkOf(const World& w);

// True when the run both CAN be written and has changed since the last write.
bool shouldAutosave(const World& w, const SaveMark& lastWritten);

}  // namespace td::sim
