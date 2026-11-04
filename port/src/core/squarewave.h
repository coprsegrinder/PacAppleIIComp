// TASK-032-4 (M7): the Apple II speaker as audio. The speaker is a plain toggle: every click ($C030) flips the cone, so a list of click times (in CPU cycles, 1,022,727 per
// second) IS the sound. The faithful rendering is a square wave whose edges sit at the click times; no filtering. This file is pure (no operating-system calls).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pacman::core {

inline constexpr std::int64_t kCpuHz = 1'022'727;   // the Apple II's clock

struct Pcm {
    int sampleRate = 44100;
    std::vector<std::int16_t> samples;   // mono, signed 16 bit
    std::size_t nominal = 0;             // the length the sound occupies in the original's timeline (its freeze); 0 = all samples. Samples beyond it are the speaker's ring-out, which may overlap the next sound
};

// Renders the clicks (cycles since the first click) into PCM. The wave starts silent, the first click raises the cone (+amplitude), every further click flips it; an edge
// lands in sample floor(cycle x rate / kCpuHz). After the last click the level is held for 1 ms and then released to 0 (no lasting DC offset); the buffer is padded with
// silence up to `totalCycles` (the freeze length of the sound, so that consecutive sounds keep the original's spacing). `amplitude` is moderate on purpose (a
// full-scale square wave is painfully loud).
Pcm renderClicks(const std::vector<std::uint32_t>& clicks, std::int64_t totalCycles, int sampleRate = 44100, std::int16_t amplitude = 5000);

// TASK-033-S: the speaker as a physical part. The Apple II drives a small cone through a coupling capacitor; a click (and the very short bursts of the dot and energizer
// sounds, 2-8 ms) does not come out as a square edge but as a low "thump" at the cone's resonance, plus a rolled-off click. The raw square wave above is the electrical signal; this is what
// the speaker makes of it. The click lists (the game's part) are untouched, only the rendering differs. The numbers are ESTIMATES of a small speaker (not yet measured from a recording):
//   input   u = 2 x amplitude x (0/1 level of the cone drive, the same swing as the raw wave, the first click raises it, every click flips it; held for good, the capacitor removes the DC)
//   x       = first-order high-pass of u at couplingHz
//   output  = clickGain x first-order low-pass of x at rolloffHz  +  bodyGain x band-pass of x at bodyHz with quality bodyQ (the ring decays with tau = bodyQ / (pi x bodyHz))
struct SpeakerModel {
    double couplingHz = 25.0;
    double bodyHz = 120.0;
    double bodyQ = 2.5;
    double bodyGain = 2.0;
    double clickGain = 1.0;
    double rolloffHz = 4000.0;
    int tailMs = 80;   // ring-out kept after the sound's own length
};

// Same click lists and the same length rules as renderClicks, but through the speaker model: `samples.size()` = nominal + the tail. The default drive is half of the raw wave's: the thump adds to the peaks (about the same loudness as before).
Pcm renderSpeaker(const std::vector<std::uint32_t>& clicks, std::int64_t totalCycles, int sampleRate = 44100, std::int16_t amplitude = 2500, const SpeakerModel& model = SpeakerModel{});

}  // namespace pacman::core
