"""TASK-033-4e / 4e3: packs scratch/prompts_<prefix>*.json (tools/capture_prompts.py, run with A2ROM=1: the real monitor ROM) into scratch/prompts.bin for tests/test_flow.cpp.
Usage: python -I tools/export_prompts.py [prefix r]
Format: 'PRMP', u32 count, per record: u32 press offset (from T0 of turn 0), u32 digit (1-5), u32 last key (0 = Space, 1 = Return), u32 turn index of the key poll (0/1), u32 key poll
cycle ($6FAE) relative to that turn's T0, u32 first key wait (the cycle at which the ROM waits in KEYIN, same base), u32 second key wait, u32 end ($43D2 for Space, $52FC for Return),
then 1024 bytes text page 2 at the first wait, 1024 at the second, 256 bytes zero page at the first wait, 256 at the second."""
import glob, json, struct, sys
PERIOD = 63189736
prefix = sys.argv[1] if len(sys.argv) > 1 else 'r'
recs = []
for f in sorted(glob.glob('scratch/prompts_%s[0-9]*.json' % prefix), key=lambda s: int(s.split('_' + prefix)[1].split('.')[0])):
    d = json.load(open(f))
    marks = d['marks']
    poll = [m for m in marks if m[0] == 0x6FAE][0][1]
    turn = 1 if poll >= PERIOD else 0
    rel = lambda c: c - turn * PERIOD
    w2 = [m for m in marks if m[0] == 0x4261][0][1]
    last_sp = any(m[0] == 0x43D2 and m[1] > w2 for m in marks)
    ret = [m for m in marks if m[0] == 0x52FC]
    end = [m for m in marks if m[0] == 0x43D2 and m[1] > w2][0][1] if last_sp else ret[0][1]
    recs.append((d['offset'], d['digit'], 0 if last_sp else 1, turn, rel(poll), rel(d['cycle_wait']), rel(d['cycle_wait2']), rel(end),
                 bytes.fromhex(d['pages']['players']), bytes.fromhex(d['pages']['press']), bytes.fromhex(d['zp1']), bytes.fromhex(d['zp2'])))
out = open('scratch/prompts.bin', 'wb')
out.write(b'PRMP' + struct.pack('<I', len(recs)))
for r in recs:
    out.write(struct.pack('<8I', *r[:8]) + r[8] + r[9] + r[10] + r[11])
print(len(recs), 'records')
