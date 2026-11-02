// The sprite drawing of a tick (TASK-029g4, first part): which images the original draws when Pac-Man and the ghosts take a step, rebuilt from $79A4-$79CA
// (Pac-Man), $88A2-$88DB (ghost image choice) and the erase calls $699E. Returned as a list of calls with the parameters the original's routines receive,
// to be executed with the verified hires routines and compared with the call log of the oracle (tools/record_tick_calls.py).
//
// Rules (read from the code, checked against 700 ticks): an entity that moved gets (1) a masked clear at its OLD position minus (4, 4) with the mask table
// $69CD and (2) its 3 x 10 image at its NEW position. Order in a tick: Pac-Man first, then the ghosts 4, 3, 2, 1 ($86EA).
// Ghost image: X = 2 * ghost number; frightened: X = 0 (blue) or, while the frightened timer is below $80 and bit 4 of its low byte is clear, 6 (white);
// eyes: X = 10; then (normal drawing mode $1D <> 0) the flag $1E := table $88E1[X / 2] and X += 12; image = word at $9E5C + X.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../datapack/datapack.h"
#include "../hires/hires.h"
#include "gamestate.h"

namespace pacman::core {

struct DrawCall {
    int type = 0;                         // 3 = masked clear (p0 = mask table, p1 = x, p2 = y); 1 = 3 x 10 image (p0 = image base, p1 = x, p2 = y, p3 = $1D, p4 = $1E);
                                          // 4 = box clear (p0 = box index 0/1); 5 = small dot (p0 = x, p1 = y); 6 = tile (p0 = tile table address, p1 = x, p2 = y);
                                          // 2 = OR blit (p0 = data address, p1 = column, p2 = row, p3 = width, p4 = rows); 11 = energizer erase (p0 = x, p1 = y); 9 = shifted 3 x 10 image (p0 = image pointer, p1 = x, p2 = y, p3 = $1D, p4 = $1E: the score of an eaten ghost);
                                          // 10 = the 1UP box clear; 7 = 12 x 3 fruit image (p0 = image address, p1 = column, p2 = row); 12 = box clear (p0 = column, p1 = row);
                                          // 8 = horizontal line (p0 = x0, p1 = x1, p2 = y, p3 = step, p4 = the value of $1E at that moment, i.e. the ghost's flag: $5CF0 ORs it into the touched bytes, so the palette bit is set only if bit 7 of it is): the ghost-house door
    std::array<int, 5> p{};
    bool operator==(const DrawCall& o) const { return type == o.type && p == o.p; }
};

// The frightened timer words $E1 (low) / $E2 (high) are outside the logical state: passed in (0 = not frightened / unknown).
std::uint16_t ghostImageBase(const GameState& state, std::size_t ghost, std::uint16_t frightenedTimer, const datapack::Pack& pack, int& flag1E);

// Sprite calls for the step from `before` to `after` (positions compared). Empty if nothing moved or the pack lacks the tables.
// pacmanActed: Pac-Man's step timer ran out in this tick. An acting Pac-Man that did not move is BLOCKED ($7905-$7912): he is erased and redrawn standing
// ($66 was set to 2) at the same place.
std::vector<DrawCall> spriteCalls(const GameState& before, const GameState& after, const datapack::Pack& pack, bool pacmanActed = false, std::uint16_t frightenedTimer = 0);

// Zero-page values outside the logical state of the recordings that the drawing needs (recorded by tools/record_tick_calls.py at the start of each tick):
// the frightened timer $E1/$E2, the 1UP blink counter and phase $E5/$E6, the fruit state $E7, $E4.
struct TickExtras {
    std::uint8_t e1 = 0, e2 = 0, e5 = 0, e6 = 0, e7 = 0, e4 = 0;
    int player = 1;                              // $50: the current player (1 or 2) in a two-player game
    std::uint16_t frightenedTimer() const noexcept { return static_cast<std::uint16_t>(e1 | (e2 << 8)); }
};

// All the drawing calls of a tick that are derived from the state (TASK-029g5): the sprite calls, then in the order of the original
//   - Pac-Man's eating: when the score changed, the score display ($7091): if the score is at least the high score, the high score takes the score's value
//     and is drawn first (box clear 0, digits at x 40, y 94); then the score (box clear 1, digits at x 40, y 16); digits right to left, 6 pixels apart;
//   - after each ghost's image: when the ghost's cell counters show that it has just left a cell that still holds a dot, the small dot of that cell is redrawn
//     ($895F: leaving a cell is when the sub-counter wrapped: sub-counter 0 after the step: right/down: cell = counter - 1; left/up: cell = counter + 1).
// `highScore` (three BCD bytes, low first) is the display's own state and is updated. NOT included: the energizer blink, the "1UP" blink, fruit.
struct DisplayState {
    std::array<std::uint8_t, 3> high{0, 0, 0};   // the high score on the screen (BCD, low byte first)
    int lastFlag1E = 0;                          // the zero-page flag $1E as the last image draw left it: the door lines ($5CF0) use it for the palette bit
};
std::vector<DrawCall> tickCalls(const GameState& before, const GameState& after, const datapack::Pack& pack, bool pacmanActed, DisplayState& display,
                                const TickExtras& extrasBefore = {}, const TickExtras& extrasAfter = {});

// Executes one call on the page with the verified hires routines (all call types of this file; the data comes from the pack). False if the pack lacks data.
bool executeCall(hires::Page& page, const DrawCall& call, const datapack::Pack& pack);

// The drawing calls of the two long "blocking" sequences (TASK-029g9), as the original issues them INSIDE one tick. Both end with the rebuilt screen; the walls are
// redrawn by line routines that the call collector does not log (and the maze flash of the level end likewise), so they are not part of the lists.
//   death (after the ghosts' steps and the 1UP blink): erase the four ghosts (4, 3, 2, 1) and Pac-Man, Pac-Man's frame, six shifted death frames ($7F13 + 30 k), each
//     followed by a clear with the second mask table; then the dot pass, the screen's labels/score/high score/level icon/lives icons, Pac-Man at his start,
//     the READY text and Pac-Man again;
//   level end (after the last item's step and score display): Pac-Man erased and drawn again, the dot pass of the NEW map, the same labels..., Pac-Man at his start.
enum class RestartKind { Death, LevelEnd, GameOver };   // GameOver: the death of a player's last life: the death head, then the text "GAME" (column 15) and "OVER" (column 21) at row 105 instead of a rebuilt screen

struct RestartInputs {
    int pacX = 0, pacY = 0;                     // Pac-Man's place when the sequence starts
    int pacImage = 0;                           // the image address of his last frame (the step draw of that tick)
    int players = 1, current = 1;               // two-player games: both labels and scores are drawn; `current` = the player whose data `after` holds
    std::uint32_t otherScore = 0;               // the other player's score (BCD)
    std::array<int, 8> ghostXY{};               // ghost 1..4 places (x, y pairs) when the sequence starts (death only)
};

// `after` is the state of the new life / new level (map, score, lives, level); display.high is used for the high score. Returns the calls in order.
std::vector<DrawCall> restartCalls(RestartKind kind, const RestartInputs& in, const GameState& after, const datapack::Pack& pack, DisplayState& display);

}  // namespace pacman::core
