"""TASK-034-TIMING: the cost of every call of the per-ghost routine $8763 in a recorded SCENARIO (as tools/ghost_call_profile.py does for the attract game; development tool).
Replays scratch/ticks_<X>.bin's keys and pokes like tools/record_ghost_ram.py and writes scratch/ghostcalls_<X>.json: [[ghost, entry-to-RTS cycles, of which drawing routines, tick, path], ...]
(tick = the index of the tick, counted from 1 like the cost tests' `tick + 1`).   Usage: python -I tools/ghost_call_profile_scenario.py <X>"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G
import cov_close

name = sys.argv[1]
if len(name) == 1 and name in 'ABCD':
    stored = json.load(open('tests/golden/gameplay_traces.json'))['scenarios'][name]
    inputs, pokes = stored['inputs'], stored['pokes']
    n = stored.get('ticks_done', G.SCENARIOS[name]['ticks'])
    G.SCENARIOS[name]['ticks'] = n
else:
    d = open('scratch/ticks_%s.bin' % name, 'rb').read()
    n = struct.unpack('<I', d[4:8])[0] - 1
    pos = 44 + (n + 1) * 1010
    k = struct.unpack('<I', d[pos:pos + 4])[0]
    pos += 4
    inputs = [[struct.unpack('<II', d[pos + 8 * i:pos + 8 * i + 8])[0], struct.unpack('<II', d[pos + 8 * i:pos + 8 * i + 8])[1] & 0x7F] for i in range(k)]
    pos += 8 * k
    k = struct.unpack('<I', d[pos:pos + 4])[0]
    pos += 4
    pokes = [list(struct.unpack('<III', d[pos + 12 * i:pos + 12 * i + 12])) for i in range(k)]
    G.SCENARIOS[name] = dict(seed=0, ticks=n, pokes=[])
DRAWS = (0x6A20, 0x699E, 0x6172, 0x5CF0)
S = {'hooked': False, 'tick': 0, 'in': False, 'stack': [], 'ghost': 0, 'c_in': 0, 'draw': 0, 'path': []}
rec = []
TR = []
orig = G.state_bytes


def hook_state(m):
    mc = cov_close.MACHINES[-1]
    if not S['hooked']:
        S['hooked'] = True
        step = mc.cpu.step
        cpu, mem = mc.cpu, mc.m

        def hook():
            pc = cpu.pc
            if pc == 0x7861 and mem[0xE8] == 0:
                S['tick'] += 1
            if not S['in']:
                if pc == 0x8763:
                    S.update({'in': True, 'stack': [], 'ghost': mem[0xE0], 'c_in': cpu.cycles, 'draw': 0, 'path': []})
                else:
                    return step()
            op = mem[pc]
            c0 = cpu.cycles
            r = step()
            if os.environ.get('TRACE') and S['tick'] == int(os.environ['TRACE']) and any(x == 0x6A20 for x, _ in S['stack']):
                TR.append((pc, cpu.cycles - c0, cpu.x, cpu.y))
            if op == 0x20:
                if cpu.pc == 0x6A20 and os.environ.get('DRAWDUMP') and S['tick'] == int(os.environ['DRAWDUMP']):
                    print('mem 7812:', [mem[0x7812 + i] for i in range(10)], '5958[149..]', mem[0x5958 + 149], mem[0x5A58 + 149])
                if cpu.pc in DRAWS and os.environ.get('DRAWDUMP') and S['tick'] == int(os.environ['DRAWDUMP']):
                    print('call %04X a=%02X x=%02X y=%02X z0C=%02X z0D=%02X z10=%02X z1D=%02X z1E=%02X at cycle %d' % (cpu.pc, cpu.a, cpu.x, cpu.y, mem[0x0C], mem[0x0D], mem[0x10], mem[0x1D], mem[0x1E], c0))
                S['stack'].append((cpu.pc, c0))
            elif op == 0x60:
                if S['stack']:
                    t, t0 = S['stack'].pop()
                    if t in DRAWS and not any(x in DRAWS for x, _ in S['stack']):
                        S['draw'] += cpu.cycles - t0
                        if os.environ.get('DRAWDUMP') and S['tick'] == int(os.environ['DRAWDUMP']):
                            print('   -> %04X took %d' % (t, cpu.cycles - t0))
                else:
                    rec.append([S['ghost'], cpu.cycles - S['c_in'], S['draw'], S['tick'], ''.join(S['path'])])
                    S['in'] = False
            elif not S['stack'] and op in (0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0):
                S['path'].append('T' if cpu.cycles - c0 > 2 else 'N')
            return r
        cpu.step = hook
    return orig(m)


G.state_bytes = hook_state
G.run(name, inputs, pokes)
json.dump(TR, open('scratch/trace_6A20.json', 'w'))
json.dump(rec, open('scratch/ghostcalls_%s.json' % name, 'w'))
print(name, 'calls', len(rec))
