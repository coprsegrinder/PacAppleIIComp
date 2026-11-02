// The game core's state (TASK-029b, docs/M4_PLAN.md): exactly the "logical state" of the golden traces (docs/GOLDEN_TRACES.md), with readable names where
// the analysis documents give a meaning and neutral names (zp(0x5A)) where they do not yet. Nothing else is state: everything else in the original's
// memory is code, tables (from the data pack) or scratch.
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "../datapack/datapack.h"
#include "ticktrace.h"

namespace pacman::core {

struct GhostRecord {
    std::array<std::uint8_t, 20> b{};  // $84+20*i .. ; the meaning of most bytes is in docs/GHOST_ANALYSIS.md (x = b[0], y = b[1], ...); ghost logic is M5
    std::uint8_t x() const noexcept { return b[0]; }
    std::uint8_t y() const noexcept { return b[1]; }
};

struct GameState {
    std::array<std::uint8_t, kZeroPageBytes> zeroPage{};  // $52-$6F
    std::uint8_t mode78 = 0;      // $78 ghost frightened flag of the ghost in the working slot (docs/GHOST_MODES.md)
    std::uint8_t mode79 = 0;      // $79 eyes flag of the working slot
    std::uint8_t demo = 0;        // $E8: 1 = attract demo running, 0 = a real game
    std::uint8_t cornerPhase = 0; // $E9: corner / chase phase (GHOST_MODES.md)
    std::array<GhostRecord, 4> ghosts{};                  // $84-$D3
    std::array<std::uint8_t, 896> map{};                  // $623A-$65B9: 0 = empty/eaten, 1 = dot, 2 = energizer (28 columns x 32, column-major)

    // access by the original's zero-page address ($52..$6F)
    std::uint8_t& zp(std::uint16_t address) { return zeroPage[address - 0x52]; }
    std::uint8_t zp(std::uint16_t address) const { return zeroPage[address - 0x52]; }

    // named fields (meaning: Verified/Inferred as in docs/PACMAN_RULES.md, MOVEMENT.md, LEVEL_TABLES.md)
    std::uint8_t& level() { return zp(0x55); }
    std::uint8_t& dotsLeft() { return zp(0x56); }     // items (dots + energizers) left; 248 = $F8 at the start
    std::uint8_t& fruitCounter() { return zp(0x57); } // 64 at the start, -1 per item eaten; 0 => fruit (LEVEL_TABLES.md)
    std::uint8_t& lives() { return zp(0x58); }        // $83 at the start
    std::uint8_t& pacStepCounter() { return zp(0x59); }
    std::uint8_t& pacX() { return zp(0x60); }
    std::uint8_t& pacY() { return zp(0x61); }
    std::uint8_t& pacDirection() { return zp(0x62); }
    std::uint8_t& requestedDirection() { return zp(0x63); }
    std::uint8_t& pacJunction() { return zp(0x64); }
    std::uint8_t& pacNextJunction() { return zp(0x65); }
    std::uint8_t level() const { return zp(0x55); }
    std::uint8_t dotsLeft() const { return zp(0x56); }
    std::uint8_t lives() const { return zp(0x58); }
    std::uint8_t pacX() const { return zp(0x60); }
    std::uint8_t pacY() const { return zp(0x61); }
    unsigned score() const;   // the three BCD bytes $52-$54 (low, mid, high) as a number
};

// The 1,010-byte layout of the golden traces (see ticktrace.h) and back.
std::array<std::uint8_t, kStateBytes> toOracleLayout(const GameState& s);
GameState fromOracleLayout(std::span<const std::uint8_t> bytes);   // bytes must be kStateBytes long, otherwise a zero state

// The state at the first tick of a new one-player game (tick 0 of all four golden scenarios, which are identical).
// Sources, honestly: the MAP is the pristine copy at $65BA of the data pack (Verified: equals the oracle); the NEW-GAME constants (score 0, demo off,
// level 1, lives $83, items $F8, fruit counter $40) are read from the new-game routine $54D5 (code read, Verified by the equal state); the REST
// (Pac-Man start position and direction, junction numbers, the four ghost records, timers) are the oracle's values taken as literals, because the
// routines that set them (start of life $53BE.., ghost house reset $6D30, the get-ready pass) are reconstructed in later pieces (029d, 029f, M5).
// Returns false and leaves the state unchanged if the pack lacks the map.
bool newGameState(const datapack::Pack& pack, GameState& out);

}  // namespace pacman::core
