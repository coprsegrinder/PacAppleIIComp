"""TASK-033-4d: the WHOLE attract demo game recorded tick by tick (development tool; original data from scratch/).
Boot, '1' + Return, then every execution of $7861 with $E8 <> 0 (the demo's ticks) up to the demo's end. Writes
  scratch/ticks_DEMOF.bin    the format of tools/record_ticks.py (inputs and pokes empty),
  scratch/ghostram_DEMOF.bin the zero page and the ghost queues per tick (as tools/record_ghost_ram.py),
  scratch/demo_cycles.bin    u32 count, then per tick the u32 cycle at which the tick starts (counted from the entry of $52BB of the same attract turn... here: from boot),
  scratch/demo_stream_full.bin (as tools/record_special.py demo),
  scratch/pages_DEMOF.bin    the hi-res page at the start of every 10th tick.
Usage: python -I tools/record_demo_full.py      (about 4 minutes)"""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
from apple2 import NeedKey
from play import do_step
from cov_close import factory

TICK_PC = 0x7861
mc = factory()
for st in ('key:1', 'key:cr'):
    do_step(mc, st, lambda *_: None)
states, rams, cycles, extra, pages = [], [], [], bytearray(), []
step = mc.cpu.step
last = [0]


def hook():
    if mc.cpu.pc == TICK_PC and mc.m[0xE8] != 0:
        if len(states) % 10 == 0:
            pages.append((len(states), bytes(mc.m[0x2000:0x4000])))
        states.append(G.state_bytes(mc.m))
        rams.append(bytes(mc.m[0:256]) + bytes(mc.m[0x800:0xC00]))
        cycles.append(mc.cpu.cycles)
        lo, hi = mc.m[0x1B], mc.m[0x1C]
        p = ((hi << 8) | lo) + 1
        extra.extend([lo, hi, mc.m[p & 0xFFFF]])
        last[0] = mc.cpu.cycles
    return step()


mc.cpu.step = hook
while True:
    try:
        mc.run(150_000)
    except NeedKey:
        mc.push_key(13)
    if states and mc.cpu.cycles - last[0] > 8_000_000:
        break      # the demo has ended (no tick for 8M cycles)
    if len(states) > 20000:
        break
print('demo ticks', len(states), flush=True)
h = b''
for s in states:
    h = hashlib.sha256(h + s).digest()
with open('scratch/ticks_DEMOF.bin', 'wb') as f:
    f.write(b'TICK' + struct.pack('<II', len(states), 1010) + h)
    for s in states:
        f.write(s)
    f.write(struct.pack('<I', 0))
    f.write(struct.pack('<I', 0))
with open('scratch/ghostram_DEMOF.bin', 'wb') as f:
    f.write(b'GRAM' + struct.pack('<II', len(rams), 1280))
    for r in rams:
        f.write(r)
with open('scratch/demo_cycles.bin', 'wb') as f:
    f.write(struct.pack('<I', len(cycles)))
    for c in cycles:
        f.write(struct.pack('<I', c))
open('scratch/demo_stream_full.bin', 'wb').write(bytes(extra))
with open('scratch/pages_DEMOF.bin', 'wb') as f:      # 'PGDM', u32 count, then per page u32 tick + 8,192 bytes: the hi-res page at the START of every 10th tick
    f.write(b'PGDM' + struct.pack('<I', len(pages)))
    for k, pg in pages:
        f.write(struct.pack('<I', k) + pg)
print('written; first tick at cycle', cycles[0], 'last', cycles[-1], flush=True)
