// TASK-032-4 (M7): the Apple II speaker as audio. The speaker is a plain toggle: every click ($C030) flips the cone, so a list of click times (in CPU cycles, 1,022,727 per
// second) IS the sound. The faithful rendering is a square wave whose edges sit at the click times; no filtering. This file is pure (no operating-system calls).
#pragma once

#include <cstdint>
#include <vector>

namespace pacman::core {

inline constexpr std::int64_t kCpuHz = 1'022'727;   // the Apple II's clock

struct Pcm {
    int sampleRate = 44100;
    std::vector<std::int16_t> samples;   // mono, signed 16 bit
};

// Renders the clicks (cycles since the first click) into PCM. The wave starts silent, the first click raises the cone (+amplitude), every further click flips it; an edge
// lands in sample floor(cycle x rate / kCpuHz). After the last click the level is held for 1 ms and then released to 0 (no lasting DC offset); the buffer is padded with
// silence up to `totalCycles` (the freeze length of the sound, so that consecutive sounds keep the original's spacing). `amplitude` is moderate on purpose (a
// full-scale square wave is painfully loud).
Pcm renderClicks(const std::vector<std::uint32_t>& clicks, std::int64_t totalCycles, int sampleRate = 44100, std::int16_t amplitude = 5000);

}  // namespace pacman::core
