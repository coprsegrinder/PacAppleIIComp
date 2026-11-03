#include "cpu6502.h"

namespace pacman::core {

namespace {
enum Mn : int { ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA };
enum Md : int { ABS, ABX, ABY, ACC, IMM, IMP, IND, IZX, IZY, REL, ZP, ZPX, ZPY };
struct Op {
    int mn, mode;
};
constexpr Op kOps[256] = {
#include "cpu6502_table.inc"
};
constexpr std::uint8_t N = 0x80, V = 0x40, B = 0x10, D = 0x08, I = 0x04, Z = 0x02, C = 0x01;

bool isRead(int mn) { return mn == ADC || mn == AND || mn == CMP || mn == CPX || mn == CPY || mn == EOR || mn == LDA || mn == LDX || mn == LDY || mn == ORA || mn == SBC || mn == BIT; }
bool isWrite(int mn) { return mn == STA || mn == STX || mn == STY; }
bool isRmw(int mn) { return mn == ASL || mn == LSR || mn == ROL || mn == ROR || mn == INC || mn == DEC; }
int baseCycles(int mode) {
    switch (mode) {
        case IMM: return 2;
        case ZP: return 3;
        case ZPX: case ZPY: case ABS: case ABX: case ABY: return 4;
        case IZX: return 6;
        case IZY: return 5;
        default: return 2;
    }
}
int rmwCycles(int mode) { return mode == ZP ? 5 : mode == ZPX ? 6 : mode == ABS ? 6 : 7; }
}  // namespace

std::uint8_t Cpu6502::setnz(int v) noexcept {
    const std::uint8_t b = static_cast<std::uint8_t>(v);
    p = static_cast<std::uint8_t>((p & ~(N | Z)) | (b & N) | (b == 0 ? Z : 0));
    return b;
}

void Cpu6502::adc(std::uint8_t m) noexcept {
    const int c = p & C;
    if (p & D) {
        int al = (a & 0xF) + (m & 0xF) + c;
        if (al >= 0xA) al = ((al + 6) & 0xF) + 0x10;
        int r = (a & 0xF0) + (m & 0xF0) + al;
        flag(N, (r & 0x80) != 0);
        flag(V, ((a ^ r) & ~(a ^ m) & 0x80) != 0);
        flag(Z, ((a + m + c) & 0xFF) == 0);
        if (r >= 0xA0) r += 0x60;
        flag(C, r >= 0x100);
        a = static_cast<std::uint8_t>(r);
    } else {
        const int r = a + m + c;
        flag(V, ((a ^ r) & (m ^ r) & 0x80) != 0);
        flag(C, r > 0xFF);
        a = setnz(r);
    }
}

void Cpu6502::sbc(std::uint8_t m) noexcept {
    const int c = p & C;
    const int r = a - m - (1 - c);
    flag(V, ((a ^ m) & (a ^ r) & 0x80) != 0);
    flag(C, r >= 0);
    if (p & D) {
        int al = (a & 0xF) - (m & 0xF) + c - 1;
        if (al < 0) al = ((al - 6) & 0xF) - 0x10;
        int r2 = (a & 0xF0) - (m & 0xF0) + al;
        if (r2 < 0) r2 -= 0x60;
        setnz(r);
        a = static_cast<std::uint8_t>(r2);
    } else {
        a = setnz(r);
    }
}

void Cpu6502::compare(std::uint8_t reg, std::uint8_t m) noexcept {
    const int r = reg - m;
    flag(C, r >= 0);
    setnz(r);
}

std::uint8_t Cpu6502::rmw(int mn, std::uint8_t v) noexcept {
    const int c = p & C;
    switch (mn) {
        case ASL: flag(C, (v & 0x80) != 0); return static_cast<std::uint8_t>(v << 1);
        case LSR: flag(C, (v & 1) != 0); return static_cast<std::uint8_t>(v >> 1);
        case ROL: flag(C, (v & 0x80) != 0); return static_cast<std::uint8_t>((v << 1) | c);
        case ROR: flag(C, (v & 1) != 0); return static_cast<std::uint8_t>((v >> 1) | (c << 7));
        case INC: return static_cast<std::uint8_t>(v + 1);
        default: return static_cast<std::uint8_t>(v - 1);
    }
}

int Cpu6502::branch(int mn) {
    const std::uint8_t off = fetch();
    std::uint8_t mask = 0, want = 0;
    switch (mn) {
        case BCC: mask = C; want = 0; break;
        case BCS: mask = C; want = C; break;
        case BNE: mask = Z; want = 0; break;
        case BEQ: mask = Z; want = Z; break;
        case BPL: mask = N; want = 0; break;
        case BMI: mask = N; want = N; break;
        case BVC: mask = V; want = 0; break;
        default: mask = V; want = V; break;
    }
    int cyc = 2;
    if ((p & mask) == want) {
        const std::uint16_t t = static_cast<std::uint16_t>(pc + static_cast<std::int8_t>(off));
        cyc += 1 + (((t & 0xFF00) != (pc & 0xFF00)) ? 1 : 0);
        pc = t;
    }
    cycles += cyc;
    return cyc;
}

int Cpu6502::implied(int mn, bool accumulator) {
    int cyc = 2;
    if (accumulator) {
        a = setnz(rmw(mn, a));
    } else {
        switch (mn) {
            case NOP: break;
            case TAX: x = setnz(a); break;
            case TAY: y = setnz(a); break;
            case TXA: a = setnz(x); break;
            case TYA: a = setnz(y); break;
            case TSX: x = setnz(sp); break;
            case TXS: sp = x; break;
            case INX: x = setnz(x + 1); break;
            case INY: y = setnz(y + 1); break;
            case DEX: x = setnz(x - 1); break;
            case DEY: y = setnz(y - 1); break;
            case CLC: flag(C, false); break;
            case SEC: flag(C, true); break;
            case CLD: flag(D, false); break;
            case SED: flag(D, true); break;
            case CLI: flag(I, false); break;
            case SEI: flag(I, true); break;
            case CLV: flag(V, false); break;
            case PHA: push(a); cyc = 3; break;
            case PHP: push(static_cast<std::uint8_t>(p | B | 0x20)); cyc = 3; break;
            case PLA: a = setnz(pull()); cyc = 4; break;
            case PLP: p = static_cast<std::uint8_t>((pull() & ~B) | 0x20); cyc = 4; break;
            case RTS: {
                const std::uint8_t lo = pull();
                const std::uint8_t hi = pull();
                pc = static_cast<std::uint16_t>((lo | (hi << 8)) + 1);
                cyc = 6;
                break;
            }
            case RTI: {
                p = static_cast<std::uint8_t>((pull() & ~B) | 0x20);
                const std::uint8_t lo = pull();
                const std::uint8_t hi = pull();
                pc = static_cast<std::uint16_t>(lo | (hi << 8));
                cyc = 6;
                break;
            }
            case BRK: {
                fetch();
                push(static_cast<std::uint8_t>(pc >> 8));
                push(static_cast<std::uint8_t>(pc & 0xFF));
                push(static_cast<std::uint8_t>(p | B | 0x20));
                flag(I, true);
                pc = static_cast<std::uint16_t>(rd(0xFFFE) | (rd(0xFFFF) << 8));
                cyc = 7;
                break;
            }
            default: failed_ = true; break;
        }
    }
    cycles += cyc;
    return cyc;
}

int Cpu6502::step() {
    const std::uint8_t opcode = fetch();
    const Op op = kOps[opcode];
    if (op.mn < 0) {
        failed_ = true;
        return 0;
    }
    const int mn = op.mn, mode = op.mode;
    if (mn == BCC || mn == BCS || mn == BEQ || mn == BMI || mn == BNE || mn == BPL || mn == BVC || mn == BVS) return branch(mn);
    if (mode == IMP || mode == ACC) return implied(mn, mode == ACC);
    if (mode == IND) {                                  // JMP (addr) with the NMOS page-wrap bug
        const std::uint8_t lo = fetch();
        const std::uint16_t addr = static_cast<std::uint16_t>(lo | (fetch() << 8));
        pc = static_cast<std::uint16_t>(rd(addr) | (rd(static_cast<std::uint16_t>((addr & 0xFF00) | ((addr + 1) & 0xFF))) << 8));
        cycles += 5;
        return 5;
    }
    std::uint16_t ea = 0;
    std::uint8_t m = 0;
    bool haveEa = false;
    int cyc = 2;
    if (mode == IMM) {
        m = fetch();
    } else {
        bool cross = false;
        switch (mode) {
            case ZP: ea = fetch(); break;
            case ZPX: ea = static_cast<std::uint8_t>(fetch() + x); break;
            case ZPY: ea = static_cast<std::uint8_t>(fetch() + y); break;
            case ABS: {
                const std::uint8_t lo = fetch();
                ea = static_cast<std::uint16_t>(lo | (fetch() << 8));
                break;
            }
            case ABX:
            case ABY: {
                const std::uint8_t lo = fetch();
                const std::uint16_t base = static_cast<std::uint16_t>(lo | (fetch() << 8));
                ea = static_cast<std::uint16_t>(base + (mode == ABX ? x : y));
                cross = (base & 0xFF00) != (ea & 0xFF00);
                break;
            }
            case IZX: {
                const std::uint8_t z = static_cast<std::uint8_t>(fetch() + x);
                ea = static_cast<std::uint16_t>(rd(z) | (rd(static_cast<std::uint8_t>(z + 1)) << 8));
                break;
            }
            default: {                                  // IZY
                const std::uint8_t z = fetch();
                const std::uint16_t base = static_cast<std::uint16_t>(rd(z) | (rd(static_cast<std::uint8_t>(z + 1)) << 8));
                ea = static_cast<std::uint16_t>(base + y);
                cross = (base & 0xFF00) != (ea & 0xFF00);
                break;
            }
        }
        haveEa = true;
        if (mn == JMP) {
            pc = ea;
            cycles += 3;
            return 3;
        }
        cyc = baseCycles(mode);
        if (isRead(mn) && cross) ++cyc;
        if (isWrite(mn) && (mode == ABX || mode == ABY || mode == IZY)) ++cyc;
        if (isRmw(mn)) cyc = rmwCycles(mode);
    }
    auto val = [&]() -> std::uint8_t { return haveEa ? rd(ea) : m; };
    switch (mn) {
        case JSR: {
            const std::uint16_t ret = static_cast<std::uint16_t>(pc - 1);
            push(static_cast<std::uint8_t>(ret >> 8));
            push(static_cast<std::uint8_t>(ret & 0xFF));
            pc = ea;
            cyc = 6;
            break;
        }
        case LDA: a = setnz(val()); break;
        case LDX: x = setnz(val()); break;
        case LDY: y = setnz(val()); break;
        case STA: wr(ea, a); break;
        case STX: wr(ea, x); break;
        case STY: wr(ea, y); break;
        case AND: a = setnz(a & val()); break;
        case ORA: a = setnz(a | val()); break;
        case EOR: a = setnz(a ^ val()); break;
        case ADC: adc(val()); break;
        case SBC: sbc(val()); break;
        case CMP: compare(a, val()); break;
        case CPX: compare(x, val()); break;
        case CPY: compare(y, val()); break;
        case BIT: {
            const std::uint8_t v = rd(ea);
            flag(Z, (a & v) == 0);
            p = static_cast<std::uint8_t>((p & ~(N | V)) | (v & (N | V)));
            break;
        }
        default:
            if (isRmw(mn)) {
                const std::uint8_t v = rd(ea);
                wr(ea, v);                              // the NMOS writes the old value back first
                wr(ea, setnz(rmw(mn, v)));
            } else {
                failed_ = true;
            }
            break;
    }
    cycles += cyc;
    return cyc;
}

}  // namespace pacman::core
