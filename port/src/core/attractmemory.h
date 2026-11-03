// What the original keeps in memory between the screens and what the attract game starts from (TASK-033-6c): `$51` the number of players the last prompt took (1 until a digit is
// pressed), `$50` the player who played last, `$52-$59` his bytes (score, level, items, fruit counter, lives, step counter), the eight bytes per player saved at `$0300` (players
// 1-5: the final scores of the last game stay), and the high score `$00-$02`. The demo shows all of it: the labels and scores of `$51` players, the high score, the level's fruit,
// and it plays on level 1 (the chase sets `$55` = 1 at `$47D5` before the demo). Fresh (after the boot menus): one player, score 0, level 1.
#pragma once

#include <array>
#include <cstdint>

namespace pacman::core {

struct AttractMemory {
    int players = 1;
    int current = 1;
    std::array<std::uint8_t, 8> live{0, 0, 0, 1, 0xF8, 0x40, 0, 1};
    std::array<std::array<std::uint8_t, 8>, 6> saves{};
    std::array<std::uint8_t, 3> high{};
    std::uint8_t e5 = 0, e6 = 0;   // `$E5` / `$E6`: the 1UP blink counter and phase; the attract game counts them on (0, 1, 2, ... in successive turns) and a game leaves its own
    std::uint8_t f3 = 0;            // `$F3`: the dot penalty counter: a game that ends with it set makes the demo's first steps wait (the 3-player capture: 18 ticks longer)
};

}  // namespace pacman::core
