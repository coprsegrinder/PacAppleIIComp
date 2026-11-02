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

namespace {
// The state the ghost is drawn with: the frightened flag is already set by Pac-Man's phase (energizer eaten earlier in the tick) but the EYES flag is only set
// after the ghost has been drawn (the contact test follows the draw), so the eyes flag of the start of the tick counts.
GameState drawnState(const GameState& before, const GameState& after, std::size_t g) {
    GameState s = after;
    s.ghosts[g].b[9] = before.ghosts[g].b[9];
    return s;
}
}  // namespace

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
        const std::uint16_t base = ghostImageBase(drawnState(before, after, static_cast<std::size_t>(g)), static_cast<std::size_t>(g), timer, pack, flag);
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

std::vector<DrawCall> tickCalls(const GameState& before, const GameState& after, const datapack::Pack& pack, bool pacmanActed, DisplayState& display,
                                const TickExtras& exBefore, const TickExtras& exAfter) {
    std::vector<DrawCall> out;
    const std::uint16_t timer = exAfter.frightenedTimer();
    const auto sprites = spriteCalls(before, after, pack, pacmanActed, timer);
    // Pac-Man's calls come first (the first two, if any), then the score display, then the ghosts' calls with their dot redraws
    const bool pacDrawn = (before.pacX() != after.pacX() || before.pacY() != after.pacY() || pacmanActed);
    std::size_t split = pacDrawn ? 2 : 0;
    out.insert(out.end(), sprites.begin(), sprites.begin() + static_cast<std::ptrdiff_t>(split));
    if (pacDrawn) display.lastFlag1E = 0x80;   // Pac-Man's draw sets $1E = $80 ($79C2)
    const std::uint32_t scoreBefore = bcd24(before), scoreAfter = bcd24(after);
    if (scoreAfter != scoreBefore) {
        const std::uint32_t hi = display.high[0] | (display.high[1] << 8) | (display.high[2] << 16);
        if (scoreAfter >= hi) {                       // $7091: the high score follows the score
            display.high = {after.zp(0x52), after.zp(0x53), after.zp(0x54)};
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
        int flag = 0;
        ghostImageBase(drawnState(before, after, static_cast<std::size_t>(g)), static_cast<std::size_t>(g), timer, pack, flag);
        const int flagBefore = display.lastFlag1E;   // what the previous image draw left in $1E: the door lines are drawn with it
        out.push_back(sprites[k++]);
        out.push_back(sprites[k++]);
        display.lastFlag1E = flag;   // the ghost's own draw sets $1E from the table
        // $8A19: a ghost that leaves the door cell redraws the ghost-house door (two dotted lines); not logged by the call collector (type 8: hline)
        if (a.x() == 0x8B && a.y() == 0x47 && a.b[17] != 0) {   // a ghost in the house exit script ($81 <> 0) LEAVES the door cell (old position: $70 is tested before the step)
            out.push_back(DrawCall{8, {0x87, 0x93, 0x4D, 2, flagBefore}});
            out.push_back(DrawCall{8, {0x87, 0x93, 0x4E, 2, flagBefore}});
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
    // $570C at the end of the tick: the 1UP blink, then the energizer blink
    if (exBefore.e6 != exAfter.e6) {
        if (exAfter.e6 == 1) {                                                   // $58FA -> $556E: label and mark
            out.push_back(DrawCall{2, {0x55F7, 0, 6, 7, 7}});
            out.push_back(DrawCall{6, {0x7201, 47, 6, 0, 0}});
        } else {                                                                 // $58E7 -> $6978: box clear at (column, row - $0F)
            out.push_back(DrawCall{10, {0, 0, 0, 0, 0}});
        }
    }
    if (before.zp(0x5F) != after.zp(0x5F)) {
        struct Spot { std::size_t cell; int tileX, tileY, maskX, maskY; };
        static constexpr Spot spots[4] = {{1 * 32 + 3, 64, 22, 63, 21}, {1 * 32 + 23, 64, 142, 63, 141}, {26 * 32 + 3, 214, 22, 214, 21}, {26 * 32 + 23, 214, 142, 214, 141}};
        for (const Spot& sp : spots) {
            if (after.map[sp.cell] == 0) continue;
            if (after.zp(0x5F) == 1) out.push_back(DrawCall{6, {0x61CA, sp.tileX, sp.tileY, 0, 0}});
            else out.push_back(DrawCall{9, {sp.maskX, sp.maskY, 0, 0, 0}});
        }
    }
    return out;
}

bool executeCall(hires::Page& page, const DrawCall& c, const datapack::Pack& pack) {
    switch (c.type) {
        case 3: {
            const auto mask = pack.find(static_cast<std::uint16_t>(c.p[0]), hires::kMaskTableBytes);
            if (mask.empty()) return false;
            hires::maskedClear(page, c.p[1], c.p[2], mask);
            return true;
        }
        case 1: {
            const auto image = pack.find(static_cast<std::uint16_t>(c.p[0]), hires::kSevenCopies3x10);
            if (image.empty()) return false;
            hires::drawImage(page, c.p[1], c.p[2], image, c.p[3] != 0 && c.p[4] != 0);
            return true;
        }
        case 2: {
            const auto data = pack.find(static_cast<std::uint16_t>(c.p[0]), static_cast<std::size_t>(c.p[3] * c.p[4]));
            if (data.empty()) return false;
            hires::blitOr(page, c.p[1], c.p[2], c.p[3], c.p[4], data);
            return true;
        }
        case 4:
        case 10: {                                                              // 10: the 1UP box: column 0, row 1 ($6992 entry 1 minus $0F)
            const auto box = pack.find(0x6992, 12);
            if (box.empty()) return false;
            if (c.type == 10) hires::boxClear(page, box[2], static_cast<std::uint8_t>(box[3] - 0x0F));
            else hires::boxClear(page, box[static_cast<std::size_t>(c.p[0]) * 2], box[static_cast<std::size_t>(c.p[0]) * 2 + 1]);
            return true;
        }
        case 5: {
            const auto dot = pack.find(0x61A8, hires::kDotTableBytes);
            if (dot.empty()) return false;
            hires::smallDot(page, c.p[0], c.p[1], dot);
            return true;
        }
        case 6: {
            const auto tile = pack.find(static_cast<std::uint16_t>(c.p[0]), hires::kTileTableBytes);
            if (tile.empty()) return false;
            hires::tile(page, c.p[1], c.p[2], tile);
            return true;
        }
        case 8: {                                                               // $5CF0: every touched byte becomes (byte & $7F) | $1E
            hires::hLine(page, c.p[0], c.p[1], c.p[2], c.p[3], (c.p[4] & 0x80) != 0);
            if ((c.p[4] & 0x7F) != 0)
                for (int col = c.p[0] / 7; col <= c.p[1] / 7; ++col) page.orInto(hires::rowOffset(c.p[2]) + static_cast<std::size_t>(col), static_cast<std::uint8_t>(c.p[4] & 0x7F));
            return true;
        }
        case 9: {                                                               // $7819: 8 rows, two bytes each: AND the mask pair of the shift, then set the palette bit
            const auto masks = pack.find(0x7843, 14);
            if (masks.empty()) return false;
            const int column = c.p[0] / 7, shift = c.p[0] % 7;
            for (int r = 0; r < 8; ++r) {
                const std::size_t off = hires::rowOffset(c.p[1] + r) + static_cast<std::size_t>(column);
                page.set(off, static_cast<std::uint8_t>((page.byte(off) & masks[static_cast<std::size_t>(shift) * 2]) | 0x80));
                page.set(off + 1, static_cast<std::uint8_t>((page.byte(off + 1) & masks[static_cast<std::size_t>(shift) * 2 + 1]) | 0x80));
            }
            return true;
        }
        default: return false;
    }
}

}  // namespace pacman::core
