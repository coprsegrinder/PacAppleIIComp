#include "tickdraw.h"

#include "screen.h"

namespace pacman::core {

namespace {
constexpr int kMaskTable = 0x69CD;
std::uint16_t word(std::span<const std::uint8_t> t, std::size_t i) { return static_cast<std::uint16_t>(t[i * 2] | (t[i * 2 + 1] << 8)); }
}  // namespace

std::uint16_t ghostImageBase(const GameState& s, std::size_t g, std::uint16_t timer, const datapack::Pack& pack, int& flag1E) {
    const auto table = pack.find(0x9E5C, 26 * 2);
    const auto flags = pack.find(0x88E1, 5);
    flag1E = 0;
    if (table.empty() || flags.empty()) return 0;
    const GhostRecord& gr = s.ghosts[g & 3];
    std::size_t x = 2 * (g + 1);                     // $E0 = ghost number 1..4
    if (gr.b[8] != 0) {                              // $78: frightened
        const std::uint8_t high = static_cast<std::uint8_t>(timer >> 8), low = static_cast<std::uint8_t>(timer & 0xFF);
        if (high != 0 || low >= 0x80) x = 0;         // blue
        else x = (low & 0x10) ? 0 : 6;               // the last 128 ticks: flashing
    }
    if (gr.b[9] != 0) x = 10;                        // $79: eyes
    const std::size_t yIndex = x / 2;
    flag1E = yIndex < 5 ? flags[yIndex] : 0xA5;      // table $88E1 (the byte after it is an opcode, $A5, when X = 10)
    x += 12;                                         // $1D <> 0
    return word(table, x / 2);
}

std::vector<DrawCall> spriteCalls(const GameState& before, const GameState& after, const datapack::Pack& pack, bool pacmanActed, std::uint16_t timer) {
    std::vector<DrawCall> out;
    const auto pacFrames = pack.find(0x96E8, 18);
    if (pacFrames.empty()) return out;
    const bool moved = before.pacX() != after.pacX() || before.pacY() != after.pacY();
    if (moved || pacmanActed) {   // an acting Pac-Man that did not move was blocked and is redrawn where he stands
        out.push_back(DrawCall{3, {kMaskTable, before.pacX() - 4, before.pacY() - 4, 0, 0}});
        out.push_back(DrawCall{1, {word(pacFrames, static_cast<std::size_t>(pacmanFrame(after))), after.pacX(), after.pacY(), 1, 0x80}});
    }
    for (int g = 3; g >= 0; --g) {
        const GhostRecord& a = before.ghosts[static_cast<std::size_t>(g)];
        const GhostRecord& b = after.ghosts[static_cast<std::size_t>(g)];
        if (a.x() == b.x() && a.y() == b.y()) continue;
        int flag = 0;
        const std::uint16_t base = ghostImageBase(after, static_cast<std::size_t>(g), timer, pack, flag);
        out.push_back(DrawCall{3, {kMaskTable, a.x() - 4, a.y() - 4, 0, 0}});
        out.push_back(DrawCall{1, {base, b.x(), b.y(), 1, flag}});
    }
    return out;
}

}  // namespace pacman::core
