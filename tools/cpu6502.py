"""Minimal NMOS 6502 core (documented opcodes) for analysis/tracing. NOT part of the shipped game.

Memory is any object with read(addr) -> int and write(addr, value); RAM below is a plain 64K array.
step() executes one instruction and returns its cycle count (page-cross and branch penalties included).
Undocumented opcodes raise CPUError so that nothing is silently mis-executed.
"""
from dis6502 import OPS

N, V, B, D, I, Z, C = 0x80, 0x40, 0x10, 0x08, 0x04, 0x02, 0x01   # bit 5 (0x20) is always set when pushed


class CPUError(Exception):
    pass


class RAM:
    def __init__(self):
        self.m = bytearray(65536)

    def read(self, a):
        return self.m[a]

    def write(self, a, v):
        self.m[a] = v


READ_OPS = {'ADC', 'AND', 'CMP', 'CPX', 'CPY', 'EOR', 'LDA', 'LDX', 'LDY', 'ORA', 'SBC', 'BIT'}
WRITE_OPS = {'STA', 'STX', 'STY'}
RMW_OPS = {'ASL', 'LSR', 'ROL', 'ROR', 'INC', 'DEC'}
BRANCH = {'BCC': (C, 0), 'BCS': (C, C), 'BNE': (Z, 0), 'BEQ': (Z, Z),
          'BPL': (N, 0), 'BMI': (N, N), 'BVC': (V, 0), 'BVS': (V, V)}
BASE = {'imm': 2, 'zp': 3, 'zpx': 4, 'zpy': 4, 'abs': 4, 'abx': 4, 'aby': 4, 'izx': 6, 'izy': 5}
RMW_CYCLES = {'zp': 5, 'zpx': 6, 'abs': 6, 'abx': 7}


class CPU:
    def __init__(self, mem):
        self.mem = mem
        self.a = self.x = self.y = 0
        self.sp = 0xFD
        self.pc = 0
        self.p = I | 0x20
        self.cycles = 0

    # --- helpers ---
    def rd(self, a):
        return self.mem.read(a & 0xFFFF)

    def wr(self, a, v):
        self.mem.write(a & 0xFFFF, v & 0xFF)

    def rd16(self, a):
        return self.rd(a) | self.rd(a + 1) << 8

    def push(self, v):
        self.wr(0x100 | self.sp, v)
        self.sp = (self.sp - 1) & 0xFF

    def pull(self):
        self.sp = (self.sp + 1) & 0xFF
        return self.rd(0x100 | self.sp)

    def setnz(self, v):
        self.p = (self.p & ~(N | Z)) | (v & N) | (Z if (v & 0xFF) == 0 else 0)
        return v & 0xFF

    def flag(self, mask, on):
        self.p = (self.p | mask) if on else (self.p & ~mask)

    def fetch(self):
        v = self.rd(self.pc)
        self.pc = (self.pc + 1) & 0xFFFF
        return v

    def fetch16(self):
        lo = self.fetch()
        return lo | self.fetch() << 8

    def ea(self, mode):
        """Effective address and whether indexing crossed a page boundary."""
        if mode == 'zp':
            return self.fetch(), False
        if mode == 'zpx':
            return (self.fetch() + self.x) & 0xFF, False
        if mode == 'zpy':
            return (self.fetch() + self.y) & 0xFF, False
        if mode == 'abs':
            return self.fetch16(), False
        if mode in ('abx', 'aby'):
            base = self.fetch16()
            a = (base + (self.x if mode == 'abx' else self.y)) & 0xFFFF
            return a, (base & 0xFF00) != (a & 0xFF00)
        if mode == 'izx':
            z = (self.fetch() + self.x) & 0xFF
            return self.rd(z) | self.rd((z + 1) & 0xFF) << 8, False
        if mode == 'izy':
            z = self.fetch()
            base = self.rd(z) | self.rd((z + 1) & 0xFF) << 8
            a = (base + self.y) & 0xFFFF
            return a, (base & 0xFF00) != (a & 0xFF00)
        raise CPUError(mode)

    # --- arithmetic ---
    def adc(self, m):
        a, c = self.a, self.p & C
        if self.p & D:
            al = (a & 0xF) + (m & 0xF) + c
            if al >= 0xA:
                al = ((al + 6) & 0xF) + 0x10
            r = (a & 0xF0) + (m & 0xF0) + al
            self.flag(N, r & 0x80)
            self.flag(V, (a ^ r) & ~(a ^ m) & 0x80)
            self.flag(Z, ((a + m + c) & 0xFF) == 0)       # NMOS: Z comes from the binary result
            if r >= 0xA0:
                r += 0x60
            self.flag(C, r >= 0x100)
            self.a = r & 0xFF
        else:
            r = a + m + c
            self.flag(V, (a ^ r) & (m ^ r) & 0x80)
            self.flag(C, r > 0xFF)
            self.a = self.setnz(r)

    def sbc(self, m):
        a, c = self.a, self.p & C
        r = a - m - (1 - c)
        self.flag(V, (a ^ m) & (a ^ r) & 0x80)
        self.flag(C, r >= 0)
        if self.p & D:
            al = (a & 0xF) - (m & 0xF) + c - 1
            if al < 0:
                al = ((al - 6) & 0xF) - 0x10
            r2 = (a & 0xF0) - (m & 0xF0) + al
            if r2 < 0:
                r2 -= 0x60
            self.setnz(r)                                   # NMOS: N,Z come from the binary result
            self.a = r2 & 0xFF
        else:
            self.a = self.setnz(r)

    def cmp(self, reg, m):
        r = reg - m
        self.flag(C, r >= 0)
        self.setnz(r)

    # --- execute ---
    def step(self):
        pc0 = self.pc
        op = self.fetch()
        if op not in OPS:
            raise CPUError(f"illegal/undocumented opcode ${op:02X} at ${pc0:04X}")
        mn, mode = OPS[op]
        if mn in BRANCH:
            return self._branch(mn)
        if mode in ('imp', 'acc'):
            return self._implied(mn, mode)
        if mode == 'ind':                                   # JMP (addr), with the NMOS page-wrap bug
            a = self.fetch16()
            self.pc = self.rd(a) | self.rd((a & 0xFF00) | ((a + 1) & 0xFF)) << 8
            self.cycles += 5
            return 5
        ea, m, cyc = None, None, 2
        if mode == 'imm':
            m = self.fetch()
        else:
            ea, cross = self.ea(mode)
            if mn == 'JMP':
                self.pc = ea
                self.cycles += 3
                return 3
            cyc = BASE[mode]
            if mn in READ_OPS and cross:
                cyc += 1
            if mn in WRITE_OPS and mode in ('abx', 'aby', 'izy'):
                cyc += 1                                    # stores always pay the indexing cycle
            if mn in RMW_OPS:
                cyc = RMW_CYCLES[mode]
        val = (lambda: m if ea is None else self.rd(ea))
        if mn == 'JSR':
            ret = (self.pc - 1) & 0xFFFF
            self.push(ret >> 8)
            self.push(ret & 0xFF)
            self.pc = ea
            cyc = 6
        elif mn == 'LDA':
            self.a = self.setnz(val())
        elif mn == 'LDX':
            self.x = self.setnz(val())
        elif mn == 'LDY':
            self.y = self.setnz(val())
        elif mn == 'STA':
            self.wr(ea, self.a)
        elif mn == 'STX':
            self.wr(ea, self.x)
        elif mn == 'STY':
            self.wr(ea, self.y)
        elif mn == 'AND':
            self.a = self.setnz(self.a & val())
        elif mn == 'ORA':
            self.a = self.setnz(self.a | val())
        elif mn == 'EOR':
            self.a = self.setnz(self.a ^ val())
        elif mn == 'ADC':
            self.adc(val())
        elif mn == 'SBC':
            self.sbc(val())
        elif mn == 'CMP':
            self.cmp(self.a, val())
        elif mn == 'CPX':
            self.cmp(self.x, val())
        elif mn == 'CPY':
            self.cmp(self.y, val())
        elif mn == 'BIT':
            v = self.rd(ea)
            self.flag(Z, (self.a & v) == 0)
            self.p = (self.p & ~(N | V)) | (v & (N | V))
        elif mn in RMW_OPS:
            v = self.rd(ea)
            self.wr(ea, v)                                  # NMOS writes the old value back first (visible to hooks)
            self.wr(ea, self.setnz(self._rmw(mn, v)))
        else:
            raise CPUError(mn)
        self.cycles += cyc
        return cyc

    def _branch(self, mn):
        off = self.fetch()
        mask, want = BRANCH[mn]
        cyc = 2
        if (self.p & mask) == want:
            t = (self.pc + (off - 256 if off > 127 else off)) & 0xFFFF
            cyc += 1 + ((t & 0xFF00) != (self.pc & 0xFF00))
            self.pc = t
        self.cycles += cyc
        return cyc

    def _rmw(self, mn, v):
        c = self.p & C
        if mn == 'ASL':
            self.flag(C, v & 0x80)
            return (v << 1) & 0xFF
        if mn == 'LSR':
            self.flag(C, v & 1)
            return v >> 1
        if mn == 'ROL':
            self.flag(C, v & 0x80)
            return ((v << 1) | c) & 0xFF
        if mn == 'ROR':
            self.flag(C, v & 1)
            return (v >> 1) | (c << 7)
        if mn == 'INC':
            return (v + 1) & 0xFF
        return (v - 1) & 0xFF

    def _implied(self, mn, mode):
        cyc = 2
        if mode == 'acc':
            self.a = self.setnz(self._rmw(mn, self.a))
        elif mn == 'NOP':
            pass
        elif mn == 'TAX':
            self.x = self.setnz(self.a)
        elif mn == 'TAY':
            self.y = self.setnz(self.a)
        elif mn == 'TXA':
            self.a = self.setnz(self.x)
        elif mn == 'TYA':
            self.a = self.setnz(self.y)
        elif mn == 'TSX':
            self.x = self.setnz(self.sp)
        elif mn == 'TXS':
            self.sp = self.x
        elif mn == 'INX':
            self.x = self.setnz(self.x + 1)
        elif mn == 'INY':
            self.y = self.setnz(self.y + 1)
        elif mn == 'DEX':
            self.x = self.setnz(self.x - 1)
        elif mn == 'DEY':
            self.y = self.setnz(self.y - 1)
        elif mn == 'CLC':
            self.flag(C, 0)
        elif mn == 'SEC':
            self.flag(C, 1)
        elif mn == 'CLD':
            self.flag(D, 0)
        elif mn == 'SED':
            self.flag(D, 1)
        elif mn == 'CLI':
            self.flag(I, 0)
        elif mn == 'SEI':
            self.flag(I, 1)
        elif mn == 'CLV':
            self.flag(V, 0)
        elif mn == 'PHA':
            self.push(self.a)
            cyc = 3
        elif mn == 'PHP':
            self.push(self.p | B | 0x20)
            cyc = 3
        elif mn == 'PLA':
            self.a = self.setnz(self.pull())
            cyc = 4
        elif mn == 'PLP':
            self.p = (self.pull() & ~B) | 0x20
            cyc = 4
        elif mn == 'RTS':
            lo = self.pull()
            self.pc = ((lo | self.pull() << 8) + 1) & 0xFFFF
            cyc = 6
        elif mn == 'RTI':
            self.p = (self.pull() & ~B) | 0x20
            lo = self.pull()
            self.pc = lo | self.pull() << 8
            cyc = 6
        elif mn == 'BRK':
            self.fetch()                                    # padding byte: pushed return address is BRK+2
            self.push(self.pc >> 8)
            self.push(self.pc & 0xFF)
            self.push(self.p | B | 0x20)
            self.flag(I, 1)
            self.pc = self.rd16(0xFFFE)
            cyc = 7
        else:
            raise CPUError(mn)
        self.cycles += cyc
        return cyc
