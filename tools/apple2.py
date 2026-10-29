"""Apple II soft-switch and monitor-ROM stand-ins for the Python 6502 core (development/tracing only).

No ROM image is used. Instead:
  * Bus: 64K RAM; $C000-$C0FF are soft switches (keyboard, speaker, video, buttons, paddles).
  * ROM routines the game calls are *trapped*: when PC reaches the entry, Python performs the documented
    monitor behavior and then does an RTS. Any other code executed in $C100-$FFFF raises CPUError.
Behavior of the trapped routines is from documented monitor behavior (Inferred), not from a ROM dump.
"""
from array import array
from cpu6502 import CPU, CPUError
from dis6502 import OPS

SCREEN_COLS, SCREEN_ROWS = 40, 24
# zero-page monitor variables
WNDLFT, WNDWDTH, WNDTOP, WNDBTM, CH, CV, BASL, BASH = 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x28, 0x29
INVFLG, PROMPT, CSWL, KSWL, RNDL, RNDH = 0x32, 0x33, 0x36, 0x38, 0x4E, 0x4F
IN = 0x0200
CYCLES_PER_PADDLE_UNIT = 11     # Inferred: standard PREAD loop timing, ~2.8 ms for 255


class NeedKey(Exception):
    """The program is waiting for a keypress and the key queue is empty. State is intact; push_key() and run again."""


def row_base(r):
    return 0x400 + (r % 8) * 0x80 + (r // 8) * 0x28


class Machine:
    TRAPS = {0xFC22: 'vtab', 0xFD0C: 'rdkey', 0xFD6F: 'getln', 0xFDF0: 'cout1', 0xFE89: 'setkbd', 0xFE93: 'setvid'}

    def __init__(self):
        self.m = bytearray(65536)
        self.cpu = CPU(self)
        self.keys = []              # pending keystrokes (values with bit 7 set)
        self.kbd = 0                # $C000 latch (bit 7 = strobe)
        self.speaker = []           # cycle counts of each $C030 access
        self.switches = {'text': True, 'mixed': False, 'page2': False, 'hires': False}
        self.buttons = [False, False, False]
        self.paddles = [127, 127, 127, 127]
        self.paddle_start = None
        self.unknown_io = {}
        self.rom_writes = 0
        self.cur_pc = 0
        self.rom_calls = {}
        self._getln = None          # in-progress GETLN state
        self.cov = None             # set by enable_coverage()
        for a in self.TRAPS:
            self.m[a] = 0x60

    # ---- setup helpers ----
    def init_text_screen(self):
        for r in range(SCREEN_ROWS):
            b = row_base(r)
            self.m[b:b + SCREEN_COLS] = b'\xA0' * SCREEN_COLS
        self.m[WNDLFT], self.m[WNDWDTH], self.m[WNDTOP], self.m[WNDBTM] = 0, 40, 0, 24
        self.m[INVFLG] = 0xFF
        self.m[PROMPT] = 0xDD       # ']' : Applesoft prompt, as left by the HELLO program
        self.set_cv(0)

    def load(self, addr, data):
        self.m[addr:addr + len(data)] = data

    def text_lines(self):
        """Text page 1 as 24 ASCII strings (inverse/flash shown as normal; non-printing as '?')."""
        out = []
        for r in range(SCREEN_ROWS):
            b = row_base(r)
            s = ''
            for c in self.m[b:b + SCREEN_COLS]:
                ch = c & 0x7F
                if c < 0x80:
                    ch = ch & 0x3F | 0x40 if ch & 0x40 == 0 else ch   # inverse/flash letters map back
                s += chr(ch) if 0x20 <= ch < 0x7F else '?'
            out.append(s.rstrip())
        return out

    # ---- coverage (TASK-013c) ----
    def enable_coverage(self):
        """Track executed, read and written addresses, and the PC of the last instruction that wrote each address.
        Opcode/operand fetches count as 'executed' only, not 'read'."""
        n = 65536
        self.cov = {'exec': bytearray(n), 'read': bytearray(n), 'write': bytearray(n),
                    'writer': array('H', [0]) * n, 'opstart': bytearray(n)}
        m = self.m
        cpu = self.cpu

        def fetch():                        # instruction-stream reads bypass the 'read' tracking
            v = m[cpu.pc]
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            return v
        cpu.fetch = fetch

    def _length(self, op):
        mode = OPS[op][1]
        return 1 if mode in ('imp', 'acc') else 2 if mode in ('imm', 'zp', 'zpx', 'zpy', 'izx', 'izy', 'rel') else 3

    # ---- bus ----
    def read(self, a):
        if self.cov is not None:
            self.cov['read'][a] = 1
        if 0xC000 <= a <= 0xC0FF:
            return self.io(a, None)
        return self.m[a]

    def write(self, a, v):
        if self.cov is not None:
            self.cov['write'][a] = 1
            self.cov['writer'][a] = self.cur_pc
        if 0xC000 <= a <= 0xC0FF:
            self.io(a, v)
        elif a >= 0xC100:
            self.rom_writes += 1            # ROM/slot space: ignored (language card not modelled)
        else:
            self.m[a] = v

    def io(self, a, v):
        cyc = self.cpu.cycles
        if a == 0xC000:
            if v is not None:
                return self._unknown(a, 'write')
            self._latch_key()
            return self.kbd
        if a == 0xC010:
            self.kbd &= 0x7F
            return self.kbd
        if a == 0xC030:
            self.speaker.append(cyc)
            return 0
        if 0xC050 <= a <= 0xC057:
            s = self.switches
            on = bool(a & 1)
            s[('text', 'mixed', 'page2', 'hires')[(a - 0xC050) >> 1]] = on   # $C050/1 graphics/text, 52/3 full/mixed, 54/5 page1/2, 56/7 lores/hires
            return 0
        if 0xC061 <= a <= 0xC063:
            return 0x80 if self.buttons[a - 0xC060 - 1] else 0
        if 0xC064 <= a <= 0xC067:
            if self.paddle_start is None:
                return 0
            return 0x80 if cyc - self.paddle_start < self.paddles[a - 0xC064] * CYCLES_PER_PADDLE_UNIT else 0
        if a == 0xC070:
            self.paddle_start = cyc
            return 0
        return self._unknown(a, 'read' if v is None else 'write')

    def _unknown(self, a, kind):
        self.unknown_io[(a, kind)] = self.unknown_io.get((a, kind), 0) + 1
        return 0

    def _latch_key(self):
        if not (self.kbd & 0x80) and self.keys:
            self.kbd = self.keys.pop(0) | 0x80

    def push_key(self, ch):
        """ch: int 0-255 or 1-char str; high bit is set automatically."""
        self.keys.append((ord(ch) if isinstance(ch, str) else ch) | 0x80)

    # ---- running ----
    def run(self, max_instr=1_000_000, stop_pc=None):
        """Run until max_instr, stop_pc, or NeedKey (raised). Returns number of instructions executed."""
        cpu, n = self.cpu, 0
        while n < max_instr:
            if cpu.pc == stop_pc:
                break
            t = self.TRAPS.get(cpu.pc)
            if t is not None:
                getattr(self, 'rom_' + t)()                 # NeedKey propagates with PC still on the trap
                self.rom_calls[t] = self.rom_calls.get(t, 0) + 1
                n += 1
                continue
            if cpu.pc >= 0xC100:
                raise CPUError(f"executing unmodelled ROM/slot address ${cpu.pc:04X}")
            if self.cov is not None:
                self.cur_pc = cpu.pc
                op = self.m[cpu.pc]
                if op in OPS:
                    self.cov['opstart'][cpu.pc] = 1
                    for i in range(self._length(op)):
                        self.cov['exec'][(cpu.pc + i) & 0xFFFF] = 1
            cpu.step()
            n += 1
        return n

    def _rts(self):
        c = self.cpu
        lo = c.pull()
        hi = c.pull()
        c.pc = ((hi << 8 | lo) + 1) & 0xFFFF
        c.cycles += 6

    # ---- ROM stand-ins ----
    def set_cv(self, row):
        m = self.m
        m[CV] = row
        base = row_base(row & 0x1F) if row < 32 else 0x400
        m[BASL] = (base + m[WNDLFT]) & 0xFF
        m[BASH] = (base + m[WNDLFT]) >> 8

    def rom_vtab(self):                 # $FC22 VTAB: recompute BASL/BASH from CV ($25) (game does INC $25 then JSR $FC22)
        self.set_cv(self.m[CV])
        self.cpu.a = self.m[BASL]
        self.cpu.setnz(self.cpu.a)
        self._rts()

    def rom_setkbd(self):               # $FE89 IN#0
        self.m[KSWL], self.m[KSWL + 1] = 0x1B, 0xFD
        self._rts()

    def rom_setvid(self):               # $FE93 PR#0
        self.m[CSWL], self.m[CSWL + 1] = 0xF0, 0xFD
        self.m[INVFLG] = 0xFF
        self._rts()

    def _next_key(self):
        """Wait for a key like RDKEY: bump the random seed while waiting; raise NeedKey if none queued."""
        self._latch_key()
        if not (self.kbd & 0x80):
            self.m[RNDL] = (self.m[RNDL] + 1) & 0xFF
            if self.m[RNDL] == 0:
                self.m[RNDH] = (self.m[RNDH] + 1) & 0xFF
            raise NeedKey()
        k = self.kbd
        self.kbd &= 0x7F
        return k

    def rom_rdkey(self):                # $FD0C: returns key (bit 7 set) in A
        self.cpu.a = self._next_key()
        self.cpu.setnz(self.cpu.a)
        self._rts()

    def rom_cout1(self):                # $FDF0: A, X, Y preserved
        self.cout(self.cpu.a)
        self._rts()

    def cout(self, c):
        m = self.m
        if c < 0x80:
            return self._store(c)
        if c < 0xA0:
            if c == 0x8D:
                m[CH] = 0
                self._lf()
            elif c == 0x8A:
                self._lf()
            elif c == 0x88:
                if m[CH] > m[WNDLFT]:
                    m[CH] -= 1
            # $87 bell and other controls: no visible effect modelled
            return
        self._store(c & m[INVFLG])

    def _store(self, c):
        m = self.m
        m[(m[BASH] << 8 | m[BASL]) + m[CH]] = c
        m[CH] += 1
        if m[CH] >= m[WNDWDTH]:
            m[CH] = 0
            self._lf()

    def _lf(self):
        m = self.m
        if m[CV] + 1 >= m[WNDBTM]:
            self._scroll()
        else:
            self.set_cv(m[CV] + 1)

    def _scroll(self):
        m = self.m
        left, w = m[WNDLFT], m[WNDWDTH]
        for r in range(m[WNDTOP], m[WNDBTM] - 1):
            src, dst = row_base(r + 1) + left, row_base(r) + left
            m[dst:dst + w] = m[src:src + w]
        last = row_base(m[WNDBTM] - 1) + left
        m[last:last + w] = b'\xA0' * w
        self.set_cv(m[WNDBTM] - 1)

    def _clreol(self):
        m = self.m
        b = (m[BASH] << 8 | m[BASL])
        for i in range(m[CH], m[WNDWDTH]):
            m[b + i] = 0xA0

    def rom_getln(self):                # $FD6F: prompt, read a line into $0200.., X = length, A = $8D
        m, cpu = self.m, self.cpu
        if self._getln is None:
            self.cout(m[PROMPT])
            self._getln = []
        buf = self._getln
        while True:
            k = self._next_key()        # may raise NeedKey; state kept in self._getln
            if k == 0x98:               # Ctrl-X: cancel line
                self.cout(0xDC)
                self.cout(0x8D)
                self.cout(m[PROMPT])
                buf.clear()
            elif k == 0x88:             # backspace
                if buf:
                    buf.pop()
                    self.cout(0x88)
            elif k == 0x8D:
                self._clreol()
                for i, b in enumerate(buf):
                    m[IN + i] = b
                m[IN + len(buf)] = 0x8D
                self.cout(0x8D)
                cpu.x, cpu.a = len(buf), 0x8D
                cpu.setnz(cpu.a)
                self._getln = None
                self._rts()
                return
            else:
                if len(buf) < 254:
                    buf.append(k)
                    self.cout(k)
