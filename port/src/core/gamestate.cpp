#include "gamestate.h"

#include <algorithm>
#include <cstring>

namespace pacman::core {

unsigned GameState::score() const {
    auto bcd = [](std::uint8_t b) { return static_cast<unsigned>((b >> 4) * 10 + (b & 15)); };
    return bcd(zp(0x52)) + 100u * bcd(zp(0x53)) + 10000u * bcd(zp(0x54));
}

std::array<std::uint8_t, kStateBytes> toOracleLayout(const GameState& s) {
    std::array<std::uint8_t, kStateBytes> o{};
    std::copy(s.zeroPage.begin(), s.zeroPage.end(), o.begin());
    o[30] = s.mode78;
    o[31] = s.mode79;
    o[32] = s.demo;
    o[33] = s.cornerPhase;
    for (std::size_t g = 0; g < 4; ++g) std::copy(s.ghosts[g].b.begin(), s.ghosts[g].b.end(), o.begin() + static_cast<std::ptrdiff_t>(kGhostRecordsAt + 20 * g));
    std::copy(s.map.begin(), s.map.end(), o.begin() + static_cast<std::ptrdiff_t>(kMapAt));
    return o;
}

GameState fromOracleLayout(std::span<const std::uint8_t> b) {
    GameState s;
    if (b.size() != kStateBytes) return s;
    std::copy_n(b.begin(), kZeroPageBytes, s.zeroPage.begin());
    s.mode78 = b[30];
    s.mode79 = b[31];
    s.demo = b[32];
    s.cornerPhase = b[33];
    for (std::size_t g = 0; g < 4; ++g) std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(kGhostRecordsAt + 20 * g), 20, s.ghosts[g].b.begin());
    std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(kMapAt), 896, s.map.begin());
    return s;
}

}  // namespace pacman::core
