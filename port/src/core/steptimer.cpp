#include "steptimer.h"

#include <algorithm>

namespace pacman::core {

bool loadStepTables(const datapack::Pack& pack, StepTables& out) {
    const auto base = pack.find(0x7851, 16);
    const auto penalty = pack.find(0x7A8C, 16);
    const auto f3 = pack.find(0x7A9C, 16);
    if (base.empty() || penalty.empty() || f3.empty()) return false;
    std::copy(base.begin(), base.end(), out.base.begin());
    std::copy(penalty.begin(), penalty.end(), out.penalty.begin());
    std::copy(f3.begin(), f3.end(), out.f3Start.begin());
    return true;
}

namespace {
std::size_t levelClass(const GameState& s) { return std::min<std::size_t>(s.level(), 15); }  // $6D1E
}

bool StepTimer::tick(GameState& s, const StepTables& t) {
    std::uint8_t& counter = s.pacStepCounter();
    counter = static_cast<std::uint8_t>(counter - 1);  // DEC $59
    if (counter != 0) return false;
    // reload ($7874-$789B): the mode tick ($808C) and the cost of ghost states
    unsigned value = t.base[levelClass(s)];
    if (s.ghosts[1].b[8] != 0) ++value;   // $A0: ghost 2 frightened
    if (s.ghosts[3].b[8] != 0) ++value;   // $C8: ghost 4 frightened
    if (s.ghosts[0].b[9] != 0) --value;   // $8D: ghost 1 eyes
    if (s.ghosts[2].b[9] != 0) --value;   // $B5: ghost 3 eyes
    counter = static_cast<std::uint8_t>(value);
    if (f3_ != 0) {                       // $789D-$78A3
        --f3_;
        counter = static_cast<std::uint8_t>(counter + 1);
    }
    return true;
}

void StepTimer::itemEaten(GameState& s, const StepTables& t) {
    const std::size_t i = levelClass(s);
    s.pacStepCounter() = static_cast<std::uint8_t>(s.pacStepCounter() + t.penalty[i]);  // $7AB4
    f3_ = t.f3Start[i];                                                                   // $7ABB
}

}  // namespace pacman::core
