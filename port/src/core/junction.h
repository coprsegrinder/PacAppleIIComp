// Pac-Man at the junction graph (TASK-029d2): arrival at a junction, acceptance of the requested direction, the next junction, and the two tunnel
// ends. Rebuilt from $78A5-$7961 (and $75F1 for the record addresses), checked against the oracle recordings. The step itself (position, animation,
// cell counters) is TASK-029d3; the tunnel transit ($7B2F) too.
//
// The graph: 64 junctions, 8 bytes each at $760A (x, y, then the neighbour junction for directions 0 (down), 1 (up), 2 (right), 3 (left); a byte
// with bit 7 set means "no exit"). Pac-Man shares it with the ghosts (docs/MOVEMENT.md).
#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "../datapack/datapack.h"
#include "gamestate.h"

namespace pacman::core {

struct Junction {
    std::uint8_t x = 0, y = 0;
    std::array<std::uint8_t, 4> next{};  // by direction; bit 7 set = no exit
};

class JunctionGraph {
public:
    static constexpr std::size_t kJunctions = 64;
    bool load(const datapack::Pack& pack);                  // false if the pack lacks the 512 bytes at $760A
    void set(std::uint8_t index, const Junction& j) noexcept { junctions_[index & 63] = j; }  // for tests
    const Junction& at(std::uint8_t index) const noexcept { return junctions_[index & 63]; }  // the original would read past the table for >= 64 (never happens)
private:
    std::array<Junction, kJunctions> junctions_{};
};

enum class JunctionOutcome {
    Move,     // go on with the step (029d3)
    Blocked,  // the way is blocked: the original only redraws Pac-Man standing ($66 := 2) and ends its part of the tick
    Tunnel,   // in a tunnel: the original continues at $7B2F (tunnel transit)
};

// One pass of $78A5-$7961 for a tick in which Pac-Man acts. In the demo ($E8 <> 0) the requested direction comes from the demo stream:
// pass the next byte of that stream (the original advances $1B/$1C and loads it into $63).
JunctionOutcome junctionStep(GameState& state, const JunctionGraph& graph, std::optional<std::uint8_t> demoStreamByte = std::nullopt);

}  // namespace pacman::core
