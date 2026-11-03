#include "cartoon.h"

#include <algorithm>
#include <cstring>
#include <utility>

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

// Collects the drawing calls of one cartoon together with the loop pass in which each is drawn.
struct Builder {
    std::span<const std::uint8_t> pacFrames, ghostFrames;
    bool colour = true;
    std::vector<Call> calls;
    std::vector<std::uint32_t> stamp;
    std::uint32_t pass = 0;

    void sync() { while (stamp.size() < calls.size()) stamp.push_back(pass); }   // calls added since the last sync belong to the current pass
    void beginPass() { sync(); ++pass; }
    void pause(std::uint32_t passes) { sync(); pass += passes; }
    void clear(int x, int y) { calls.push_back({3, {0x69CD, x - 4, y - 4, 0, 0, 0, 0, 0}}); }             // masked clear, mask table $69CD
    void clearGhost3(int x, int y) { calls.push_back({3, {0x6A04, x - 5, y - 4, 0, 0, 0, 0, 0}}); }       // cartoon 3's shifted ghost: mask table $6A04, at (x - 5, y - 4)
    void pac(int c66, int dir, int x, int y) {                                                              // $79A4
        const int t = c66 & 3;
        const int a = t == 3 ? 1 : t;
        const int idx = t == 0 ? 0 : ((a & 1) ? dir + 1 : dir + 5);
        calls.push_back({1, {word(pacFrames, static_cast<std::size_t>(idx)), x, y, 1, 128, 0, 0, 0}});
    }
    void ghost(int x, int y) {                                                                              // $88A2, ghost 1
        calls.push_back({1, {word(ghostFrames, static_cast<std::size_t>(1 + (colour ? 6 : 0))), x, y, 1, 1, 0, 0, 0}});
    }
    void vline(int x, int row0, int row1) { calls.push_back({11, {x, row0, row1, 1, 0, 0, 0, 0}}); }
    void shiftedGhost(int image, int x, int y) { calls.push_back({9, {image, x, y, 1, 1, 0, 0, 0}}); }      // $7E1F: image shifted by software, drawn at (x - 1, y)
};

CartoonPlan finish(Builder& b, std::int64_t cycles) {
    b.sync();
    std::vector<std::uint8_t> out(40 + b.calls.size() * 33, 0);
    std::memcpy(out.data(), "CSCR", 4);
    const std::uint32_t n = static_cast<std::uint32_t>(b.calls.size());
    std::memcpy(out.data() + 4, &n, 4);
    for (std::size_t i = 0; i < b.calls.size(); ++i) {
        out[40 + i * 33] = b.calls[i].type;
        std::memcpy(out.data() + 41 + i * 33, b.calls[i].p, 32);
    }
    CartoonPlan plan;
    plan.script = std::move(out);
    plan.passOfCall = std::move(b.stamp);
    plan.passes = b.pass;
    plan.cycles = cycles;
    return plan;
}

CartoonPlan cartoon1(Builder& b) {   // $81AE
    b.calls.push_back({10, {}});
    int px = 0xC1, py = 0x7A, gx = 0xE5, gy = 0x7A, c66 = 0, c59 = 1, c80 = 1;
    for (;;) {   // phase 1: both walk left
        b.beginPass();
        if (--c59 == 0) {
            c59 = 0x20;
            b.clear(px, py);
            px -= 2;
            if (px < 0x21) break;
            c66 = (c66 + 1) & 0xFF;
            b.pac(c66 & 0x0F, 3, px, py);
            c66 &= 0x0F;
        }
        if (--c80 == 0) {
            c80 = 0x1D;
            b.clear(gx, gy);
            gx -= 2;
            b.ghost(gx, gy);
        }
    }
    b.clear(px, py);
    b.clear(gx, gy);
    b.pause(800);   // 8 x 100 short delays
    px = 0;
    py -= 0x0E;
    gx = 0x38;
    static const std::uint16_t bigFrames[4] = {0x8330, 0x8380, 0x8330, 0x82E0};   // by the animation counter & 3 (read from the oracle's calls)
    for (;;) {   // phase 2: the giant Pac-Man chases the ghost to the right; $59 and $80 keep counting
        b.beginPass();
        if (--c59 == 0) {
            c59 = 6;
            px += 2;
            if (px >= 0xA0) break;
            b.calls.push_back({8, {bigFrames[c66 & 3], px, py, 0, 0, 0, 0, 0}});
            c66 = (c66 + 1) & 0xFF;
        }
        if (--c80 == 0) {
            c80 = 7;
            b.clear(gx, gy);
            gx += 2;
            b.ghost(gx, gy);
        }
    }
    return finish(b, kCartoonCycles[0]);
}

CartoonPlan cartoon2(Builder& b, std::span<const std::uint8_t> tab) {   // $83D0: Pac-Man walks past a post, the ghost tears on it
    b.calls.push_back({10, {}});
    int px = 0xC1, py = 0x7A, gx = 0xE5, gy = 0x7A, c66 = 0, c59 = 1, c80 = 1;
    for (;;) {
        b.beginPass();
        if (--c59 == 0) {
            c59 = 0x28;
            b.vline(0x80, 0x7C, 0x81);
            b.vline(0x81, 0x7C, 0x81);
            b.clear(px, py);
            px -= 2;
            if (px < 0x10) break;
            c66 = (c66 + 1) & 0xFF;
            b.pac(c66 & 0x0F, 3, px, py);
            c66 &= 0x0F;
        }
        if (--c80 != 0) continue;
        c80 = 0x21;
        if (gx == 0x71) continue;
        b.clear(gx, gy);
        gx -= 2;
        b.ghost(gx, gy);
        if (gx >= 0x7B) continue;
        if (gx == 0x71) {
            b.clear(gx, gy);
            b.clear(gx + 10, gy);
            b.ghost(gx, gy);
            b.vline(0x74, 0x7D, 0x7F);
            b.vline(0x76, 0x7E, 0x7F);
        } else {
            const std::size_t x = static_cast<std::size_t>(gx - 0x73);
            c80 = 0xA0;
            b.vline(tab[x], tab[x + 1], 0x7F);
        }
    }
    return finish(b, kCartoonCycles[1]);
}

CartoonPlan cartoon3(Builder& b, std::span<const std::uint8_t> ptrTable) {   // $8486: the ghost drags a torn cloth, then both come back
    b.calls.push_back({10, {}});
    int px = 0xC1, py = 0x7A, gx = 0xE5, gy = 0x7A, c66 = 0, c59 = 1, c80 = 1;
    for (;;) {   // phase 1
        b.beginPass();
        if (--c59 == 0) {
            c59 = 0x26;
            b.clear(px, py);
            px -= 2;
            if (px < 0x20) break;
            c66 = (c66 + 1) & 0xFF;
            b.pac(c66 & 0x0F, 3, px, py);
            c66 &= 0x0F;
        }
        if (--c80 == 0) {
            c80 = 0x21;
            b.clear(gx, gy);
            gx -= 2;
            b.ghost(gx, gy);
            b.vline(gx + 3, 0x7D, 0x7F);
        }
    }
    b.clear(gx, gy);
    b.pause(800);   // INFERRED: the same pause as cartoon 1
    gx = 0x37;
    px = 0x10;
    const int ptrs[2] = {word(ptrTable, 0), word(ptrTable, 1)};   // $854A, $854C
    for (;;) {   // phase 2
        b.beginPass();
        if (--c59 == 0) {
            c59 = 0x22;
            b.clear(px, py);
            if (px >= 0xB0) break;
            px += 2;
            c66 = (c66 + 1) & 0xFF;
            b.pac(c66 & 0x0F, 2, px, py);
            c66 &= 0x0F;
        }
        if (--c80 == 0) {
            c80 = 0x27;
            b.clearGhost3(gx, gy);
            gx += 2;
            b.shiftedGhost(ptrs[(c66 & 2) >> 1], gx, gy);
        }
    }
    return finish(b, kCartoonCycles[2]);
}

}  // namespace

CartoonPlan cartoonPlan(const datapack::Pack& pack, int cartoon, bool colour) {
    Builder b;
    b.pacFrames = pack.find(0x96E8, 18);     // words 0-8: Pac-Man frame pointers
    b.ghostFrames = pack.find(0x9E5C, 24);   // words 0-11: ghost frame pointers
    b.colour = colour;
    if (b.pacFrames.empty() || b.ghostFrames.empty()) return {};
    if (cartoon == 1) return cartoon1(b);
    if (cartoon == 2) {
        const auto tab = pack.find(0x847E, 8);   // the tear's line table
        if (tab.empty()) return {};
        return cartoon2(b, tab);
    }
    if (cartoon == 3) {
        const auto ptrs = pack.find(0x854A, 4);  // the two cloth images
        if (ptrs.empty()) return {};
        return cartoon3(b, ptrs);
    }
    return {};
}

CartoonPlan cartoon1Plan(const datapack::Pack& pack, bool colour) { return cartoonPlan(pack, 1, colour); }

std::vector<std::uint8_t> cartoon1Script(const datapack::Pack& pack, bool colour) { return cartoonPlan(pack, 1, colour).script; }

std::size_t cartoonCallsDue(const CartoonPlan& plan, std::int64_t elapsedCycles) noexcept {
    if (plan.passes == 0 || elapsedCycles < 0) return 0;
    if (elapsedCycles >= plan.cycles) return plan.passOfCall.size();
    // the call stamped with pass p is drawn when p passes have run: p * cycles / passes
    std::size_t n = 0;
    while (n < plan.passOfCall.size() && static_cast<std::int64_t>(plan.passOfCall[n]) * plan.cycles / plan.passes <= elapsedCycles) ++n;
    return n;
}

bool CartoonPlayer::start(const datapack::Pack& pack, int cartoon) {
    active_ = false;
    if (cartoon < 1 || cartoon > 3) return false;
    plan_ = cartoonPlan(pack, cartoon);
    if (plan_.script.empty()) return false;
    elapsed_ = 0;
    active_ = true;
    return true;
}

void CartoonPlayer::advance(std::int64_t cycles) noexcept {
    if (!active_) return;
    elapsed_ += cycles;
    if (elapsed_ >= plan_.cycles) active_ = false;
}

void CartoonPlayer::render(const datapack::Pack& pack, hires::Page& page) const {
    hires::ReplayStats stats;
    const std::size_t n = std::max<std::size_t>(1, cartoonCallsDue(plan_, elapsed_));
    page.clear();
    hires::replayScript(pack, plan_.script, page, stats, static_cast<std::uint32_t>(n));
}

}  // namespace pacman::core
