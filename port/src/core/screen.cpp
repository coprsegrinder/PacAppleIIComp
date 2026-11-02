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
