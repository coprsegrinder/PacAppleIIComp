#include "tickdraw.h"

#include "screen.h"
#include "../hires/hires.h"

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

namespace {

void digitCalls(std::vector<DrawCall>& out, std::uint32_t bcd, int x, int y, const datapack::Pack& pack) {
    const auto words = pack.find(0x717D, 20);
    if (words.empty()) return;
    do {
        const unsigned d = bcd & 0xF;
        bcd >>= 4;
        out.push_back(DrawCall{6, {words[d * 2] | (words[d * 2 + 1] << 8), x, y, 0, 0}});
        x -= 6;
    } while (bcd != 0);
}

std::uint32_t bcd24(const GameState& s) { return s.zp(0x52) | (s.zp(0x53) << 8) | (s.zp(0x54) << 16); }

}  // namespace

std::vector<DrawCall> tickCalls(const GameState& before, const GameState& after, const datapack::Pack& pack, bool pacmanActed, std::array<std::uint8_t, 3>& high,
                                std::uint16_t timer) {
    std::vector<DrawCall> out;
    const auto sprites = spriteCalls(before, after, pack, pacmanActed, timer);
    // Pac-Man's calls come first (the first two, if any), then the score display, then the ghosts' calls with their dot redraws
    const bool pacDrawn = (before.pacX() != after.pacX() || before.pacY() != after.pacY() || pacmanActed);
    std::size_t split = pacDrawn ? 2 : 0;
    out.insert(out.end(), sprites.begin(), sprites.begin() + static_cast<std::ptrdiff_t>(split));
    const std::uint32_t scoreBefore = bcd24(before), scoreAfter = bcd24(after);
    if (scoreAfter != scoreBefore) {
        const std::uint32_t hi = high[0] | (high[1] << 8) | (high[2] << 16);
        if (scoreAfter >= hi) {                       // $7091: the high score follows the score
            high = {after.zp(0x52), after.zp(0x53), after.zp(0x54)};
            out.push_back(DrawCall{4, {0, 0, 0, 0, 0}});
            digitCalls(out, scoreAfter, 40, 94, pack);
        }
        out.push_back(DrawCall{4, {1, 0, 0, 0, 0}});
        digitCalls(out, scoreAfter, 40, 16, pack);
    }
    const auto dot = pack.find(0x61A8, hires::kDotTableBytes);
    std::size_t k = split;
    for (int g = 3; g >= 0; --g) {
        const GhostRecord& a = before.ghosts[static_cast<std::size_t>(g)];
        const GhostRecord& b = after.ghosts[static_cast<std::size_t>(g)];
        if (a.x() == b.x() && a.y() == b.y()) continue;
        out.push_back(sprites[k++]);
        out.push_back(sprites[k++]);
        // $8A19: a ghost that has reached the door cell redraws the ghost-house door (two dotted lines); not logged by the call collector (type 8: hline)
        int flag = 0;
        ghostImageBase(after, static_cast<std::size_t>(g), timer, pack, flag);
        if (a.x() == 0x8B && a.y() == 0x47) {   // the ghost LEAVES the door cell (its old position, as $70 is tested before the step)
            out.push_back(DrawCall{8, {0x87, 0x93, 0x4D, 2, flag}});
            out.push_back(DrawCall{8, {0x87, 0x93, 0x4E, 2, flag}});
        }
        // the cell the ghost has just left ($7B/$7C = record bytes 11/12, $7D/$7E = bytes 13/14)
        const unsigned dir = b.b[2];
        int col = b.b[11], row = b.b[12];
        bool left = false;
        switch (dir) {
            case 2: left = b.b[13] == 0; col -= 1; break;
            case 3: left = b.b[13] == 0; col += 1; break;
            case 0: left = b.b[14] == 0; row -= 1; break;
            case 1: left = b.b[14] == 0; row += 1; break;
            default: break;
        }
        if (left && col >= 0 && row >= 0 && static_cast<std::size_t>(col) * 32 + static_cast<std::size_t>(row) < after.map.size() && !dot.empty() &&
            (after.map[static_cast<std::size_t>(col) * 32 + static_cast<std::size_t>(row)] & 1) != 0)
            out.push_back(DrawCall{5, {col * 6 + 0x3B, row * 6 + 5, 0, 0, 0}});   // type 5: p0 = x, p1 = y (as logged)
    }
    return out;
}

}  // namespace pacman::core
