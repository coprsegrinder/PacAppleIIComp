#include "screen.h"

namespace pacman::core {

int pacmanFrame(const GameState& s) noexcept {
    const unsigned phase = s.zp(0x66) & 3u;
    const unsigned dir = s.pacDirection();
    if (phase == 0) return 0;
    if (phase == 2) return static_cast<int>(dir + 5);
    return static_cast<int>(dir + 1);
}

bool drawPlayfield(hires::Page& page, const GameState& s, const datapack::Pack& pack) {
    const auto walls = pack.find(0x5F69, hires::kWallTableBytes);
    const auto dot = pack.find(0x61A8, hires::kDotTableBytes);
    const auto tile = pack.find(0x61CA, hires::kTileTableBytes);
    if (walls.empty() || dot.empty() || tile.empty()) return false;
    page.clear();
    if (!hires::drawWalls(page, walls, true)) return false;
    hires::hLine(page, 0x87, 0x93, 0x4D, 2, true);   // the ghost-house door, two lines ($8A7D-$8A9B, JSR $5CF0 twice), step 2 as in the walls
    hires::hLine(page, 0x87, 0x93, 0x4E, 2, true);
    hires::drawDots(page, s.map, dot, tile);
    return true;
}

namespace {

// $70F5-$7123 with $7159: the number (24-bit BCD) is drawn digit by digit from the units, 6 pixels to the left each time; at least one digit.
bool drawNumber(hires::Page& page, std::uint32_t bcd, int x, int y, const datapack::Pack& pack) {
    const auto words = pack.find(0x717D, 20);   // ten tile-table addresses, one per digit
    if (words.empty()) return false;
    do {
        const unsigned d = bcd & 0xF;
        bcd >>= 4;
        const std::uint16_t table = static_cast<std::uint16_t>(words[d * 2] | (words[d * 2 + 1] << 8));
        const auto tile = pack.find(table, hires::kTileTableBytes);
        if (tile.empty()) return false;
        hires::tile(page, x, y, tile);
        x -= 6;
    } while (bcd != 0);
    return true;
}

}  // namespace

bool drawHud(hires::Page& page, const GameState& s, const datapack::Pack& pack, const std::array<std::uint8_t, 3>& high) {
    const auto label1 = pack.find(0x55F7, 49);
    const auto label2 = pack.find(0x5628, 49);
    const auto mark = pack.find(0x7201, hires::kTileTableBytes);
    const auto box = pack.find(0x6992, 12);
    const auto fruit = pack.find(0x6B6E, 36);
    const auto life = pack.find(0x9CB8, hires::kSevenCopies3x10);
    if (label1.empty() || label2.empty() || mark.empty() || box.empty() || fruit.empty() || life.empty()) return false;
    hires::blitOr(page, 0, 6, 7, 7, label1);                                   // $550A..: the player label
    hires::tile(page, 47, 6, mark);
    hires::hLine(page, 4, 0x35, 14, 2, true);                                  // $5CF6 under the player label: dotted line, x $04 to $35 (the oracle's values)
    hires::boxClear(page, box[2], box[3]);                                     // $696A with $50 = 1: the player's score box
    const std::uint32_t score = s.zp(0x52) | (s.zp(0x53) << 8) | (s.zp(0x54) << 16);
    if (!drawNumber(page, score, 40, 16, pack)) return false;
    hires::blitOr(page, 0, 84, 7, 7, label2);                                  // the high-score label
    hires::hLine(page, 4, 0x35, 91, 2, true);                                  // the same dotted line under it
    hires::boxClear(page, box[0], box[1]);                                     // $696A with $50 = 0
    const std::uint32_t hi = high[0] | (high[1] << 8) | (high[2] << 16);
    if (!drawNumber(page, hi, 40, 94, pack)) return false;
    hires::blitMergePalette(page, 33, 176, 3, 12, fruit);                      // the level icon (level 1)
    const int icons = (s.lives() & 0x7F) > 0 ? (s.lives() & 0x7F) - 1 : 0;     // $56CF: one icon less than the lives, 12 pixels apart
    for (int i = 0; i < icons; ++i) hires::drawImage(page, 9 + 12 * i, 178, life, true);
    return true;
}

bool drawPacman(hires::Page& page, const GameState& s, const datapack::Pack& pack, bool orMode) {
    const auto frames = pack.find(0x96E8, 18);           // nine words
    if (frames.empty()) return false;
    const std::size_t i = static_cast<std::size_t>(pacmanFrame(s)) * 2;
    const std::uint16_t base = static_cast<std::uint16_t>(frames[i] | (frames[i + 1] << 8));
    const auto image = pack.find(base, hires::kSevenCopies3x10);
    if (image.empty()) return false;
    hires::drawImage(page, s.pacX(), s.pacY(), image, orMode);
    return true;
}

}  // namespace pacman::core
