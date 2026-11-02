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

bool newGameState(const datapack::Pack& pack, GameState& out) {
    const auto map = pack.find(0x65BA, 896);
    if (map.empty()) return false;
    GameState s;
    std::copy(map.begin(), map.end(), s.map.begin());
    // new-game routine $54D5: score 0, demo off, $F3 := 0 (not part of the logical state), level 1, $59 := 1, lives $83, items $F8, fruit counter $40
    s.zp(0x52) = s.zp(0x53) = s.zp(0x54) = 0;
    s.demo = 0;
    s.level() = 1;
    s.pacStepCounter() = 1;
    s.lives() = 0x83;
    s.dotsLeft() = 0xF8;
    s.fruitCounter() = 0x40;
    // oracle literals (see the header): start of life, get-ready pass, ghost house
    static constexpr std::array<std::uint8_t, kZeroPageBytes> kStart = {
        0x00, 0x00, 0x00, 0x01, 0xF8, 0x40, 0x83, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x8F, 0x03, 0x03, 0x2C, 0x2D, 0x00, 0x00, 0x02,
        0x00, 0x0D, 0x17, 0x02, 0x00, 0x00, 0x00};
    for (std::size_t i = 8; i < kStart.size(); ++i) s.zeroPage[i] = kStart[i];  // $5A-$6F; $52-$59 were set from the new-game routine above
    s.mode78 = 0x01;
    s.mode79 = 0x01;
    s.cornerPhase = 0x01;
    static constexpr std::array<std::array<std::uint8_t, 20>, 4> kGhosts = {{
        {0x8B, 0x47, 0x03, 0x17, 0x18, 0x00, 0x08, 0xFF, 0x00, 0x00, 0x00, 0x0D, 0x0B, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00},
        {0x7F, 0x5D, 0x01, 0x17, 0x18, 0x00, 0x09, 0xFF, 0x00, 0x00, 0x00, 0x0D, 0x0B, 0x02, 0x00, 0x02, 0x01, 0x01, 0x70, 0x00},
        {0x8B, 0x55, 0x00, 0x17, 0x18, 0x00, 0x0A, 0xFF, 0x00, 0x00, 0x00, 0x0D, 0x0B, 0x02, 0x00, 0x03, 0x01, 0x01, 0x30, 0x00},
        {0x99, 0x5D, 0x01, 0x17, 0x18, 0x00, 0x0B, 0xFF, 0x00, 0x00, 0x00, 0x0D, 0x0B, 0x02, 0x00, 0x04, 0x01, 0x01, 0x90, 0x00}}};
    for (std::size_t g = 0; g < 4; ++g) s.ghosts[g].b = kGhosts[g];
    out = s;
    return true;
}

}  // namespace pacman::core
