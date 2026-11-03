#include "cartoon.h"

#include <cstring>

namespace pacman::core {

int cartoonAfterLevel(unsigned level) noexcept {
    // $8155 subtracts 13 from levels above 9 until the value is 2, 5 or 9 or below; checked on the original for every level 1-255 (docs/CARTOONS.md section 1):
    // the result is the same as level mod 13 = 2, 5, 9 (no cartoon after levels 10-13 or any other).
    const unsigned m = level % 13;
    return m == 2 ? 1 : m == 5 ? 2 : m == 9 ? 3 : 0;
}

namespace {

struct Call {
    std::uint8_t type;
    std::int32_t p[8];
};

std::uint16_t word(std::span<const std::uint8_t> t, std::size_t index) { return static_cast<std::uint16_t>(t[2 * index] | t[2 * index + 1] << 8); }

}  // namespace

std::vector<std::uint8_t> cartoon1Script(const datapack::Pack& pack, bool colour) {
    const auto pacFrames = pack.find(0x96E8, 18);    // words 0-8: Pac-Man frame pointers
    const auto ghostFrames = pack.find(0x9E5C, 24);  // words 0-11: ghost frame pointers
    if (pacFrames.empty() || ghostFrames.empty()) return {};
    std::vector<Call> calls;
    const auto clear = [&](int x, int y) { calls.push_back({3, {0x69CD, x - 4, y - 4, 0, 0, 0, 0, 0}}); };   // masked clear, mask table $69CD
    const auto pac = [&](int c66, int x, int y) {                                                              // $79A4
        const int t = c66 & 3;
        const int idx = t == 0 ? 0 : ((t == 3 ? 1 : t) & 1 ? 3 + 1 : 3 + 5);   // direction 3 (left): dir + 1 or dir + 5
        calls.push_back({1, {word(pacFrames, static_cast<std::size_t>(idx)), x, y, 1, 128, 0, 0, 0}});
    };
    const auto ghost = [&](int x, int y) {                                                                     // $88A2, ghost 1
        calls.push_back({1, {word(ghostFrames, static_cast<std::size_t>(1 + (colour ? 6 : 0))), x, y, 1, 1, 0, 0, 0}});
    };
    calls.push_back({10, {}});
    int px = 0xC1, py = 0x7A, gx = 0xE5, gy = 0x7A, c66 = 0, c59 = 1, c80 = 1;
    for (;;) {   // phase 1: both walk left
        if (--c59 == 0) {
            c59 = 0x20;
            clear(px, py);
            px -= 2;
            if (px < 0x21) break;
            c66 = (c66 + 1) & 0xFF;
            pac(c66 & 0x0F, px, py);
            c66 &= 0x0F;
        }
        if (--c80 == 0) {
            c80 = 0x1D;
            clear(gx, gy);
            gx -= 2;
            ghost(gx, gy);
        }
    }
    clear(px, py);
    clear(gx, gy);
    px = 0;
    py -= 0x0E;
    gx = 0x38;
    static const std::uint16_t bigFrames[4] = {0x8330, 0x8380, 0x8330, 0x82E0};   // by the animation counter & 3 (read from the oracle's calls)
    for (;;) {   // phase 2: the giant Pac-Man chases the ghost to the right; $59 and $80 keep counting
        if (--c59 == 0) {
            c59 = 6;
            px += 2;
            if (px >= 0xA0) break;
            calls.push_back({8, {bigFrames[c66 & 3], px, py, 0, 0, 0, 0, 0}});
            c66 = (c66 + 1) & 0xFF;
        }
        if (--c80 == 0) {
            c80 = 7;
            clear(gx, gy);
            gx += 2;
            ghost(gx, gy);
        }
    }
    std::vector<std::uint8_t> out(40 + calls.size() * 33, 0);
    std::memcpy(out.data(), "CSCR", 4);
    const std::uint32_t n = static_cast<std::uint32_t>(calls.size());
    std::memcpy(out.data() + 4, &n, 4);
    for (std::size_t i = 0; i < calls.size(); ++i) {
        out[40 + i * 33] = calls[i].type;
        std::memcpy(out.data() + 41 + i * 33, calls[i].p, 32);
    }
    return out;
}

}  // namespace pacman::core
