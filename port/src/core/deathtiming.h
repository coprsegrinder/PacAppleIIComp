// TASK-033-3b: when the things of the death sequence happen, in cycles after the end of the catching tick's own drawing.
// Built from the oracle's recordings of 245+ deaths (tools/death_timing.py, tools/death_lib.py; docs/M8_PLAN.md): the stillness, the erase of the ghosts and Pac-Man, the six
// death frames with their sounds, the rebuild of the screen, READY, and the wait before the next tick. The parts that depend on where Pac-Man dies (his shift x mod 7) and on what the
// rebuild has to draw (dots, energizers, score digits, spare lives) are modelled; what is left (page-crossing cycles of about 0-66 per frame, one unexplained outlier of 1,700 cycles in
// 220 rebuilds) is measured in tests/test_deathtiming.cpp against the recordings.
#pragma once

#include <array>
#include <cstdint>

namespace pacman::core {

struct DeathTimingInput {
    int pacX = 0;               // Pac-Man's place when he was caught (pixels)
    int dots = 0;               // small dots still on the maze (the rebuild draws them again)
    int energizers = 0;         // energizers still on the maze
    int scoreDigits = 1;        // digits shown for the player's score and for the high score (at least 1 each)
    int highDigits = 1;
    int livesBefore = 3;        // $58 & $7F as it was before the catch
};

struct DeathTimeline {
    std::array<std::int64_t, 5> clear{};        // ghosts 4, 3, 2, 1 and Pac-Man erased
    std::int64_t image = 0;                     // Pac-Man's last image drawn
    std::int64_t imageClear = 0;                // ... and erased
    std::array<std::int64_t, 6> frame{};        // the six death frames drawn
    std::array<std::int64_t, 6> sound{};        // their sounds start
    std::array<std::int64_t, 6> frameClear{};   // each frame erased
    std::int64_t rebuild = 0;                   // the screen of the new life starts to be drawn
    std::int64_t ready = 0;                     // the READY text is drawn
    std::int64_t lastBlit = 0;
    std::int64_t total = 0;                     // the next tick starts
};

DeathTimeline deathTimeline(const DeathTimingInput& in);

// The last life: the same animation, then the words GAME OVER, then a long wait (measured on 5 runs, identical every time)
inline constexpr std::int64_t kGameOverTextAfterLastClear = 18'585;   // from the erase of the last death frame to the first letter
inline constexpr std::int64_t kGameOverTextDraw = 2'052;               // the text is drawn in this time
inline constexpr std::int64_t kGameOverPause = 10'453'613;             // from the last letter to the first drawing of the next screen (attract)

}  // namespace pacman::core
