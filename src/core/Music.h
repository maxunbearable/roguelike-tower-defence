#pragma once

#include <cstdint>
#include <vector>

#include "core/Synth.h"

namespace td::core {

// Procedurally composed music.
//
// The game had no music at all, which is the single loudest "this is a
// prototype" signal a game can send. Rather than license a soundtrack, this
// composes one from the same synthesiser every sound effect already uses: no
// asset, no licence question, and it ships inside the binary.
//
// Raylib-free on purpose, so the composition can be unit-tested -- a loop that
// clicks at the seam or drifts in DC offset is audible and worth a test.
enum class Track {
    Hub,     // slow, sparse: pad and a wandering arpeggio. For menus.
    Battle,  // adds a bass pulse and percussion. For a run.
};

// One bar-aligned loop, seamless end to start.
Pcm composeTrack(Track track);

// Wraps 16-bit mono PCM at kSampleRate in a RIFF/WAVE header, so it can be
// handed to a decoder that wants a file rather than samples. Returned as bytes.
std::vector<uint8_t> toWav(const Pcm& pcm);

}  // namespace td::core
