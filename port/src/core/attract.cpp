#include "attract.h"

#include <algorithm>

namespace pacman::core {

namespace {

// The cycle-exact builder. Every routine below is the original's routine written out instruction group by instruction group: the numbers are the 6502 cycle counts of the
// instructions it runs (2 for LDA #, 3 for STA zp, 4 for LDA abs, 5 for LDA (zp),Y ...), one more for a page crossing of an indexed read and for a taken branch that
// crosses a page. The comments name the original's address ranges.
struct Builder {
    const datapack::Pack& pack;
    ScreenScript& out;
    bool trace;
    std::int64_t t = 0;
    int z[256] = {};
    int p55b2 = 0;                 // the operand of "LDA $0666,X" in the OR blit, patched by the callers
    int p6951 = 0;                 // the operand of the tile routine
    bool blitSteps = true;         // false while a dispatcher call draws its own 3 x 10 image with one step

    Builder(const datapack::Pack& p, ScreenScript& o, bool tr) : pack(p), out(o), trace(tr) {
        z[0x1D] = 1;
    }

    void add(int n) { t += n; }
    int u8(int address) const {
        const auto s = pack.find(static_cast<std::uint16_t>(address), 1);
        return s.empty() ? 0 : s[0];
    }
    static int cross(int base, int idx) { return ((base & 255) + idx > 255) ? 1 : 0; }
    static int branch(bool taken, int nxt, int target) {
        if (!taken) return 2;
        return 3 + (((nxt >> 8) != (target >> 8)) ? 1 : 0);
    }
    void event(int kind, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0) {
        if (trace) out.trace.push_back({t, kind, {a, b, c, d, e}});
    }
    void step(ScreenOp op, const DrawCall& call = {}, int a = 0) {
        ScreenStep s;
        s.at = t;
        s.op = op;
        s.call = call;
        s.a = a;
        out.steps.push_back(s);
    }

    // $5B58: the address of the row $10 (returns the low byte, which decides the page crossings of the writes)
    int r5b58() {
        const int y = z[0x10];
        add(3 + 4 + cross(0x5B67, y) + 3 + 4 + cross(0x5C27, y) + 2 + 3 + 6);
        return u8(0x5B67 + y);
    }
    // $5933: byte column ($04) and shift ($03) of the pixel column $0C (high byte $0D = 0)
    void r5933() {
        const int x = z[0x0C];
        add(3 + 3 + 3 + 4 + cross(0x5A58, x) + 3 + 4 + cross(0x5958, x) + 3 + 6);
        z[0x04] = u8(0x5A58 + x);
        z[0x03] = u8(0x5958 + x);
    }
    // $6F5E with no key pressed in the attract mode
    void rPoll() {
        event(4);
        add(62);
    }
    // $6CE8: a delay of x outer passes of 1,014 cycles
    void rDelay(int x) {
        for (int i = 0; i < x; ++i) {
            event(5, x - i);
            add(2 + 143 * 7 + 6 + 2);
            add(branch(i < x - 1, 0x6CF2, 0x6CE8));
        }
        add(6);
    }
    // $55A6: the OR blit; one loop pass per row (each pass is an event and a drawing step)
    void rBlit() {
        const int w = z[0x1A];
        for (;;) {
            const int data = p55b2 + z[0x03];
            event(3, data, z[0x04], z[0x10], z[0x1A], z[0x18]);
            if (blitSteps) step(ScreenOp::Draw, DrawCall{2, {data, z[0x04], z[0x10], w, 1}});
            add(6);
            const int lo12 = r5b58();
            add(3 + 3 + 3 + 3);
            int x = z[0x03];
            int yy = z[0x04];
            for (int b = 0; b < w; ++b) {
                add(4 + cross(p55b2, x) + 5 + cross(lo12, yy) + 6 + 2 + 2 + 5);
                add(b < w - 1 ? 3 : 2);
                ++x;
                ++yy;
            }
            z[0x03] = x;
            z[0x10] += 1;
            z[0x18] -= 1;
            add(3 + 5 + 5);
            if (z[0x18] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    // $6172: the small dot
    void rDot() {
        event(7, z[0x0C], z[0x10]);
        step(ScreenOp::Draw, DrawCall{5, {z[0x0C], z[0x10], 0, 0, 0}});
        add(6);
        r5933();
        add(5);
        z[0x03] = (z[0x03] << 1) & 255;
        for (int half = 0; half < 2; ++half) {
            add(6);
            const int lo12 = r5b58();
            const int x = z[0x03];
            const int yy = z[0x04];
            add(3 + 3);
            add(4 + cross(0x61A8, x) + 5 + cross(lo12, yy) + 6 + 2 + 2);
            add(4 + cross(0x61A8, x + 1) + 5 + cross(lo12, yy + 1) + 6);
            if (half == 0) {
                add(5);
                z[0x10] += 1;
            }
        }
        add(6);
    }
    // $693A: a tile of 8 rows x 2 bytes from the table at p6951
    void rTile() {
        event(8, p6951, z[0x0C] | (z[0x0D] << 8), z[0x10]);
        step(ScreenOp::Draw, DrawCall{6, {p6951, z[0x0C] | (z[0x0D] << 8), z[0x10], 0, 0}});
        add(2 + 3);
        z[0x18] = 0xF8;
        add(6);
        r5933();
        add(3 + 8 + 3);
        z[0x03] = (z[0x03] << 4) & 255;
        for (;;) {
            add(6);
            const int lo12 = r5b58();
            const int x = z[0x03];
            const int yy = z[0x04];
            add(3 + 3);
            add(4 + cross(p6951, x) + 5 + cross(lo12, yy) + 6 + 2 + 2);
            add(4 + cross(p6951, x + 1) + 5 + cross(lo12, yy + 1) + 6 + 2);
            z[0x03] = x + 2;
            z[0x10] += 1;
            z[0x18] = (z[0x18] + 1) & 255;
            add(3 + 5 + 5);
            if (z[0x18] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    // $7159: a tile from the table of pointers at $717D (the caller has paid the JSR)
    void r7159(int a) {
        add(2 + 2 + 4 + 4 + 4 + 4 + 4 + 4 + 3 + 3 + 3 + 3 + 3 + 3);
        p6951 = u8(0x717D + 2 * a) | (u8(0x717E + 2 * a) << 8);
        z[0x0C] = z[0x0E];
        z[0x0D] = z[0x0F];
        z[0x10] = z[0x11];
        add(6);
        rTile();
        add(6);
    }
    // $61B6: the energizer tile
    void r61b6() {
        add(2 + 4 + 4 + 2 + 4 + 4);
        p6951 = 0x61CA;
        add(6);
        rTile();
        add(6);
    }
    // $44F1: the 4 x 7 image at $450A
    void r44f1() {
        z[0x1A] = 4;
        z[0x18] = 7;
        p55b2 = 0x450A;
        z[0x03] = 0;
        add(2 + 3 + 2 + 3 + 2 + 4 + 2 + 4 + 2 + 3 + 3);
        rBlit();
    }
    // $6A20 with $1D <> 0: the OR path for $1E <> 0, otherwise the AND #$7F path
    void rDispatch(int a, int y) {
        event(6, a | (y << 8), z[0x0C] | (z[0x0D] << 8), z[0x10], z[0x1D], z[0x1E]);
        step(ScreenOp::Draw, DrawCall{1, {a | (y << 8), z[0x0C] | (z[0x0D] << 8), z[0x10], z[0x1D], z[0x1E]}});
        add(3 + 2 + 3);
        if (z[0x1E] != 0) {
            add(branch(true, 0x6A28, 0x6A62));
            p55b2 = a | (y << 8);
            add(4 + 4);
            add(6 + 26);
            z[0x0C] = (z[0x0C] - 4) & 255;
            z[0x10] = (z[0x10] - 4) & 255;
            add(2 + 3);
            z[0x18] = 10;
            add(6);
            r5933();
            add(6 + 3 + 4 + 3 + 6);
            z[0x03] = u8(0x7812 + z[0x03]);
            add(2 + 3);
            z[0x1A] = 3;
            add(3);
            const bool before = blitSteps;
            blitSteps = false;
            rBlit();
            blitSteps = before;
            return;
        }
        add(2);
        p55b2 = a | (y << 8);
        add(4 + 4);
        add(6 + 26);
        z[0x0C] = (z[0x0C] - 4) & 255;
        z[0x10] = (z[0x10] - 4) & 255;
        add(2 + 3 + 2 + 3);
        z[0x18] = 10;
        z[0x1A] = 3;
        add(6);
        r5933();
        add(6 + 3 + 4 + 3 + 6);
        z[0x03] = u8(0x7812 + z[0x03]);
        for (;;) {
            add(6);
            const int lo12 = r5b58();
            add(3 + 3 + 3 + 3);
            int x = z[0x03];
            int yy = z[0x04];
            for (int b = 0; b < 3; ++b) {
                add(4 + cross(p55b2, x) + 5 + cross(lo12, yy) + 2 + 6 + 2 + 2 + 5);
                add(b < 2 ? 3 : 2);
                ++x;
                ++yy;
            }
            z[0x03] = x;
            z[0x10] += 1;
            z[0x18] -= 1;
            add(3 + 5 + 5);
            if (z[0x18] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    // $88A2: the ghost image of ghost $E0 (title screen: $78 = $79 = 0, $1D = 1)
    void r88a2() {
        int x = 2 * z[0xE0];
        add(3 + 2 + 2 + 3 + 3 + 3 + 3);
        add(3 + 2);
        add(2 + 2 + 2 + 2 + 4 + 3 + 2 + 2 + 2);
        z[0x1E] = u8(0x88E1 + (x >> 1));
        x += 12;
        const int a = u8(0x9E5C + x);
        const int y = u8(0x9E5D + x);
        add(4 + cross(0x9E5C, x) + 4 + cross(0x9E5D, x));
        add(6);
        rDispatch(a, y);
        add(3 + 2 + 6);
    }

    // ---- text: $42D0 ----------------------------------------------------------------------------------------------------------------------------------------------
    void drawGlyph(int code) {
        add(6);                                     // JSR $4316
        add(3 + 6 + 3 + 4 + 2 + 4);
        const int v = code * 9;
        add(v <= 255 ? 3 : 8);
        add(2 + 4 + 2 + 4);
        add((v & 255) + 0x53 <= 255 ? 3 : 8);
        p55b2 = 0x4553 + 9 * code;
        glyphTail();
    }
    void glyphTail() {                              // $4338: draw (1 x 9), key poll, delay
        z[0x1A] = 1;
        z[0x18] = 9;
        z[0x03] = 0;
        add(15);
        add(6);
        rBlit();
        add(6);
        rPoll();
        add(2 + 3);
        rDelay(0x50);
    }
    void textLine(int ptr, int yIdx) {
        const int lo14 = ptr & 255;
        auto rd = [&](int i) {
            add(5 + ((lo14 + i) > 255 ? 1 : 0));
            return u8(ptr + i);
        };
        z[0x04] = rd(yIdx);
        add(3);
        ++yIdx;
        add(2);
        z[0x10] = rd(yIdx);
        add(3 + 3);
        z[0x0A] = z[0x10];
        ++yIdx;
        add(2);
        add(3);
        z[0x0B] = yIdx;
        for (;;) {
            add(3);
            const int c = rd(z[0x0B]);
            if (c & 0x80) {
                add(branch(true, 0x42E4, 0x4303));
                add(5 + 6);
                z[0x0B] += 1;
                return;
            }
            add(2);
            if (c == 0) {
                add(branch(true, 0x42E6, 0x42F6));
                drawGlyph(c);
            } else {
                add(2 + 2);
                if (c == 0x20) {
                    add(branch(true, 0x42EA, 0x42F9));
                } else {
                    add(2 + 2);
                    if (c == 0x21) {
                        add(branch(true, 0x42EE, 0x4306));
                        add(2 + 4 + 2 + 4);
                        p55b2 = 0x4673;
                        add(6);
                        glyphTail();
                        add(3);
                    } else {
                        add(2 + 2);
                        if (c >= 3) {
                            add(branch(true, 0x42F2, 0x42F6));
                        } else {
                            add(2);
                            add(5 + 5);
                            z[0x10] += 2;
                        }
                        drawGlyph(c);
                    }
                }
            }
            add(3 + 3 + 5 + 5 + branch(true, 0x4303, 0x42DE));
            z[0x10] = z[0x0A];
            z[0x0B] += 1;
            z[0x04] += 1;
        }
    }

    // ---- the logo: $5260, $5CF0 --------------------------------------------------------------------------------------------------------------------------------------
    void r5ce7() {
        add(2 + 2);
        z[0x03] += 1;
        if (z[0x03] < 7) {
            add(branch(true, 0x5CEC, 0x5CEF));
        } else {
            add(2 + 2 + 2);
            z[0x03] = 0;
            z[0x04] += 1;
        }
        add(6);
    }
    void rHline() {                                 // $5CF0 with $1D = 1: two pixels per step
        event(2, z[0x0C], z[0x0E], z[0x10], z[0x1D], z[0x1E]);
        step(ScreenOp::Draw, DrawCall{8, {z[0x0C], z[0x0E], z[0x10], 2, z[0x1E]}});
        add(2 + 3 + 3);
        z[0x0D] = z[0x0F] = 0;
        add(6);
        const int lo12 = r5b58();
        add(6);
        r5933();
        add(3 + 3);
        for (;;) {
            add(6 + 4 + cross(0x5925, z[0x03]) + 5 + cross(lo12, z[0x04]) + 6 + 6);
            add(5 + cross(lo12, z[0x04]) + 2 + 3 + 6);
            add(3 + 2);
            add(6);
            r5ce7();
            add(6);
            r5ce7();
            add(2 + 3 + 2 + 3);
            const int v = z[0x0C] + 2;
            z[0x0C] = v & 255;
            if (v > 255) {
                add(2 + 5);
                z[0x0D] += 1;
            } else {
                add(3);
            }
            add(3 + 3);
            const int d = z[0x0D], f = z[0x0F];
            if (d < f) {
                add(3);
                continue;
            }
            add(2);
            if (d == f) {
                add(3);
                add(3 + 3);
                if (z[0x0E] >= z[0x0C]) {
                    add(3);
                    continue;
                }
                add(2);
                break;
            }
            add(2);
            add(branch(true, 0x5D2A, 0x5D30));
            break;
        }
        add(6);
    }
    void runLogo(int ptr) {                         // $5260
        add(3 + 3 + 3 + 3 + 2 + 3 + 2 + 3 + 3 + 3);
        z[0x1D] = 1;
        z[0x1E] = 0;
        for (;;) {
            add(2);
            const int lo = ptr & 255;
            auto rd = [&](int i) {
                add(5 + ((lo + i) > 255 ? 1 : 0));
                return u8(ptr + i);
            };
            z[0x0C] = rd(0);
            add(3);
            add(2);
            z[0x0E] = rd(1);
            add(3);
            add(2);
            const int c = rd(2);
            z[0x10] = c;
            add(3);
            add(2);
            if (c == 0xFD) {
                add(branch(true, 0x5287, 0x52A7));
                add(4 + 3 + 3);
                return;
            }
            add(2 + 2);
            if (c == 0xFE) {
                add(branch(true, 0x528B, 0x52AD));
                add(2 + 3 + 3);
                z[0x1E] = 0;
            } else {
                add(2 + 2);
                if (c == 0xFF) {
                    add(branch(true, 0x528F, 0x52B4));
                    add(2 + 3 + 3);
                    z[0x1E] = 0x80;
                } else {
                    add(2 + 6);
                    rHline();
                    add(4 + 2 + 2);
                }
            }
            add(2 + 3 + 2 + 3);
            if (lo + 3 > 255) {
                add(2 + 5);
            } else {
                add(branch(true, 0x52A2, 0x52A4));
            }
            ptr += 3;
            add(3);
        }
    }

    // ---- the whole turn ---------------------------------------------------------------------------------------------------------------------------------------------
    void run() {
        // $52BB: the video switches, the lo-res page 2 filled with $DD, the page clear
        add(16 + 2 + 2 + 256 * 25 - 1);
        step(ScreenOp::LoresFill, {}, 0xDD);
        add(6);
        event(1);
        step(ScreenOp::WipeStart);
        add(90575);
        add(2 + 2 + 6);                             // LDA #$7A, LDY #$4A, JSR $5260
        runLogo(0x4A7A);
        rPoll();
        add(6 + 4 + 4 + 4 + 4);                     // JSR $6F2F: hi-res page 1 on
        step(ScreenOp::ShowHires);
        add(6);
        add(2);                                     // LDY #$0A
        for (int k = 0; k < 11; ++k) {
            add(2 + 6);
            rDelay(250);
            add(4 + 2 + 2);
            add(2 + (k < 10 ? 3 : 2));
        }
        add(6);                                     // RTS of $52BB
        add(6);                                     // JSR $6F46 at $43D9
        event(1);
        step(ScreenOp::WipeStart);
        add(90575);
        add(2 + 3 + 2 + 3 + 3 + 2 + 3 + 2 + 3 + 2);
        const int ptr = 0x434F;
        add(6);
        textLine(ptr, 0);
        auto ghost = [&](int row, int e0) {
            add(6 + 2 + 3 + 2 + 3 + 6);
            z[0x0C] = 0x49;
            z[0x0D] = 0;
            add(2 + 3 + 2 + 3 + 6);
            z[0x10] = row;
            z[0xE0] = e0;
            r88a2();
            add(3 + 6);
            textLine(ptr, z[0x0B]);
            add(3 + 6);
            textLine(ptr, z[0x0B]);
        };
        ghost(0x22, 1);
        ghost(0x31, 3);
        ghost(0x40, 2);
        ghost(0x4F, 4);
        add(2 + 3 + 2 + 3 + 6);                     // $4453: the dot
        z[0x0C] = 0x74;
        z[0x10] = 0x78;
        rDot();
        add(2 + 3 + 2 + 3 + 2 + 3 + 2 + 6);
        z[0x0E] = 0x7C;
        z[0x0F] = 0;
        z[0x11] = 0x75;
        r7159(1);
        add(2 + 3 + 2 + 3 + 2 + 6);
        z[0x0E] += 6;
        r7159(0);
        add(2 + 3 + 2 + 3 + 6);
        z[0x04] = 0x14;
        z[0x10] = 0x75;
        r44f1();
        add(2 + 3 + 2 + 3 + 6);
        z[0x0C] = 0x73;
        z[0x10] = 0x81;
        r61b6();
        add(2 + 3 + 2 + 3 + 2 + 6);
        z[0x0E] = 0x7C;
        z[0x11] = 0x80;
        r7159(5);
        add(2 + 3 + 2 + 3 + 2 + 6);
        z[0x0E] += 6;
        r7159(0);
        add(2 + 3 + 2 + 3 + 6);
        z[0x04] = 0x14;
        z[0x10] = 0x80;
        r44f1();
        add(2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 2 + 3 + 2 + 4 + 2 + 4 + 6);   // $44B5: the copyright mark
        z[0x04] = 0x0C;
        z[0x10] = 0x9A;
        z[0x03] = 0;
        z[0x1A] = 5;
        z[0x18] = 9;
        p55b2 = 0x4526;
        rBlit();
        for (int i = 0; i < 3; ++i) {
            add(3 + 6);
            textLine(ptr, z[0x0B]);
        }
        add(3);                                     // JMP $4689
        add(63);                                    // $4689-$46B5: the start-up values of the chase, then JSR $8712
        out.length = t;
    }
};

}  // namespace

bool buildTitleScript(const datapack::Pack& pack, ScreenScript& out, bool withTrace) {
    // the data this screen reads (all in the data pack)
    static const struct { std::uint16_t address; std::size_t length; } need[] = {
        {0x434F, 131}, {0x450A, 370}, {0x4A7A, 2022}, {0x5958, 512}, {0x5B67, 384}, {0x717D, 24}, {0x7812, 7}, {0x88E1, 5}, {0x96E8, 4888}, {0x61A8, 14}, {0x61CA, 1904},
    };
    for (const auto& n : need)
        if (pack.find(n.address, n.length).empty()) return false;
    out = ScreenScript{};
    Builder b(pack, out, withTrace);
    b.run();
    return true;
}

// ---- the player --------------------------------------------------------------------------------------------------------------------------------------------------------
void ScreenPlayer::start(const ScreenScript* script, const datapack::Pack* pack) {
    script_ = script;
    pack_ = pack;
    next_ = 0;
    now_ = 0;
    page_ = hires::Page();
    lores_.fill(0);
    hiresShown_ = false;
    wipeAt_ = -1;
    wiped_ = 0;
}

// the page clear $6F46: 10 cycles of set-up, then 32 pages; a page costs 2,830 cycles (a store every 11 cycles after 2 cycles of set-up, the store takes 6)
void ScreenPlayer::applyWipeUpTo(std::int64_t t) {
    if (wipeAt_ < 0) return;
    const std::int64_t d = t - wipeAt_ - 10;
    std::size_t count = 0;
    if (d >= 0) {
        const std::int64_t p = d / 2830;
        const std::int64_t r = d % 2830;
        if (p >= 32) {
            count = hires::kPageBytes;
        } else {
            std::int64_t n = r >= 8 ? std::min<std::int64_t>(256, (r - 8) / 11 + 1) : 0;
            count = static_cast<std::size_t>(p * 256 + n);
        }
    }
    while (wiped_ < count) page_.set(wiped_++, hires::kBlack);
}

void ScreenPlayer::advance(std::int64_t cycles) {
    if (script_ == nullptr || pack_ == nullptr) return;
    const std::int64_t target = now_ + cycles;
    while (next_ < script_->steps.size() && script_->steps[next_].at <= target) {
        const ScreenStep& s = script_->steps[next_++];
        applyWipeUpTo(s.at);
        switch (s.op) {
            case ScreenOp::LoresFill:
                lores_.fill(static_cast<std::uint8_t>(s.a));
                hiresShown_ = false;
                break;
            case ScreenOp::ShowHires:
                hiresShown_ = true;
                break;
            case ScreenOp::WipeStart:
                applyWipeUpTo(s.at);
                wipeAt_ = s.at;
                wiped_ = 0;
                break;
            case ScreenOp::Draw:
                executeCall(page_, s.call, *pack_);   // the logo lines are type 8 ($5CF0): x0, x1, y, step, the palette flag $1E
                break;
        }
    }
    now_ = target;
    applyWipeUpTo(now_);
}

}  // namespace pacman::core
