// Drawing the play screen from the game state (TASK-029g). First part (g1): the playfield (walls in colour mode, the dots and energizers of the CURRENT
// map) and Pac-Man's sprite, as the original draws them; checked against the oracle's hi-res page at the first tick of a game (docs/M4_PLAN.md).
// Also drawn: the ghost-house door. Not yet drawn: the score and its labels, the lives and fruit icons, the ghosts (later parts: g2-g4).
#pragma once

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

// Pac-Man's 3 x 10 image through the dispatcher at his position ($79A4-$79CA: frame table $96E8). orMode = plain OR ($1E = $80, the draw of every tick);
// the very first draw of a life ($5839: $1E = 0, always frame 0 at the start position) ORs and then clears the palette bit instead.
bool drawPacman(hires::Page& page, const GameState& state, const datapack::Pack& pack, bool orMode = true);

}  // namespace pacman::core
