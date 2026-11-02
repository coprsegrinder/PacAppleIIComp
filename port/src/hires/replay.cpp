#include "replay.h"

#include <cstring>

namespace pacman::hires {

bool replayScript(const datapack::Pack& pack, std::span<const std::uint8_t> f, Page& page, ReplayStats& stats) {
    if (f.size() < 40 || std::memcmp(f.data(), "CSCR", 4) != 0) return false;
    std::uint32_t count = 0;
    std::memcpy(&count, f.data() + 4, 4);
    const std::size_t rec = 1 + 8 * 4;
    if (f.size() != 40 + count * rec) return false;
    stats.calls = count;
    std::memcpy(stats.recordedSha256.data(), f.data() + 8, 32);
    for (std::uint32_t i = 0; i < count && 40 + (i + 1) * rec <= f.size(); ++i) {
        const std::uint8_t* r = f.data() + 40 + i * rec;
        const int type = r[0];
        std::array<std::int32_t, 8> p{};
        std::memcpy(p.data(), r + 1, 32);
        const auto addr = [&](std::int32_t a, std::size_t n) { return pack.find(static_cast<std::uint16_t>(a), n); };
        bool ran = true;
        switch (type) {
            case 10: page.clear(); break;
            case 1: {
                const auto d = addr(p[0], kSevenCopies3x10);
                if (d.empty()) { ran = false; break; }
                drawImage(page, p[1], p[2], d, p[3] != 0 && p[4] != 0);
                break;
            }
            case 2: {
                const auto d = addr(p[0], static_cast<std::size_t>(p[3]) * static_cast<std::size_t>(p[4]));
                if (d.empty()) { ran = false; break; }
                blitOr(page, p[1], p[2], p[3], p[4], d);
                break;
            }
            case 3: {
                const auto m = addr(p[0], kMaskTableBytes);
                if (m.empty()) { ran = false; break; }
                maskedClear(page, p[1], p[2], m);
                break;
            }
            case 4: {
                const auto t = addr(0x6992, 12);
                if (t.empty()) { ran = false; break; }
                boxClear(page, t[static_cast<std::size_t>(p[0]) * 2], t[static_cast<std::size_t>(p[0]) * 2 + 1]);
                break;
            }
            case 5: {
                const auto t = addr(0x61A8, kDotTableBytes);
                if (t.empty()) { ran = false; break; }
                smallDot(page, p[0], p[1], t);
                break;
            }
            case 6: {
                const auto t = addr(p[0], kTileTableBytes);
                if (t.empty()) { ran = false; break; }
                tile(page, p[1], p[2], t);
                break;
            }
            case 7: {
                const auto d = addr(p[0], 36);
                if (d.empty()) { ran = false; break; }
                blitMergePalette(page, p[1], p[2], 3, 12, d);
                break;
            }
            case 8: {
                const auto d = addr(p[0], kBigSpriteBytes);
                if (d.empty()) { ran = false; break; }
                bigSprite(page, p[1], p[2], d);
                break;
            }
            case 9: {
                const auto d = addr(p[0], kImage3x10);
                if (d.empty()) { ran = false; break; }
                shiftedImage(page, p[1], p[2], d, p[3] != 0 && p[4] != 0);
                break;
            }
            default: ran = false;
        }
        if (ran) ++stats.used[static_cast<std::size_t>(type)];
        else {
            ++stats.unusable;
            stats.lastUnusable = static_cast<int>(i);
        }
        if (type == 10) stats.lastClear = static_cast<int>(i);
    }

    return true;
}

}  // namespace pacman::hires
