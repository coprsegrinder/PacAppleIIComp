// Pac-Man at the junction graph (TASK-029d2): arrival at a junction, acceptance of the requested direction, the next junction, and the two tunnel
// ends. Rebuilt from $78A5-$7961 (and $75F1 for the record addresses), checked against the oracle recordings. The step itself (position, animation,
// cell counters) is TASK-029d3; the tunnel transit ($7B2F) too.
//
// The graph: 64 junctions, 8 bytes each at $760A (x, y, then the neighbour junction for directions 0 (down), 1 (up), 2 (right), 3 (left); a byte
// with bit 7 set means "no exit"). Pac-Man shares it with the ghosts (docs/MOVEMENT.md).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
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

// The attract demo's move stream (TASK-029d2c): the demo is a list of direction bytes in the program ($47DA onward, 71 bytes, 0..3). The original keeps a
// pointer in $1B/$1C (set to $47DA when the demo starts, $47C9-$47CF), and at every junction pass of an acting demo tick it increments the pointer FIRST
// and then loads the byte it points to into $63 ($78DB-$78E5). The bytes come from the data pack.
class DemoStream {
public:
    static constexpr std::uint16_t kStart = 0x47DA;
    explicit DemoStream(const datapack::Pack& pack, std::uint16_t start = kStart) : pack_(&pack), pointer_(start) {}
    std::uint16_t pointer() const noexcept { return pointer_; }
    // Advances the pointer and returns the byte there; 0xFF (no wish) if the pointer has left the data in the pack.
    std::uint8_t next() {
        ++pointer_;
        const auto b = pack_->find(pointer_, 1);
        return b.empty() ? std::uint8_t{0xFF} : b[0];
    }
private:
    const datapack::Pack* pack_;
    std::uint16_t pointer_;
};

using StreamByteSource = std::function<std::uint8_t()>;

// One pass of $78A5-$7961 for a tick in which Pac-Man acts. In the demo ($E8 <> 0) the requested direction comes from the demo stream: `stream` is called
// exactly when the original advances its pointer (only on the paths that reach $78D7), and its result replaces $63.
JunctionOutcome junctionStep(GameState& state, const JunctionGraph& graph, const StreamByteSource& stream);
// Convenience for tests: a fixed byte instead of a stream.
JunctionOutcome junctionStep(GameState& state, const JunctionGraph& graph, std::optional<std::uint8_t> demoStreamByte = std::nullopt);

}  // namespace pacman::core
