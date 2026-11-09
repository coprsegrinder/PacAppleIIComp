"""TASK-034-TIMING: writes scratch/ticks_<S>.bin.cyc for a golden scenario from scratch/tickcost_<S>.json (tools/tick_cost_record.py): u32 count, then the cycle at which every tick starts
(counted from the first sample). Development tool.  Usage: python -I tools/export_tick_cycles.py A"""
import json, struct, sys
S = sys.argv[1]
cost = json.load(open('scratch/tickcost_%s.json' % S))['cycles']
at, t = [], 0
for c in cost:
    at.append(t)
    t += c
at.append(t)
open('scratch/ticks_%s.bin.cyc' % S, 'wb').write(struct.pack('<I', len(at)) + b''.join(struct.pack('<I', v) for v in at))
print(S, len(at), 'tick starts')
