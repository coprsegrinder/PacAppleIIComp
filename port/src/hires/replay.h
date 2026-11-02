// Replays a recorded, ordered list of drawing calls (tools/capture_script.py, format 'CSCR') with the hires layer. The script holds parameters only;
// all image/mask/tile data comes from the verified data pack. Used by the tests and by the window viewer.
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "../datapack/datapack.h"
#include "hires.h"

namespace pacman::hires {

struct ReplayStats {
    std::uint32_t calls = 0;
    int unusable = 0;      // calls whose data is not in the pack (e.g. the working buffer at $7E70)
    int lastUnusable = -1; // index of the last such call
    int lastClear = -1;    // index of the last page clear
    std::array<int, 13> used{};  // calls replayed per type (10 = page clear, 11 = vertical line, 12 = horizontal line)
    std::array<std::uint8_t, 32> recordedSha256{};  // the original's final-page hash stored in the script
};

// False if the script file is malformed. The page is replayed in place (starting from whatever it holds; a script normally begins with a clear).
// maxCalls: replay only the first maxCalls calls (0 = all), to reach a picture in the middle of a sequence.
bool replayScript(const datapack::Pack& pack, std::span<const std::uint8_t> file, Page& page, ReplayStats& stats, std::uint32_t maxCalls = 0);

}  // namespace pacman::hires
