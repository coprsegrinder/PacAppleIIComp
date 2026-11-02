// Drawing the play screen from the game state (TASK-029g). First part (g1): the playfield (walls in colour mode, the dots and energizers of the CURRENT
// map) and Pac-Man's sprite, as the original draws them; checked against the oracle's hi-res page at the first tick of a game (docs/M4_PLAN.md).
// Also drawn: the ghost-house door. Not yet drawn: the score and its labels, the lives and fruit icons, the ghosts (later parts: g2-g4).
#pragma once

#include <array>
#include <cstdint>

#include "../datapack/datapack.h"
#include "../hires/hires.h"
#include "gamestate.h"

namespace pacman::core {

// Pac-Man's frame number 0..8 ($79A4-$79C7): phase = $66 & 3; phase 0: closed mouth (0); phases 1 and 3: direction + 1; phase 2: direction + 5.
int pacmanFrame(const GameState& state) noexcept;

// The page of a fresh play screen: cleared to $80, walls (table $5F69, colour mode), then the items of the state's map ($6113 pass).
// False if the pack lacks a table.
bool drawPlayfield(hires::Page& page, const GameState& state, const datapack::Pack& pack);

// The score area, labels, high score, level icon and lives icons as the original draws them while it builds the screen ($550A, $70CC, $56CF; the call list
// was read from the oracle). `highScore` is the three BCD bytes of the high score (low first; outside the game state, 0 in a fresh run). Verified only for the
// first screen of a game: score 0, 3 lives, level 1; the digit loop and the lives rule are read from the code, the level icon is the one of level 1.
// players = 2 adds the second player's label ("2UP": column 32, mark = the digit 2 at x 271, dotted line x 228-277, score box 2, score digits from x 266, y 16;
// positions from the table at $55C7 + 8 x 2); player2Score is that player's score (BCD, 24 bits; outside the game state, 0 at the start).
bool drawHud(hires::Page& page, const GameState& state, const datapack::Pack& pack, const std::array<std::uint8_t, 3>& highScore = {0, 0, 0}, int players = 1,
             std::uint32_t player2Score = 0);

// Pac-Man's 3 x 10 image through the dispatcher at his position ($79A4-$79CA: frame table $96E8). orMode = plain OR ($1E = $80, the draw of every tick);
// the very first draw of a life ($5839: $1E = 0, always frame 0 at the start position) ORs and then clears the palette bit instead.
bool drawPacman(hires::Page& page, const GameState& state, const datapack::Pack& pack, bool orMode = true);

}  // namespace pacman::core
