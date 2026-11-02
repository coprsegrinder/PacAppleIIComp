// The start of a life (TASK-029f2), rebuilt from $53E6-$5437 and the player save routines $53BE / $53CF; used after Pac-Man was caught and at the start of a
// game. Checked against the oracle recordings (the state at the first tick after each catch) and, through newGameState, against tick 0 of every scenario.
//
//   $53E6:  Pac-Man's 14 bytes $60-$6D := table $5446 (position $8B/$8F, directions, junctions $2C/$2D, cell counters, ...);
//           the four ghost records $84-$D7 := table $5454 (84 bytes; $D4-$D7 lie outside the logical state);
//           $59-$5F := 0;  the player's saved bytes $52-$59 are loaded back ($53CF);  $E9 := 1 ($EA/$EB, $E4, $E7, $1F outside the logical state);
//           the in-house countdowns of ghosts 2, 3 and 4 (record byte 18) := $54A4 / $54B4 / $54C4 [min(level, 15)].
//   $53BE / $53CF: the player's bytes $52-$59 (score, level, items left, fruit counter, lives, step counter) are kept in a save slot of 8 bytes per player.
// NOT rebuilt: the maze save/restore of the 2-player game ($5394 / $5360: it copies 4 x 128 bytes between the map and a per-player store), the drawing
// ($5659, $5850, $5839), the get-ready jingle and its delay loops, the game-over exit ($6EC3).
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "gamestate.h"

namespace pacman::core {

struct StartTables {
    std::array<std::uint8_t, 14> pacman{};     // $5446
    std::array<std::uint8_t, 80> ghosts{};     // first 80 of the 84 bytes at $5454
    std::array<std::uint8_t, 16> house2{}, house3{}, house4{};   // $54A4, $54B4, $54C4
};

struct PlayerSave {
    std::array<std::uint8_t, 8> bytes{};       // $52-$59 as saved by $53BE
};

bool loadStartTables(const datapack::Pack& pack, StartTables& out);   // false if the pack lacks the 142 bytes at $5446
PlayerSave savePlayer(const GameState& state) noexcept;               // $53BE
void loadPlayer(GameState& state, const PlayerSave& save) noexcept;   // $53CF
void startOfLife(GameState& state, const StartTables& tables, const PlayerSave& save) noexcept;   // $53E6-$5437

}  // namespace pacman::core
