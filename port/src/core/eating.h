// Eating a dot or an energizer (TASK-029d4), rebuilt from $7A5F-$7B2E: the item test at the map cell, the delay of Pac-Man (StepTimer::itemEaten), the
// score in BCD, the item counters and the cleared map cell. Checked against the oracle recordings.
//
// NOT rebuilt here (other pieces): what an ENERGIZER does to the ghosts ($6D60: frightened flags, timers, score step $68, $E4 ...: ghost logic, M5),
// the sounds ($7BDC, $7C45) and the drawing/high-score update ($7091, $5743), the level end ($6CF4 when the last item is gone: levels, M6),
// the sounds and drawing. The extra life at 10,000 points is checked here (it is inside the score routine $7091, which $7B12 calls): see lives.h.
#pragma once

#include <cstdint>

#include "gamestate.h"
#include "pacstep.h"
#include "steptimer.h"

namespace pacman::core {

struct EatResult {
    bool ate = false;            // there was an item in the cell
    bool energizer = false;      // it was an energizer (the caller must apply the ghost effects)
    bool levelComplete = false;  // the last item of the maze was eaten ($56 reached 0, real game only)
    bool outsideMap = false;     // the cell counters pointed past the 896-byte map (only after the position was poked without the counters): the original then reads and
                                 // clears bytes of the master copy that follows the map ($65BA...), which is not part of the logical state; nothing is done here
};

// BCD addition on the three score bytes $52 (low), $53, $54 (high), as the 6502's decimal mode does it.
// The tick in which an energizer is eaten, beyond a dot's: JSR $7C45 and its body (the sound itself is the freeze of sound.cpp), the saved pointer, the decimal addition of 50 points, JSR $6D60 (the
// effect: ghostmode.cpp), CLC, BCC taken (87) and the loop over the four energizer places of $5743 (110; its calls are the glue of the type 11 clears)
inline constexpr int kEnergizerTickCycles = 87 + 110 - 7;   // (the 7 from the oracle: 7 of the 197 counted from the listing are not run or are counted elsewhere)

// The tick in which a ghost is eaten (game): besides the sound's freeze (sound.cpp) it spends six passes of $805D (the loop over the four ghosts that lets the eyes walk on: 3,378 cycles each
// when no ghost is eyes) and the set-up of $7DB8, 20,806 to 20,809 cycles on the oracle, depending on the path of Pac-Man's draw prologue $79A4 (below); the attract game waits in a
// delay loop instead (gamedriver.h: kDemoGhostEatenTickCycles)
// (This is the sum without Pac-Man's draw prologue $79A4-$79CA, which the freeze routine ends with (JMP $79A4) and whose path depends on Pac-Man's mouth phase $66 and direction $62:
// 20,806 - 47, the 47 being the path of phase 1 that the first two recordings happened to take; the recording T00 takes phase 3 (48) and cost one cycle more.)
inline constexpr int kGhostEatenTickExtraCycles = 20'759;

// $79A4-$79CA: LDA $66, AND #$0F, STA $66, AND #3 and the choice of Pac-Man's image (frame table $96E8 indexed by 2 * ($62 + 1 or + 5)), up to the JMP $6A20 (its own cost is the draw call's).
// Phase 0: 33; phase 1: 47; phase 2: 50; phase 3: 48 (read on the oracle: T00 tick 1204 and the first ghost-eaten ticks of D); an indexed read of the frame table that carries over a page costs one more.
inline int pacmanDrawPrologueCycles(unsigned z66, unsigned z62) {
    const unsigned phase = z66 & 3;
    int c = 3 + 2 + 3 + 2;                             // LDA $66, AND #$0F, STA $66, AND #3
    unsigned frame = 0;
    if (phase == 0) {
        c += 3;                                        // BEQ taken
    } else {
        c += 2 + 2;                                    // BEQ, CMP #3
        if (phase == 3) c += 2 + 2;                    // BNE not taken, LDA #1
        else c += 3;                                   // BNE taken
        const unsigned seen = phase == 3 ? 1u : phase;
        c += 2 + 3;                                    // ROR A, LDA $62
        if ((seen & 1) != 0) {
            c += 3;                                    // BCS taken
            frame = z62 + 1;
        } else {
            c += 2 + 2 + 2;                            // BCS, ADC #4, SEC
            frame = z62 + 5;
        }
        c += 2;                                        // ADC #0
    }
    const unsigned x = (frame * 2) & 255u;
    c += 2 + 2 + 2 + 3;                                // ASL A, TAX, LDA #$80, STA $1E
    c += 4 + (0xE8 + x > 0xFF ? 1 : 0) + 4 + (0xE9 + x > 0xFF ? 1 : 0) + 3;   // LDA $96E8,X, LDY $96E9,X, JMP $6A20
    return c;
}

void addToScore(GameState& state, std::uint8_t bcdLow);

// The item test of $7A5F for the cell Pac-Man's step reported. Cell value 0: nothing happens. Otherwise: StepTimer::itemEaten, then
//   real game: dot (value 1): score + 10; energizer (any other non-zero value): score + 50; cell := 0; $57 -= 1; $56 -= 1.
//   attract demo ($E8 <> 0): cell := 0 and $56 -= 1 only ($7B1F-$7B2E: no score, no fruit counter).
EatResult eatAt(GameState& state, const ItemTest& cell, StepTimer& timer, const StepTables& tables, int player = 1);

}  // namespace pacman::core
