"""Run-data collector for apple2tc, built into OUR oracle machine (TASK-029h2).

apple2tc decompiles against a JSON file of "run data" that its own emulator (a2run --collect) records while the program runs: which control-flow
instructions were executed and where they went (BranchTargets, Branches), and the code that the program wrote into memory and then executed
(generations). a2run cannot poke memory, but our scenarios (level end, energizers, eaten ghosts, two players...) need pokes. So this module
records the same data in the oracle machine, with the same rules as apple2tc-master/lib/cpuemu/DebugState6502.cpp (collectData / saveGeneration):

  * before each instruction: if its address was written before, it is a "generated" instruction (memExecStart, memExecFull);
  * a branch (relative), JMP, JSR, RTS, RTI, BRK records (instruction address -> effective address) and the target as branch target
    (RTS: the address popped + 1; the effective address of a relative branch is its target whether or not it is taken);
  * a store/read-modify-write to an address that is a generated instruction start saves a generation (all generated instruction bytes,
    as segments, with the registers BEFORE that instruction) and clears the written marks of those bytes;
  * generation 0 is empty and holds the start registers.

Usage:  c = Collector(mc)  (before running; wraps mc.cpu.step and mc.write)   ...   c.save('x.json') / c.merge(other)
Several collectors (one per scenario) are merged with Collector.merge_files(out, [paths]).
Validated against a2run on an attract-mode run (tools/a2tc_validate.py). Original code and bytes: output stays in scratch/."""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import OPS

BRANCHES = {'BCC', 'BCS', 'BNE', 'BEQ', 'BPL', 'BMI', 'BVC', 'BVS'}
CONTROL = BRANCHES | {'JMP', 'JSR', 'RTS', 'RTI', 'BRK'}
WRITERS = {'STA', 'STX', 'STY', 'INC', 'DEC', 'ASL', 'LSR', 'ROL', 'ROR'}
LEN = {'imp': 1, 'acc': 1, 'imm': 2, 'zp': 2, 'zpx': 2, 'zpy': 2, 'izx': 2, 'izy': 2, 'rel': 2, 'abs': 3, 'abx': 3, 'aby': 3, 'ind': 3}


class Collector:
    def __init__(self, mc, limit=None):
        self.mc, self.limit = mc, limit
        cpu = mc.cpu
        self.regs0 = self._regs()
        self.targets, self.edges = set(), {}
        self.generations = [{'regs': self.regs0, 'code': []}]
        self.written = bytearray(65536)
        self.exec_start = bytearray(65536)
        self.exec_full = bytearray(65536)
        self.icount = 0
        self.stop = False
        self.pre = None
        self.writing = False
        step = cpu.step
        write = mc.write
        m = mc.m

        def wstep():
            pc = cpu.pc
            op = m[pc]
            if op not in OPS:
                return step()
            mn, mode = OPS[op]
            size = LEN[mode]
            if self.written[pc]:
                self.exec_start[pc] = 1
                for i in range(pc, min(pc + size, 65536)):
                    self.exec_full[i] = 1
            if mn in CONTROL:
                ea = self._ea(mn, mode, pc)
                self.targets.add(ea)
                self.edges.setdefault(pc, set()).add(ea)
                if self.limit and self.icount >= self.limit:
                    self.stop = True
                self.icount += 1
            self.writing = mn in WRITERS and mode != 'acc'
            if self.writing:
                self.pre = self._regs()
            return step()

        def wwrite(a, v):
            if self.writing:
                if self.exec_start[a]:
                    self._save_generation()
                self.written[a] = 1
            return write(a, v)

        cpu.step = wstep
        mc.write = wwrite

    def _regs(self):
        c = self.mc.cpu
        return {'a': c.a, 'pc': c.pc, 'sp': c.sp, 'status': c.p, 'x': c.x, 'y': c.y}

    def _ea(self, mn, mode, pc):
        m, c = self.mc.m, self.mc.cpu
        if mode == 'rel':
            d = m[pc + 1]
            return (pc + 2 + (d - 256 if d >= 128 else d)) & 0xFFFF
        if mn in ('JMP', 'JSR') and mode == 'abs':
            return m[pc + 1] | m[pc + 2] << 8
        if mn == 'JMP' and mode == 'ind':
            a = m[pc + 1] | m[pc + 2] << 8
            return m[a] | m[(a & 0xFF00) | ((a + 1) & 0xFF)] << 8
        if mn == 'RTS':
            sp = c.sp
            return ((m[0x100 + ((sp + 1) & 255)] | m[0x100 + ((sp + 2) & 255)] << 8) + 1) & 0xFFFF
        raise RuntimeError(f"control-flow instruction {mn} {mode} at ${pc:04X} not modelled")

    def _save_generation(self):
        m = self.mc.m
        code, a = [], 0
        ef = self.exec_full
        while a < 65536:
            if ef[a]:
                b = a
                while b < 65536 and ef[b]:
                    b += 1
                code.append({'addr': a, 'bytes': list(m[a:b])})
                self.targets.add(a)
                for i in range(a, b):
                    self.written[i] = 0
                a = b + 1
            else:
                a += 1
        self.generations.append({'regs': self.pre, 'code': code})
        self.exec_start = bytearray(65536)
        self.exec_full = bytearray(65536)

    def data(self):
        return {'BaseStats': {'decimalADC': False, 'decimalSBC': False, 'decimalSet': False, 'limit': self.icount, 'stackOverflow': False,
                              'stackUnderflow': False, 'startRegs': self.regs0},
                'BranchTargets': sorted(self.targets),
                'Branches': {str(k): sorted(v) for k, v in sorted(self.edges.items())},
                'generations': self.generations}

    def finish(self):
        """As finishCollection() in a2run: a last generation with everything generated and not yet saved, with the final registers."""
        c = self.mc.cpu
        self.pre = self._regs()
        self._save_generation()

    def save(self, path):
        self.finish()
        json.dump(self.data(), open(path, 'w'), separators=(',', ':'))
