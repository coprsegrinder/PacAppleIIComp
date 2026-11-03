"""TASK-033-6c: the memory of the attract game in consecutive turns without any key: at the first tick of the demo ($7861), at the death routine ($8BDD) and at the next turn's $52BB.
Writes scratch/mem_demo_turn_<k>_<first|death|t0>.bin (the first 1,024 bytes of memory) for k = 1..3 (development tool; original data from scratch/, real monitor ROM)."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine
import explore

mc = make_machine(real_rom=True)
explore.send(mc, ['1', '\r'])
turn, last = 0, -10**12
phase = 'first'
while turn < 3 or phase != 'first':
    target = {'first': 0x7861, 'death': 0x8BDD, 't0': 0x52BB}[phase]
    mc.run(50_000_000, stop_pc=target)
    if mc.cpu.pc != target:
        continue
    if phase == 'first':
        if mc.m[0xE8] == 1 and mc.cpu.cycles - last > 3_000_000:
            turn += 1
            open('scratch/mem_demo_turn_%d_first.bin' % turn, 'wb').write(bytes(mc.m[0:0x400]))
            print('turn', turn, 'first tick at cycle', mc.cpu.cycles, flush=True)
            phase = 'death'
        else:
            last = mc.cpu.cycles
            mc.cpu.step()
            continue
    elif phase == 'death':
        open('scratch/mem_demo_turn_%d_death.bin' % turn, 'wb').write(bytes(mc.m[0:0x400]))
        phase = 't0'
        mc.cpu.step()
        continue
    else:
        open('scratch/mem_demo_turn_%d_t0.bin' % turn, 'wb').write(bytes(mc.m[0:0x400]))
        phase = 'first'
        last = mc.cpu.cycles
    mc.cpu.step()
