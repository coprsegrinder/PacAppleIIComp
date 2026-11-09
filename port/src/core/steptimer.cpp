#include "steptimer.h"

#include "cycles.h"

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

unsigned StepTimer::calmReloadValue(const GameState& s, const StepTables& t) noexcept { return t.base[levelClass(s)]; }

unsigned StepTimer::reloadValue(const GameState& s, const StepTables& t) noexcept {
    unsigned value = t.base[levelClass(s)];
    if (s.ghosts[1].b[8] != 0) ++value;   // $A0: ghost 2 frightened
    if (s.ghosts[3].b[8] != 0) ++value;   // $C8: ghost 4 frightened
    if (s.ghosts[0].b[9] != 0) --value;   // $8D: ghost 1 eyes
    if (s.ghosts[2].b[9] != 0) --value;   // $B5: ghost 3 eyes
    return value;
}

bool StepTimer::tick(GameState& s, const StepTables& t) {
    std::uint8_t& counter = s.pacStepCounter();
    counter = static_cast<std::uint8_t>(counter - 1);  // DEC $59
    if (counter != 0) return false;
    // reload ($7874-$789B): the mode tick ($808C) and the cost of ghost states
    // The cycles: instead of the idle path (BEQ not taken 2 + RTS 6) the branch is taken (3); $786D asks the joystick flag (LDA $F5, BEQ taken in keyboard mode, the paddle poll is input.cpp's);
    // JSR $808C (the routine itself: modeTimerStep), JSR $6D1E (+ body), TAX, LDA $7851,X, CLC, LDX #8 and the four tests of the ghosts' flags.
    cy(-8 + 3 + 3 + 3);                                                // the idle path is replaced; $786D: LDA $F5, BEQ taken
    cy(6 + 6 + (s.level() < 15 ? 14 : 15) + 2 + 4 + 2 + 2);            // JSR $808C, JSR $6D1E (+ body), TAX, LDA $7851,X, CLC, LDX #8
    const bool f[4] = {s.ghosts[1].b[8] != 0, s.ghosts[3].b[8] != 0, s.ghosts[0].b[9] != 0, s.ghosts[2].b[9] != 0};
    cy(4 + (f[0] ? 2 + 2 : 3) + 4 + (f[1] ? 2 + 2 : 3) + 2 + 2 + 4 + (f[2] ? 2 + 2 : 3) + 4 + (f[3] ? 2 + 2 : 3));   // LDY $98,X ... SBC #1
    cy(3 + 3);                                                         // STA $59, LDA $F3
    const unsigned value = reloadValue(s, t);
    counter = static_cast<std::uint8_t>(value);
    if (f3_ != 0) {                       // $789D-$78A3
        cy(2 + 5 + 5);                                                 // BEQ not taken, DEC $F3, INC $59
        --f3_;
        counter = static_cast<std::uint8_t>(counter + 1);
    } else {
        cy(3);                                                         // BEQ taken
    }
    return true;
}

bool StepTimer::nearGhostDelays(const GameState& s, const GhostRecord& g) noexcept {
    const unsigned px = s.pacX(), py = s.pacY(), gx = g.b[0], gy = g.b[1], dir = g.b[2];
    unsigned distance;
    if (px == gx) distance = py > gy ? py - gy : gy - py;           // same column: $8B3A
    else if (py == gy) distance = px > gx ? px - gx : gx - px;      // same row: $8B6F
    else return false;
    if (distance >= 10) return false;                               // $8B45
    if (s.level() < 5) return false;                                // $8B4A
    // $8B50-$8B5E: a ghost moving horizontally (direction bit 1 set) looks at Pac-Man's Y (X if Y is 0), otherwise at Pac-Man's X
    const unsigned looked = (dir & 2) != 0 ? (py != 0 ? py : px) : px;
    return (looked & 2) == 0;
}

void StepTimer::applyNearGhostDelay(GameState& s, const GhostRecord& g) noexcept {
    if (nearGhostDelays(s, g)) s.pacStepCounter() = static_cast<std::uint8_t>(s.pacStepCounter() + 1);  // $8B63-$8B65
}

void StepTimer::itemEaten(GameState& s, const StepTables& t) {
    const std::size_t i = levelClass(s);
    // $7AAC-$7AB6: CLC, JSR $6D1E (which ends with CMP #$0F, so the carry is SET for every level >= 15 and ADC adds it), ADC $59.
    // This quirk was found on the oracle at levels 15, 16 and 21: eating at those levels delays Pac-Man by 1 although the table entry is 0.
    const unsigned carry = s.level() >= 15 ? 1u : 0u;
    s.pacStepCounter() = static_cast<std::uint8_t>(s.pacStepCounter() + t.penalty[i] + carry);  // $7AB4
    f3_ = t.f3Start[i];                                                                   // $7ABB
}

}  // namespace pacman::core
