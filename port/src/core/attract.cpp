#include "attract.h"

#include <algorithm>
#include <utility>

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
    bool noDraw = false;           // true while the shifted-image routine runs its dispatcher: the type 9 step already draws the image
    int rec[5][20] = {};           // the four ghost records (ghosts 1-4) of the chase, copied to and from the working area $70-$83
    int p69b8 = 0x69CD;            // the mask table operand of the masked clear
    bool savedPlayers = false;     // hudCycles: $53CF loads each player's own score and lives from the tables below; the bonus-life test of $7124 runs
    std::array<std::array<int, 3>, 6> savedScore{};
    std::array<int, 6> savedLives{};

    Builder(const datapack::Pack& p, ScreenScript& o, bool tr) : pack(p), out(o), trace(tr) {
        z[0x1D] = 1;
    }

    void add(int n) { t += n; }
    struct Busy {                  // records the window of a drawing routine
        Builder& b;
        std::int64_t start;
        explicit Busy(Builder& bb) : b(bb), start(bb.t) {}
        ~Busy() { b.out.busy.emplace_back(start, b.t); }
    };
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
        if (noDraw && op == ScreenOp::Draw) return;
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
        if (z[0x0D] != 0) {                         // x >= 256: the column table is read from the pixel x + 4 and 36 columns are added
            const int xx = (z[0x0C] + 4) & 255;
            add(3 + 2 + 2 + 3 + 2 + 2 + 2 + 4 + cross(0x5A58, xx) + 2 + 3 + 4 + cross(0x5958, xx) + 3 + 6);
            z[0x04] = u8(0x5A58 + xx) + 0x24;
            z[0x03] = u8(0x5958 + xx);
            return;
        }
        const int x = z[0x0C];
        add(3 + 3 + 3 + 4 + cross(0x5A58, x) + 3 + 4 + cross(0x5958, x) + 3 + 6);
        z[0x04] = u8(0x5A58 + x);
        z[0x03] = u8(0x5958 + x);
    }
    // $6F5E with no key pressed in the attract mode
    void rPoll() {
        out.polls.push_back(t);
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
        Busy window(*this);
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
        Busy window(*this);
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
        Busy window(*this);
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
        Busy window(*this);
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
        Busy window(*this);
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
                    out.spaceChecks.push_back({t, 19});
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

    // ---- the chase: $4689-$47D9 -------------------------------------------------------------------------------------------------------------------------------------
    void copyIn() {                                 // $86FC (the JSR is paid by the caller): ghost record $E0 -> $70-$83
        const int e0 = z[0xE0];
        add(3 + 4 + cross(0x870D, e0) + 2 + 20 * 16 - 1 + 6);
        for (int i = 0; i < 20; ++i) z[0x70 + i] = rec[e0][i];
    }
    void copyOut() {                                // $8712: $70-$83 -> ghost record $E0 (15 cycles a byte)
        const int e0 = z[0xE0];
        add(3 + 4 + cross(0x870D, e0) + 2 + 20 * 15 - 1 + 6);
        for (int i = 0; i < 20; ++i) rec[e0][i] = z[0x70 + i];
    }
    void r7d80() {                                  // JSR $7D80: the place of Pac-Man
        add(6 + 3 + 3 + 3 + 3 + 2 + 3 + 6);
        z[0x0C] = z[0x60];
        z[0x10] = z[0x61];
        z[0x0D] = 0;
    }
    void r7d8d() {                                  // JSR $7D8D: the place of the ghost
        add(6 + 3 + 3 + 3 + 3 + 2 + 3 + 6);
        z[0x0C] = z[0x70];
        z[0x10] = z[0x71];
        z[0x0D] = 0;
    }
    void r7da9() {                                  // JSR $7DA9: x -= 5, y -= 4
        add(6 + 26);
        z[0x0C] = (z[0x0C] - 5) & 255;
        z[0x10] = (z[0x10] - 4) & 255;
    }
    void rMclearBody() {                            // $69A1: the masked clear of a 3 x 10 block
        Busy window(*this);
        event(9, p69b8, z[0x0C] | (z[0x0D] << 8), z[0x10]);
        step(ScreenOp::Draw, DrawCall{3, {p69b8, z[0x0C] | (z[0x0D] << 8), z[0x10], 0, 0}});
        add(2 + 3);
        z[0x18] = 0xF6;
        add(6);
        r5933();
        add(5 + 5);
        z[0x03] = (z[0x03] << 2) & 255;
        for (;;) {
            add(6);
            const int lo12 = r5b58();
            add(2 + 3 + 3 + 3);
            int x = z[0x03];
            int yy = z[0x04];
            for (int b = 0; b < 3; ++b) {
                add(4 + cross(p69b8, x) + 5 + cross(lo12, yy) + 2 + 6 + 2 + 2 + 5);
                add(b < 2 ? 3 : 2);
                ++x;
                ++yy;
            }
            z[0x10] += 1;
            z[0x18] = (z[0x18] + 1) & 255;
            add(5 + 5);
            if (z[0x18] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    void r699e() {                                  // JSR $699E: erase the block at ($0C - 4, $10 - 4)
        add(6);
        add(6 + 26);                                // JSR $7D9A
        z[0x0C] = (z[0x0C] - 4) & 255;
        z[0x10] = (z[0x10] - 4) & 255;
        rMclearBody();
    }
    void r69e9() {                                  // JSR $69E9: erase with the second mask table at ($0C - 5, $10 - 4)
        add(6);
        r7da9();
        add(2 + 2 + 4 + 4);
        p69b8 = 0x6A04;
        add(6);
        rMclearBody();
        add(2 + 2 + 4 + 4);
        p69b8 = 0x69CD;
        add(6);
    }
    void r79a4() {                                  // JSR $79A4: Pac-Man's image
        add(6);
        int a = z[0x66] & 0x0F;
        add(3 + 2 + 3);
        z[0x66] = a;
        const int t3 = a & 3;
        add(2);
        int idx;
        if (t3 == 0) {
            add(branch(true, 0x79AE, 0x79BE));
            idx = 0;
        } else {
            add(2);
            add(2);
            int carry;
            if (t3 == 3) {
                add(2 + 2);
                a = 1;
                carry = 1;
            } else {
                add(branch(true, 0x79B2, 0x79B4));
                a = t3;
                carry = 0;
            }
            add(2);
            carry = a & 1;
            add(3);
            if (carry) {
                add(branch(true, 0x79B9, 0x79BC));
                idx = z[0x62] + 1;
                add(2);
            } else {
                add(2);
                add(2 + 2);
                idx = z[0x62] + 5;
                add(2);
            }
        }
        add(2 + 2 + 2 + 3);
        z[0x1E] = 0x80;
        const int x = 2 * idx;
        const int lo = u8(0x96E8 + x);
        const int hi = u8(0x96E9 + x);
        add(4 + cross(0x96E8, x) + 4 + cross(0x96E9, x));
        add(3);
        rDispatch(lo, hi);
    }
    void r88a2Chase() {                             // JSR $88A2 with the frightened flag, the eyes flag and the timer of the chase
        add(6);
        int x = 2 * z[0xE0];
        add(3 + 2 + 2 + 3);
        if (z[0x78] == 0) {
            add(branch(true, 0x88AA, 0x88BC));
        } else {
            add(2);
            add(3);
            if (z[0xE2] != 0) {
                add(branch(true, 0x88AE, 0x88BA));
                x = 0;
                add(2);
            } else {
                add(2);
                add(3 + 2);
                if (z[0xE1] >= 0x80) {
                    add(branch(true, 0x88B4, 0x88BA));
                    x = 0;
                    add(2);
                } else {
                    add(2);
                    x = 6;
                    add(2 + 2);
                    if ((z[0xE1] & 0x10) == 0) {
                        add(branch(true, 0x88BA, 0x88C2));
                    } else {
                        add(2);
                        x = 0;
                        add(2);
                    }
                }
            }
        }
        add(3);
        if (z[0x79] == 0) {
            add(branch(true, 0x88C0, 0x88C2));
        } else {
            add(2);
            x = 0x0A;
            add(2);
        }
        add(3 + 2);
        add(2 + 2 + 2 + 2);
        const int y = x >> 1;
        add(4 + cross(0x88E1, y) + 3);
        z[0x1E] = u8(0x88E1 + y);
        add(2 + 2 + 2);
        x += 12;
        const int lo = u8(0x9E5C + x);
        const int hi = u8(0x9E5D + x);
        add(4 + cross(0x9E5C, x) + 4 + cross(0x9E5D, x));
        add(6);
        rDispatch(lo, hi);
        add(3 + 2 + 6);
    }
    void r7819() {                                  // $7819 (the JSR is paid by the caller): the energizer is blanked
        Busy window(*this);
        step(ScreenOp::Draw, DrawCall{11, {z[0x0C], z[0x10], 0, 0, 0}});
        add(2 + 3);
        z[0x18] = 0xF8;
        add(6);
        r5933();
        add(5);
        z[0x03] = (z[0x03] << 1) & 255;
        for (;;) {
            add(6);
            const int lo12 = r5b58();
            const int x = z[0x03];
            const int yy = z[0x04];
            add(3 + 3);
            add(4 + cross(0x7843, x) + 5 + cross(lo12, yy) + 2 + 6 + 2);
            add(4 + cross(0x7844, x) + 5 + cross(lo12, yy + 1) + 2 + 6);
            z[0x10] += 1;
            z[0x18] = (z[0x18] + 1) & 255;
            add(5 + 5);
            if (z[0x18] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    void r7e4f() { add(2 + 10 * 63 + 9 * 3 + 2 + 6); }   // $7E4F: the 30-byte image buffer shifted by one pixel
    void r7e1f() {                                  // JSR $7E1F: the score image of an eaten ghost
        Busy window(*this);
        const int ptr = z[0x16] | (z[0x17] << 8);
        if (trace) out.trace.push_back({t + 6, 10, {ptr, z[0x0C] | (z[0x0D] << 8), z[0x10], z[0x1D], z[0x1E]}});
        step(ScreenOp::Draw, DrawCall{9, {ptr, z[0x0C] | (z[0x0D] << 8), z[0x10], z[0x1D], z[0x1E]}});
        add(6);
        r7da9();
        add(6);
        r5933();
        add(2);
        const int lo16 = ptr & 255;
        for (int i = 29; i >= 0; --i) add(5 + ((lo16 + i) > 255 ? 1 : 0) + 5 + 2 + (i > 0 ? 3 : 2));
        add(3);
        if (z[0x03] == 0) {
            add(branch(true, 0x7E33, 0x7E3C));
        } else {
            add(2);
            add(3);
            const int n = z[0x03];
            for (int k = 0; k < n; ++k) {
                add(6);
                r7e4f();
                add(5);
                add(k < n - 1 ? 3 : 2);
            }
        }
        add(6 + 3 + 4 + 3 + 6);
        z[0x03] = u8(0x7812 + z[0x03]);
        add(6 + 26);
        z[0x0C] = (z[0x0C] + 4) & 255;
        z[0x10] = (z[0x10] + 4) & 255;
        add(2 + 2 + 2 + 3);
        int lo = 0x70 - z[0x03];
        int hi;
        if (lo >= 0) {
            add(branch(true, 0x7E4B, 0x7E4C));
            hi = 0x7E;
        } else {
            add(2 + 2);
            lo += 256;
            hi = 0x7D;
        }
        add(3);
        const bool before = noDraw;
        noDraw = true;
        const bool beforeBlit = blitSteps;
        blitSteps = false;
        rDispatch(lo & 255, hi);
        blitSteps = beforeBlit;
        noDraw = before;
    }
    void r467c() {                                  // JSR $467C: the ghost-eaten animation (attract mode: a delay instead of the sound)
        add(6);
        add(6);                                     // JSR $7DB8
        const int a = z[0x68];
        add(3 + 2);
        int ptr;
        if (a == 2) {
            add(branch(true, 0x7DBE, 0x7DCC));
            ptr = 0x7FC7;
            add(2 + 2 + 3);
        } else {
            add(2 + 2);
            if (a == 4) {
                add(branch(true, 0x7DC2, 0x7DD2));
                ptr = 0x7FE5;
                add(2 + 2 + 3);
            } else {
                add(2 + 2);
                if (a == 8) {
                    add(branch(true, 0x7DC6, 0x7DD8));
                    ptr = 0x8003;
                    add(2 + 2);
                } else {
                    add(2);
                    ptr = 0x8021;
                    add(2 + 2 + 3);
                }
            }
        }
        add(3 + 3);
        z[0x16] = ptr & 255;
        z[0x17] = ptr >> 8;
        r7d80();
        r699e();
        r7d8d();
        r699e();
        r7d8d();
        r7e1f();
        add(3);
        add(branch(true, 0x7DF6, 0x7E0E));
        add(2 + 6);
        rDelay(255);
        r7d8d();
        r69e9();
        r7d80();
        add(3);                                     // JMP $79A4: the dispatcher's RTS returns to $467F
        add(-6);
        r79a4();
        add(2 + 2 + 3 + 3 + 3 + 2 + 6);             // CLC, SED, LDA $68, ADC $68, STA $68, CLD, RTS
        const int v = z[0x68];
        int lo = (v & 15) * 2;
        int hi = (v >> 4) * 2;
        if (lo > 9) {
            lo -= 10;
            ++hi;
        }
        z[0x68] = ((hi % 10) << 4) | lo;
    }
    void runChase() {                               // from the first JSR $8712 at $46B5 to the JMP $5303 (time t = the cycle at which that routine starts working)
        z[0x5E] = 0x70;
        z[0x59] = 0x20;
        z[0x80] = 0x18;
        z[0x62] = 3;
        z[0x66] = 0;
        z[0x78] = 0;
        z[0x79] = 0;
        z[0x81] = 1;
        z[0x5F] = 1;
        z[0x60] = 0x8D;
        z[0x71] = 0x68;
        z[0x61] = 0x68;
        z[0xE0] = 4;
        for (int e0 = 4; e0 >= 1; --e0) {
            z[0xE0] = e0;
            copyOut();
            add(5);
            add(e0 > 1 ? 3 : 2);
            if (e0 > 1) add(6);
        }
        z[0xE0] = 0;
        add(2 + 3 + 2 + 3 + 2 + 3 + 2 + 3);
        rec[1][0] = 0xB1;
        rec[2][0] = 0xC1;
        rec[3][0] = 0xD1;
        rec[4][0] = 0xE1;
        for (;;) {                                  // loop A: Pac-Man runs to the left
            add(5);
            z[0x59] -= 1;
            if (z[0x59] != 0) {
                add(branch(true, 0x46D0, 0x46EC));
            } else {
                add(2);
                add(6);
                rPoll();
                add(2 + 3);
                z[0x59] = 0x20;
                r7d80();
                r699e();
                add(2 + 3 + 2 + 3);
                z[0x60] -= 2;
                r7d80();
                add(5);
                z[0x66] += 1;
                r79a4();
            }
            add(6 + 6);
            add(2 + 3);
            z[0xE0] = 4;
            for (;;) {
                add(6);
                copyIn();
                add(5);
                z[0x80] -= 1;
                if (z[0x80] != 0) {
                    add(branch(true, 0x46FA, 0x4711));
                } else {
                    add(2);
                    add(2 + 3);
                    z[0x80] = 0x18;
                    r7d8d();
                    r699e();
                    add(2 + 3 + 2 + 3);
                    z[0x70] -= 2;
                    r7d8d();
                    r88a2Chase();
                }
                add(6);
                copyOut();
                add(5);
                z[0xE0] -= 1;
                if (z[0xE0] != 0) {
                    add(branch(true, 0x4716, 0x46F3));
                } else {
                    add(2);
                    break;
                }
            }
            add(5);
            z[0x5E] -= 1;
            if (z[0x5E] != 0) {
                add(branch(true, 0x471C, 0x4742));
            } else {
                add(2);
                add(2 + 3);
                z[0x5E] = 0x60;
                add(3);
                if (z[0x5F] != 0) {
                    add(2);
                    add(2 + 3 + 2 + 3 + 6);
                    z[0x10] = 0x67;
                    z[0x0C] = 0x4B;
                    r61b6();
                    add(2);
                    add(branch(true, 0x4733, 0x4740));
                    z[0x5F] = 0;
                    add(3);
                } else {
                    add(branch(true, 0x4724, 0x4733));
                    add(2 + 3 + 2 + 3 + 6);
                    z[0x10] = 0x66;
                    z[0x0C] = 0x4A;
                    r7819();
                    add(2 + 3);
                    z[0x5F] = 1;
                }
            }
            add(3 + 2);
            if (z[0x60] < 0x4F) {
                add(branch(true, 0x4748, 0x474B));
                break;
            }
            add(2);
            add(3);
        }
        add(2 + 3 + 3 + 3 + 3 + 2 + 3 + 2 + 3 + 3);   // loop B: the ghosts are frightened and run to the right
        for (int g = 1; g <= 4; ++g) rec[g][8] = 1;
        z[0xE2] = 0xFF;
        z[0x68] = 2;
        z[0x62] = 2;
        bool done = false;
        while (!done) {
            add(5);
            z[0x59] -= 1;
            if (z[0x59] != 0) {
                add(branch(true, 0x4763, 0x477F));
            } else {
                add(2);
                add(6);
                rPoll();
                add(2 + 3);
                z[0x59] = 0x20;
                r7d80();
                r699e();
                add(2 + 3 + 2 + 3);
                z[0x60] += 2;
                r7d80();
                add(5);
                z[0x66] += 1;
                r79a4();
            }
            add(6 + 6);
            add(2 + 3);
            z[0xE0] = 4;
            for (;;) {
                add(6);
                copyIn();
                add(5);
                z[0x80] -= 1;
                if (z[0x80] != 0) {
                    add(branch(true, 0x478D, 0x47C0));
                } else {
                    add(2);
                    add(2 + 3);
                    z[0x80] = 0x40;
                    add(3);
                    if (z[0x79] != 0) {
                        add(branch(true, 0x4793, 0x47C0));
                    } else {
                        add(2);
                        add(2 + 3 + 3 + 2);
                        const int diff = (z[0x70] - z[0x60]) & 255;
                        if (diff >= 5) {
                            add(branch(true, 0x479E, 0x47AD));
                            r7d8d();
                            r699e();
                            add(2 + 3 + 2 + 3);
                            z[0x70] += 2;
                            r7d8d();
                            r88a2Chase();
                        } else {
                            add(2);
                            r467c();
                            add(3 + 2);
                            if (z[0x68] == 0x32) {
                                add(branch(true, 0x47A7, 0x47C9));
                                done = true;
                                break;
                            }
                            add(2);
                            add(2 + 3);
                            z[0x79] = 1;
                            add(branch(true, 0x47AD, 0x47C0));
                        }
                    }
                }
                add(6);
                copyOut();
                add(5);
                z[0xE0] -= 1;
                if (z[0xE0] != 0) {
                    add(branch(true, 0x47C5, 0x4786));
                } else {
                    add(2);
                    add(branch(true, 0x47C9, 0x475F));
                    break;
                }
            }
        }
        add(2 + 2 + 3 + 3 + 2 + 3 + 3 + 3);         // $47C9-$47D7: up to and including the JMP $5303
        out.length = t;
    }

    // ---- the set-up of a game screen: $531E, $5659, $5850, $53E6 (here with the demo's values: one player, level 1, no spare lives) ------------------------------------
    int mapCell(int index) const { return u8(0x65BA + index); }   // the pristine maze map (a game that is running would pass its own map)
    void r5331() {                                  // $5331: copy the maze map (896 bytes) from $65BA to $623A
        add(6);
        add(5 * 4 + 2);
        int p14 = 0x65BA, p16 = 0x623A;
        for (;;) {
            add(5 + 6);
            add(5);
            ++p16;
            add((p16 & 255) != 0 ? 3 : 2 + 5);
            add(5);
            ++p14;
            add((p14 & 255) != 0 ? 3 : 2 + 5);
            add(3 + 2);
            if ((p16 >> 8) != 0x65) {
                add(3);
                continue;
            }
            add(2);
            add(3 + 2);
            if ((p16 & 255) != 0xBA) {
                add(3);
                continue;
            }
            add(2);
            break;
        }
        add(6);
    }
    void r53be() {                                  // $53BE: save the player's data
        add(6);
        add(3 + 4 + cross(0x53E0, z[0x50]) + 2 + 8 * 16 - 1 + 6);
    }
    void r53cf() {                                  // JSR $53CF: load the player's data
        add(6 + 3 + 4 + cross(0x53E0, z[0x50]) + 2 + 8 * 15 - 1 + 6);
        if (savedPlayers) {
            for (int i = 0; i < 3; ++i) z[0x52 + i] = savedScore[static_cast<std::size_t>(z[0x50])][static_cast<std::size_t>(i)];
            z[0x58] = savedLives[static_cast<std::size_t>(z[0x50])];
        }
    }
    void r531e() {                                  // JSR $531E
        add(6 + 2 + 3);
        r5331();
        add(2 + 3 + 2 + 3);
        r53be();
        add(6);
    }
    void rVline() {                                 // $5D3D (the JSR is paid by the caller): one pixel per row from $10 to $11
        Busy window(*this);
        event(11, z[0x0C], z[0x10], z[0x11]);
        step(ScreenOp::Draw, DrawCall{13, {z[0x0C], z[0x10], z[0x11], 0, 0}});
        add(6);
        r5933();
        for (;;) {
            add(6);
            r5b58();
            add(3 + 3);
            add(6 + 4 + cross(0x5925, z[0x03]) + 5 + cross(u8(0x5B67 + z[0x10]), z[0x04]) + 6 + 6);
            add(5);
            z[0x10] += 1;
            add(3 + 3);
            if (z[0x11] >= z[0x10]) {
                add(3);
                continue;
            }
            add(2);
            break;
        }
        add(6);
    }
    void r5d53() {                                  // JSR $5D53: the maze walls from the table at $5F69 (72 horizontal and 70 vertical lines)
        add(6);
        add(2 + 3 + 3 + 2 + 2 + 3 + 2 + 3 + 3);
        z[0x1E] = 0x80;
        int ptr = 0x5F69;
        z[0x0D] = 0;
        add(2 + 3 + 2 + 3);
        for (int k = 0; k < 72; ++k) {
            add(2);
            const int lo = ptr & 255;
            z[0x0C] = u8(ptr);
            add(5 + cross(lo, 0) + 3 + 2);
            z[0x0E] = u8(ptr + 1);
            add(5 + cross(lo, 1) + 3 + 2);
            z[0x10] = u8(ptr + 2);
            add(5 + cross(lo, 2) + 3);
            add(6);
            rHline();
            add(2 + 2 + 3 + 3);
            add(lo + 3 > 255 ? 2 + 5 : 3);
            ptr += 3;
            add(5);
            add(k < 71 ? 3 : 2);
        }
        add(2 + 3);
        for (int k = 0; k < 70; ++k) {
            add(2);
            const int lo = ptr & 255;
            z[0x0C] = u8(ptr);
            add(5 + cross(lo, 0) + 3 + 2);
            z[0x10] = u8(ptr + 1);
            add(5 + cross(lo, 1) + 3 + 2);
            z[0x11] = u8(ptr + 2);
            add(5 + cross(lo, 2) + 3);
            add(6);
            rVline();
            add(2 + 3 + 2 + 3);
            add(lo + 3 > 255 ? 2 + 5 : 3);
            ptr += 3;
            add(5);
            add(k < 69 ? 3 : 2);
        }
        add(6);
    }
    void r8a7d() {                                  // JSR $8A7D: the two lines of the ghost-house door
        add(6);
        for (int y = 0x4D; y <= 0x4E; ++y) {
            add(15);
            z[0x0C] = 0x87;
            z[0x0E] = 0x93;
            z[0x10] = y;
            add(6);
            rHline();
        }
        add(6);
    }
    void r6113() {                                  // JSR $6113: the dots and energizers of the map
        add(6);
        add(15 + 10);
        int p16 = 0x623A;
        int z0e = 0x3B;
        int col = 0;
        for (;;) {
            int y11 = 0x0B;
            for (int row = 1; row <= 30; ++row) {
                add(3);
                const int cell = mapCell(col * 32 + row);
                add(5 + cross(p16 & 255, row));
                if (cell == 0) {
                    add(3);
                } else {
                    add(2);
                    add(2);
                    if (cell & 1) {
                        add(3);
                        add(3 + 3 + 3 + 3);
                        z[0x0C] = z0e;
                        z[0x10] = y11;
                        add(6);
                        rDot();
                    } else {
                        add(2);
                        add(3 + 3 + 5 + 3 + 3 + 5);
                        z[0x0C] = z0e - 1;
                        z[0x10] = y11 - 1;
                        add(6);
                        z[0x0D] = 0;
                        r61b6();
                        add(2 + 3);
                    }
                }
                add(2 + 3 + 2 + 3);
                y11 += 6;
                add(5 + 3 + 2);
                add(row + 1 < 0x1F ? 3 : 2);
            }
            add(3 + 2 + 3);
            const int v = (p16 & 255) + 0x1F + 1;
            p16 = (p16 & 0xFF00) | (v & 255);
            if (v > 255) {
                add(2 + 5);
                p16 += 0x100;
            } else {
                add(3);
            }
            add(2 + 3 + 2 + 3 + 2);
            z0e += 6;
            ++col;
            if (z0e < 0xDD) {
                add(3);
                add(10);
                continue;
            }
            add(2);
            break;
        }
        add(6);
    }
    void rHlineMid() {                              // JSR $5CF6: a dotted line, entered after "LDA #0 / STA $0D / STA $0F" (the caller has set them)
        Busy window(*this);
        step(ScreenOp::Draw, DrawCall{8, {z[0x0C], z[0x0E] | (z[0x0F] << 8), z[0x10], 2, z[0x1E]}});
        add(6 + 6);
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
    void r696a() {                                  // JSR $696A: clear the score box of player $50
        Busy window(*this);
        add(6);                                     // JSR $696A
        event(12, z[0x50]);
        step(ScreenOp::Draw, DrawCall{4, {z[0x50], 0, 0, 0, 0}});
        const int x = 2 * z[0x50];
        add(3 + 2 + 2 + 4 + cross(0x6992, x) + 3 + 4 + cross(0x6993, x) + 3);
        z[0x04] = u8(0x6992 + x);
        z[0x10] = u8(0x6993 + x);
        add(2 + 3);
        z[0x18] = 12;
        for (;;) {
            add(6);
            r5b58();
            add(3 + 2 + 2);
            for (int b = 0; b < 8; ++b) add(6 + 2 + 2 + (b < 7 ? 3 : 2));
            add(5 + 5);
            z[0x10] += 1;
            z[0x18] -= 1;
            if (z[0x18] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    void r70f5() {                                  // $70F5: the digits of the score ($05-$07), right to left
        for (;;) {
            add(2 + 2);
            int a = 0;
            for (int k = 0; k < 4; ++k) {
                add(2 + 5 + 5 + 5 + 2 + 2);
                const int c = z[0x07] & 1;
                z[0x07] >>= 1;
                const int c2 = z[0x06] & 1;
                z[0x06] = (c << 7) | (z[0x06] >> 1);
                const int c3 = z[0x05] & 1;
                z[0x05] = (c2 << 7) | (z[0x05] >> 1);
                a = (c3 << 7) | (a >> 1);
            }
            add(3 * branch(true, 0x7104, 0x70F9) + 2);
            add(2 + 2 + 2 + 2 + 2);
            a >>= 4;
            add(6);
            r7159(a);
            add(2 + 3 + 2 + 3);
            const int v = z[0x0E] - 6;
            z[0x0E] = v & 255;
            if (v < 0) {
                add(2 + 5);
                z[0x0F] -= 1;
            } else {
                add(3);
            }
            add(3);
            if (z[0x07] != 0) {
                add(branch(true, 0x711B, 0x70F5));
                continue;
            }
            add(2 + 3);
            if (z[0x06] != 0) {
                add(branch(true, 0x711F, 0x70F5));
                continue;
            }
            add(2 + 3);
            if (z[0x05] != 0) {
                add(branch(true, 0x7123, 0x70F5));
                continue;
            }
            add(2);
            break;
        }
        add(6);
    }
    void r70cc() {                                  // JSR $70CC: erase the score box and draw the digits
        add(6);
        add(3 + 3 + 3 + 3 + 3 + 3);
        z[0x05] = z[0x52];
        z[0x06] = z[0x53];
        z[0x07] = z[0x54];
        r696a();
        const int x = 4 * z[0x50];
        add(3 + 2 + 2 + 2 + (4 + cross(0x7141, x) + 3) + (4 + cross(0x7142, x) + 3) + (4 + cross(0x7143, x) + 3));
        z[0x0E] = u8(0x7141 + x);
        z[0x0F] = u8(0x7142 + x);
        z[0x11] = u8(0x7143 + x);
        add(6);
        r70f5();
        add(3);
        add(3);
        if (z[0x50] == 0) {
            add(3);
        } else if (savedPlayers && (z[0x58] & 0x80) != 0) {
            add(2 + 3 + 2 + 3 + 2 + 3);             // BEQ not taken, BIT $58, BPL not taken (the bonus life is still to come), LDA $54, CMP #1, BCC taken (the score is below 10,000)
        } else {
            add(2 + 3 + 3);                         // BEQ not taken, BIT $58, BPL $7140 (no bonus life in the attract game)
        }
        add(6);
    }
    void r556e(int x) {                             // JSR $556E: the label of a score
        add(6);
        for (int k = 0; k < 5; ++k) add(4 + cross(0x55C7 + k, x) + 3);
        z[0x04] = u8(0x55C7 + x);
        z[0x10] = u8(0x55C8 + x);
        z[0x0E] = u8(0x55C9 + x);
        z[0x0F] = u8(0x55CA + x);
        z[0x11] = u8(0x55CB + x);
        add(2 + 3 + 2 + 3 + 3 + 2 + 4 + 2 + 4);
        z[0x03] = 0;
        z[0x18] = 7;
        z[0x1A] = 7;
        p55b2 = mem5592 | (mem5597 << 8);
        add(6);
        rBlit();
        add(3);
        if (z[0x50] == 0) {
            add(3);
        } else {
            add(2);
            add(6);
            r7159(z[0x50]);
        }
        add(6);
    }
    void r550a() {                                  // JSR $550A: the labels, scores and the high score
        add(6);
        mem5592 = 0xF7;
        mem5597 = 0x55;
        add(3 + 3);
        z[0x50] = z[0x51];
        for (;;) {
            add(3 + 2 + 2 + 2 + 2);
            const int x = 8 * z[0x50];
            add(4 + cross(0x55CC, x) + 3 + 4 + cross(0x55CD, x) + 3);
            z[0x0B] = u8(0x55CC + x);
            z[0x0A] = u8(0x55CD + x);
            r556e(x);
            add(2 + 3 + 3);
            z[0x0D] = 0;
            z[0x0F] = 0;
            add(3 + 3 + 2 + 2 + 3);
            z[0x0C] = z[0x0B];
            const int v = z[0x0B] + 0x31;
            z[0x0E] = v & 255;
            if (v > 255) {
                add(2 + 5);
                z[0x0F] += 1;
            } else {
                add(3);
            }
            rHlineMid();
            add(3);
            if (z[0x50] == 0) {
                add(3);
            } else {
                add(2);
                r53cf();
            }
            r70cc();
            add(5);
            z[0x50] = (z[0x50] - 1) & 255;
            if (z[0x50] & 0x80) {
                add(3);
                break;
            }
            add(2);
            if (z[0x50] == 0) {
                add(3);
                add(2 + 4 + 2 + 4 + 3 + 3 + 3 + 3 + 3 + 3 + 3);
                mem5592 = 0x28;
                mem5597 = 0x56;
                z[0x52] = z[0x00];
                z[0x53] = z[0x01];
                z[0x54] = z[0x02];
            } else {
                add(2);
                add(3);
            }
        }
        add(2 + 4 + 2 + 4 + 6);
        mem5592 = 0xF7;
        mem5597 = 0x55;
    }
    void r6f13() {                                  // JSR $5692 -> JMP $6F13: the level's fruit as a 12 x 3 image
        add(6 + 3);
        add(3 + 2 + 3);
        const int lvl = std::min(z[0x55], 0x1A);
        add(2 + 2 + 2 + 2 + 3 + 2 + 3);
        const int x = 4 * lvl;
        add(4 + cross(0x6AFE, x) + 4 + cross(0x6AFF, x) + 3);
        const int a = u8(0x6AFE + x), y = u8(0x6AFF + x);
        z[0x10] = 0xB0;
        z[0x04] = 0x21;
        event(13, a | (y << 8), z[0x04], z[0x10]);
        step(ScreenOp::Draw, DrawCall{7, {a | (y << 8), z[0x04], z[0x10], 0, 0}});
        const int p6f00 = a | (y << 8);
        add(4 + 4 + 2 + 3 + 2 + 3);
        z[0x18] = 12;
        z[0x03] = 0;
        for (;;) {
            add(6);
            const int lo12 = r5b58();
            add(3 + 3 + 2 + 3);
            int xx = z[0x03];
            int yy = z[0x04];
            for (int b = 0; b < 3; ++b) {
                add(5 + cross(lo12, yy) + 2 + 4 + cross(p6f00, xx) + 6 + 2 + 2 + 5);
                add(branch(b < 2, 0x6F0A, 0x6EFB));
                ++xx;
                ++yy;
            }
            z[0x03] = xx;
            add(3 + 5 + 5);
            z[0x10] += 1;
            z[0x18] -= 1;
            if (z[0x18] != 0) {
                add(branch(true, 0x6F12, 0x6EF0));
            } else {
                add(2);
                break;
            }
        }
        add(6);
    }
    void r5659() {                                  // JSR $5659 up to the fruit: page clear, walls, door, dots, labels and scores
        add(6);                                     // JSR $5659
        add(6);                                     // JSR $6F46
        event(1);
        step(ScreenOp::WipeStart, {}, 0);
        add(90575);
        add(4);                                     // STA $C010
        add(6 + 4 + 4 + 4 + 4 + 6);
        step(ScreenOp::ShowHires);
        r5d53();
        r8a7d();
        add(3 + 3);
        z[0xF1] = z[0x50];
        r53cf();
        r6113();
        r550a();
        add(3 + 3);
        z[0x50] = z[0xF1];
        r53cf();
        r6f13();
        add(6 + 3 + 2 + 3 + 3 + 3 + 6);             // JSR $56CF (no life icons in the demo)
        add(3 + 3 + 6);                             // LDA $E8, BNE $5691, RTS
    }
    void r5839() {                                  // JSR $5839: Pac-Man's start image
        add(6 + 5 + 5 + 5 + 5 + 4 + 3);
        z[0x0C] = 0x8B;
        z[0x10] = 0x8F;
        z[0x0D] = 0;
        z[0x1E] = 0;
        rDispatch(0xFA, 0x96);
    }
    void r80e3() {                                  // JMP $80E3 (attract game): the words GAME OVER
        add(3);
        const int cols[2] = {0x0F, 0x15};
        const int datas[2] = {0x811D, 0x8139};
        for (int k = 0; k < 2; ++k) {
            add(2 + 2 + 2 + 6 + 3 + 3 + 3 + 2 + 3 + 6);
            z[0x10] = 0x69;
            z[0x1A] = 4;
            z[0x04] = cols[k];
            z[0x03] = 0;
            add(2 + 2 + 2 + (k == 0 ? 6 : 3) + 3 + 4 + 4);
            z[0x18] = 7;
            p55b2 = datas[k];
            add(3);
            rBlit();
        }
    }
    void r5850() {
        add(6);
        r5839();
        add(3 + 2);
        r80e3();
    }
    void r6e79() {                                  // JSR $6E79: the ghost queues
        add(2 + 15 * 14 - 1);
        add(2 + 2);
        for (int k = 0; k < 128; ++k) {
            add(2 + 5 + 5 + 5 + 2 + 2 + 2 + 5 + 5 + 5 + 2 + 2);
            add(k < 127 ? 3 : 2);
        }
        add(6);
    }
    void r53e6() {                                  // JSR $53E6: the start of a life
        add(6);
        r5839();
        add(2 + 14 * (4 + 4 + 2) + 13 * 3 + 2);
        add(2);
        add(84 * (4 + 5 + 2) + 83 * 3 + 2);
        add(2 + 2 + 7 * (5 + 2) + 6 * 3 + 2);
        add(6);
        r6e79();
        r53cf();
        add(2 + 3 + 2 + 3 + 2 + 3 + 3 + 3 + 3);
        add(6 + 3 + 2 + 3 + 6);
        add(2 + 2);
        add(4 + 5 + 4 + 5 + 4 + 5);
        add(2);
        for (int k = 0; k < 4; ++k) {
            add(2 + 6);
            rDelay(255);
            add(2 + (k < 3 ? 3 : 2));
        }
        add(6);
        rPoll();
        add(6);
    }
    void runDemoSetup() {                           // $5303-$530C: the attract game's screen; ends where the first tick starts ($530F)
        z[0x50] = 1;
        z[0x51] = 1;
        z[0x55] = 1;
        z[0x58] = 0;
        z[0xE8] = 1;
        mem5592 = 0xF7;
        mem5597 = 0x55;
        r531e();
        r5659();
        r5850();
        r53e6();
        out.length = t;
    }
    int mem5592 = 0xF7, mem5597 = 0x55;             // the patched immediates of the label routine

    // ---- the death sequence of the attract game: $8BDD-$8C0B, $7EC0-$7F04 (E8 <> 0: no sound) ---------------------------------------------------------------------------
    void r556eJmp(int x) {                          // $556E entered by a JMP ($58FA): the label of the player
        const int before = 0;
        (void)before;
        r556e(x);
        // (r556e starts with the 6 cycles of a JSR: the JMP has none)
    }
    void runDemoDeath(const std::array<std::pair<int, int>, 4>& ghosts, int pacX, int pacY, int caughtGhost) {
        for (int g = 1; g <= 4; ++g) {
            rec[g][0] = ghosts[static_cast<std::size_t>(g - 1)].first;
            rec[g][1] = ghosts[static_cast<std::size_t>(g - 1)].second;
        }
        z[0x60] = pacX;
        z[0x61] = pacY;
        z[0xE8] = 1;
        z[0x50] = 1;
        z[0x51] = 1;
        z[0x1D] = 1;
        z[0xE0] = caughtGhost;
        z[0x70] = ghosts[static_cast<std::size_t>(caughtGhost - 1)].first;   // the working area holds the ghost that touched Pac-Man
        z[0x71] = ghosts[static_cast<std::size_t>(caughtGhost - 1)].second;
        mem5592 = 0xF7;
        mem5597 = 0x55;
        add(6);
        copyOut();                                  // JSR $8712
        add(6 + 3 + 2 + 2 + 2 + 2 + 3);             // JSR $58FA: LDA $50, ASL x3, TAX, JMP $556E
        add(-6);
        r556e(8 * z[0x50]);
        add(2);                                     // LDY #3
        for (int k = 0; k < 3; ++k) {
            add(2 + 6);
            rDelay(250);
            add(2 + (k < 2 ? 3 : 2));
        }
        add(2 + 3);                                 // LDA #4, STA $E0
        z[0xE0] = 4;
        for (;;) {
            add(6);
            copyIn();
            r7d8d();
            r699e();
            add(5);
            z[0xE0] -= 1;
            if (z[0xE0] != 0) {
                add(3);
            } else {
                add(2);
                break;
            }
        }
        r7d80();
        r699e();
        add(6);                                     // JSR $7EC0
        r7d80();
        add(2 + 2 + 6);                             // LDA #$E6, LDY #$9B, JSR $6A20
        rDispatch(0xE6, 0x9B);
        add(2);
        for (int k = 0; k < 3; ++k) {
            add(2 + 6);
            rDelay(255);
            add(2 + (k < 2 ? 3 : 2));
        }
        r7d80();
        r699e();
        add(2 + 3);                                 // LDA #6, STA $0A
        z[0x0A] = 6;
        for (;;) {
            add(3 + 2 + 2);
            const int x = 2 * z[0x0A];
            r7d80();
            const int lo = u8(0x7F05 + x), hi = u8(0x7F06 + x);
            add(4 + cross(0x7F05, x) + 4 + cross(0x7F06, x) + 3 + 3);
            z[0x16] = lo;
            z[0x17] = hi;
            r7e1f();
            add(6 + 3 + 2 + 2 + 4 + 4 + 3);         // JSR $7C82 ... JMP $7BBC
            add(4 + 4 + 3);
            add(2 + 6);                             // the demo: BEQ not taken, RTS (no sound)
            add(2 + 6);                             // LDX #$80, JSR $6CE8
            rDelay(0x80);
            r7d80();
            r69e9();
            add(5);
            z[0x0A] -= 1;
            if (z[0x0A] != 0) {
                add(branch(true, 0x7F04, 0x7EDE));
            } else {
                add(2);
                break;
            }
        }
        add(6);                                     // RTS of $7EC0
        add(3 + 2 + 3);                             // LDA $E8, BEQ (not taken), JMP $43D2
        add(2 + 3 + 6);                             // $43D2: LDA #1, STA $E8, JSR $52BB
        out.length = t;
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
            out.spaceChecks.push_back({t, 12});
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

std::int64_t drawCallCycles(const DrawCall& c, const datapack::Pack& pack) {
    ScreenScript scratch;
    Builder b(pack, scratch, false);
    const int x = c.p[1];
    const std::int64_t t0 = b.t;
    switch (c.type) {
        case 1:
            b.z[0x0C] = x & 255;
            b.z[0x0D] = x >> 8;
            b.z[0x10] = c.p[2];
            b.z[0x1D] = c.p[3];
            b.z[0x1E] = c.p[4];
            b.rDispatch(c.p[0] & 255, c.p[0] >> 8);
            break;
        case 2:
            b.p55b2 = c.p[0];
            b.z[0x04] = c.p[1];
            b.z[0x10] = c.p[2];
            b.z[0x1A] = c.p[3];
            b.z[0x18] = c.p[4];
            b.z[0x03] = 0;
            b.rBlit();
            break;
        case 3:
            b.p69b8 = c.p[0];
            b.z[0x0C] = x & 255;
            b.z[0x0D] = x >> 8;
            b.z[0x10] = c.p[2];
            b.rMclearBody();
            break;
        case 4:
        case 10:
            b.z[0x50] = c.type == 4 ? c.p[0] : c.p[0] + 1;
            b.r696a();
            break;
        case 5:
            b.z[0x0C] = c.p[0] & 255;
            b.z[0x0D] = c.p[0] >> 8;
            b.z[0x10] = c.p[1];
            b.rDot();
            break;
        case 6:
            b.p6951 = c.p[0];
            b.z[0x0C] = x & 255;
            b.z[0x0D] = x >> 8;
            b.z[0x10] = c.p[2];
            b.rTile();
            break;
        case 9:
            b.z[0x16] = c.p[0] & 255;
            b.z[0x17] = c.p[0] >> 8;
            b.z[0x0C] = x & 255;
            b.z[0x0D] = x >> 8;
            b.z[0x10] = c.p[2];
            b.z[0x1D] = c.p[3];
            b.z[0x1E] = c.p[4];
            b.r7e1f();
            break;
        case 11:
            b.z[0x0C] = c.p[0] & 255;
            b.z[0x0D] = c.p[0] >> 8;
            b.z[0x10] = c.p[1];
            b.r7819();
            break;
        default:
            break;
    }
    return b.t - t0;
}

std::int64_t tickCostCycles(const std::vector<DrawCall>& calls, const datapack::Pack& pack, bool demo, bool ate, bool energizer) {
    const std::int64_t idle = demo ? kIdleTickCyclesDemo : kIdleTickCyclesGame;
    if (calls.empty() && !ate && !energizer) return idle;
    std::int64_t c = idle + 12;   // 2,939 for a tick that draws (fitted)
    for (const auto& d : calls) {
        c += drawCallCycles(d, pack);
        switch (d.type) {
            case 1: c += -106; break;
            case 2: c += 66; break;
            case 3: c += 609; break;
            case 5: c += 134; break;
            case 6: c += 92; break;
            case 10: c += 40; break;
            case 11: c += 59; break;
            default: break;
        }
    }
    if (ate) c += 144;
    if (energizer) c += 5'650;
    return c;
}

std::int64_t hudCycles(const datapack::Pack& pack, int players, const std::array<std::uint32_t, 6>& scores, const std::array<int, 6>& lives, std::uint32_t high) {
    ScreenScript scratch;
    Builder b(pack, scratch, false);
    b.savedPlayers = true;
    for (int p = 1; p <= 5; ++p) {
        const std::uint32_t s = scores[static_cast<std::size_t>(p)];
        b.savedScore[static_cast<std::size_t>(p)] = {static_cast<int>(s & 255), static_cast<int>((s >> 8) & 255), static_cast<int>((s >> 16) & 255)};
        b.savedLives[static_cast<std::size_t>(p)] = lives[static_cast<std::size_t>(p)];
    }
    b.z[0x00] = static_cast<int>(high & 255);
    b.z[0x01] = static_cast<int>((high >> 8) & 255);
    b.z[0x02] = static_cast<int>((high >> 16) & 255);
    b.z[0x51] = players;
    b.r550a();
    return b.t;
}

std::int64_t hudExtraCycles(const datapack::Pack& pack, int players, int current, const std::array<std::uint32_t, 6>& scores, const std::array<int, 6>& lives, std::uint32_t high) {
    if (players <= 1) return 0;
    std::array<std::uint32_t, 6> one{};
    std::array<int, 6> oneLives{};
    one[1] = scores[static_cast<std::size_t>(current)];
    oneLives[1] = lives[static_cast<std::size_t>(current)];
    return hudCycles(pack, players, scores, lives, high) - hudCycles(pack, 1, one, oneLives, high);
}

bool buildDemoDeathScript(const datapack::Pack& pack, ScreenScript& out, const std::array<std::pair<int, int>, 4>& ghosts, int pacX, int pacY, int caughtGhost, bool withTrace) {
    static const struct { std::uint16_t address; std::size_t length; } need[] = {
        {0x5958, 512}, {0x5B67, 384}, {0x717D, 24}, {0x7812, 7}, {0x7843, 14}, {0x88E1, 5}, {0x96E8, 4888}, {0x61CA, 1904}, {0x870D, 5}, {0x7FC7, 120}, {0x69CD, 28}, {0x6A04, 28},
        {0x55C7, 146}, {0x7F05, 14}, {0x5925, 7},
    };
    for (const auto& n : need)
        if (pack.find(n.address, n.length).empty()) return false;
    out = ScreenScript{};
    Builder b(pack, out, withTrace);
    b.runDemoDeath(ghosts, pacX, pacY, caughtGhost);
    return true;
}

bool buildAttractScript(const datapack::Pack& pack, ScreenScript& out, bool withTrace) {
    static const struct { std::uint16_t address; std::size_t length; } need[] = {
        {0x434F, 131}, {0x450A, 370}, {0x4A7A, 2022}, {0x5958, 512}, {0x5B67, 384}, {0x717D, 24}, {0x7812, 7}, {0x7843, 14}, {0x88E1, 5}, {0x96E8, 4888}, {0x61A8, 14}, {0x61CA, 1904},
        {0x870D, 5}, {0x7FC7, 120}, {0x69CD, 28}, {0x6A04, 28}, {0x65BA, 896}, {0x5F69, 426}, {0x55C7, 146}, {0x7141, 24}, {0x6992, 12}, {0x6AFE, 436}, {0x5925, 7},
    };
    for (const auto& n : need)
        if (pack.find(n.address, n.length).empty()) return false;
    out = ScreenScript{};
    Builder b(pack, out, withTrace);
    b.run();
    b.runChase();
    b.runDemoSetup();
    return true;
}

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
void ScreenPlayer::start(const ScreenScript* script, const datapack::Pack* pack, const hires::Page* initial) {
    script_ = script;
    pack_ = pack;
    next_ = 0;
    now_ = 0;
    page_ = initial != nullptr ? *initial : hires::Page();
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

// ---- one turn of the attract loop ----------------------------------------------------------------------------------------------------------------------------------------
bool AttractTurn::start(const datapack::Pack& pack) {
    pack_ = &pack;
    if (!buildAttractScript(pack, intro_, false)) return false;
    introLength_ = intro_.length;
    if (!driver_.init(pack)) return false;
    introPlayer_.start(&intro_, &pack);
    phase_ = Phase::Intro;
    now_ = 0;
    demoTick_ = 0;
    tickRunning_ = false;
    length_ = 0;
    return true;
}

const hires::Page& AttractTurn::hires() const noexcept {
    switch (phase_) {
        case Phase::Intro: return introPlayer_.hires();
        case Phase::Demo: return shown_;
        default: return deathPlayer_.hires();
    }
}

void AttractTurn::beginDemo() {
    driver_.newDemo(introPlayer_.hires());
    shown_ = introPlayer_.hires();
    phase_ = Phase::Demo;
    tickStart_ = introLength_;
    tickRunning_ = false;
    demoTick_ = 0;
    completed_ = 0;
}

void AttractTurn::startTick() {
    const TickEvents ev = driver_.tick();
    ++demoTick_;
    pending_ = driver_.page();
    tickWasLast_ = ev.demoOver;
    const bool eaten = ev.ghostEaten[0] || ev.ghostEaten[1] || ev.ghostEaten[2] || ev.ghostEaten[3];
    tickEnd_ = tickStart_ + (ev.demoOver ? kDemoCatchTickCycles : eaten ? kDemoGhostEatenTickCycles : driver_.tickCycles() + driver_.extraCycles());
    tickRunning_ = true;
}

void AttractTurn::beginDeath() {
    const GameState& c = driver_.core().caughtState();
    std::array<std::pair<int, int>, 4> ghosts{};
    for (std::size_t g = 0; g < 4; ++g) ghosts[g] = {c.ghosts[g].x(), c.ghosts[g].y()};
    buildDemoDeathScript(*pack_, death_, ghosts, c.pacX(), c.pacY(), 1, false);
    deathStart_ = tickEnd_;
    deathPlayer_.start(&death_, pack_, &pending_);
    length_ = deathStart_ + death_.length;
    phase_ = Phase::Death;
}

void AttractTurn::advance(std::int64_t cycles) {
    const std::int64_t target = now_ + cycles;
    if (phase_ == Phase::Intro) {
        const std::int64_t to = std::min(target, introLength_);
        introPlayer_.advance(to - introPlayer_.now());
        now_ = to;
        if (now_ >= introLength_) beginDemo();
    }
    while (phase_ == Phase::Demo) {
        if (!tickRunning_) {
            if (tickStart_ > target) break;
            startTick();
        }
        if (tickEnd_ > target) break;
        // the tick is over: its picture is shown
        shown_ = pending_;
        ++completed_;
        tickRunning_ = false;
        tickStart_ = tickEnd_;
        if (tickWasLast_) {
            beginDeath();
        }
    }
    if (phase_ == Phase::Death) {
        const std::int64_t to = std::min(target, length_);
        deathPlayer_.advance(to - deathStart_ - deathPlayer_.now());
        if (to >= length_) phase_ = Phase::Done;
        now_ = to;
        return;
    }
    now_ = target;
}

bool AttractTurn::nextCheck(std::int64_t after, KeyCheck& out) const {
    if (phase_ == Phase::Intro) {
        const auto& polls = intro_.polls;
        const auto& spaces = intro_.spaceChecks;
        const auto p = std::upper_bound(polls.begin(), polls.end(), after);
        const auto l = std::upper_bound(spaces.begin(), spaces.end(), after, [](std::int64_t v, const std::pair<std::int64_t, int>& e) { return v < e.first; });
        if (p != polls.end() && (l == spaces.end() || *p <= l->first)) {
            out = {*p, 0};
            return true;
        }
        if (l != spaces.end()) {
            out = {l->first, l->second};
            return true;
        }
        out = {std::max(after + 1, introLength_ + pollOffset(0)), 0};   // the poll of the first tick
        return true;
    }
    if (phase_ == Phase::Demo) {
        if (!tickRunning_) {
            const std::int64_t at = tickStart_ + pollOffset(demoTick_);
            if (at <= after) return false;   // (cannot happen: the tick of a poll that has been passed is running)
            out = {at, 0};
            return true;
        }
        const std::int64_t own = tickStart_ + pollOffset(demoTick_ - 1);
        if (own > after) {
            out = {own, 0};
            return true;
        }
        if (tickWasLast_) return false;
        out = {tickEnd_ + pollOffset(demoTick_), 0};
        return true;
    }
    return false;
}

bool AttractTurn::runToDemoTick(std::size_t k) {
    while (phase_ == Phase::Intro) advance(introLength_ - now_);
    while (phase_ == Phase::Demo && completed_ < k) advance(2000);   // (a tick costs more than 3,000 cycles: one step completes at most one tick)
    return phase_ == Phase::Demo && completed_ == k;
}

}  // namespace pacman::core
