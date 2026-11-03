// TASK-031-6/6b: the intermission cartoons as GAME LOGIC. A cartoon is produced as a list of drawing calls in the format of the hires replay ('CSCR' script), written from the
// disassembly ($81AE...), and executed with hires::replayScript. Nothing here comes from a recorded run.
//   cartoon 1 ($81AE): levels with (level mod 13) = 2   -- model equal to the oracle call for call (docs/CARTOONS.md section 3)
//   cartoon 2 ($83D0, after levels 5, 18, ...) and cartoon 3 ($8486, after levels 9, 22, ...): models equal to the oracle call for call (docs/CARTOONS.md sections 4, 5)
// Timing (INFERRED, refined in M8): the original's loop passes take equal time; the whole cartoon 1 takes 5,597,305 cycles (golden data). Every call is stamped with the
// loop pass in which it is drawn (the pause between the two phases counts as 800 passes: 8 x 100 short delays), and a pass lasts total / passes cycles.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../datapack/datapack.h"
#include "../hires/hires.h"
#include "../hires/replay.h"

namespace pacman::core {

inline constexpr std::int64_t kCartoonCycles[3] = {5'597'305, 4'347'216, 8'211'963};   // golden data: cartoons 1, 2, 3
inline constexpr std::int64_t kCartoon1Cycles = kCartoonCycles[0];

// Which cartoon follows the end of level `level` ($8155): 1, 2 or 3, or 0 for none. Level mod 13 = 2, 5 or 9 (docs/CARTOONS.md section 1).
int cartoonAfterLevel(unsigned level) noexcept;

struct CartoonPlan {
    std::vector<std::uint8_t> script;       // 'CSCR' script for hires::replayScript (recorded-hash field zero); empty if the pack lacks the frame tables $96E8 / $9E5C
    std::vector<std::uint32_t> passOfCall;  // loop pass in which each call is drawn (non-decreasing)
    std::uint32_t passes = 0;               // loop passes of the whole cartoon
    std::int64_t cycles = 0;                // its duration in cycles
};

// A cartoon (1, 2 or 3) as a plan; empty if `cartoon` is not 1-3 or the pack lacks the tables ($96E8, $9E5C, $847E, $854A).
CartoonPlan cartoonPlan(const datapack::Pack& pack, int cartoon, bool colour = true);
// Cartoon 1 as a plan.`colour` is the original's $1D flag (the real game: true).
CartoonPlan cartoon1Plan(const datapack::Pack& pack, bool colour = true);
std::vector<std::uint8_t> cartoon1Script(const datapack::Pack& pack, bool colour = true);   // plan.script only

// How many calls of the plan are drawn once `elapsedCycles` have passed since the cartoon started.
std::size_t cartoonCallsDue(const CartoonPlan& plan, std::int64_t elapsedCycles) noexcept;

// Plays a plan against the cycle clock of the live program.
class CartoonPlayer {
public:
    bool start(const datapack::Pack& pack, int cartoon);    // false for cartoon 0 and for cartoons not rebuilt yet (2, 3: 031-7)
    bool active() const noexcept { return active_; }
    void advance(std::int64_t cycles) noexcept;             // elapsed time; ends the cartoon when the plan's duration is over
    void render(const datapack::Pack& pack, hires::Page& page) const;   // the page as it looks now (replays the calls due from the page clear)
private:
    CartoonPlan plan_;
    std::int64_t elapsed_ = 0;
    bool active_ = false;
};

}  // namespace pacman::core
