"""TASK-033-4e: packs scratch/prompts_p*.json (tools/capture_prompts.py) into scratch/prompts.bin for tests/test_flow.cpp.
Format: 'PRMP', u32 count, per record: u32 press offset (from T0 of turn 0), u32 digit (1-5), u32 last key (0 = Space, 1 = Return), u32 turn index of the key poll (0/1),
u32 key poll cycle ($6FAE) relative to that turn's T0, u32 players-prompt key wait ($4233), u32 press-return key wait ($4261), u32 end ($43D2 for Space, $52FC for Return) all relative to the same T0,
1024 bytes text page 2 at the first key wait, 1024 bytes at the second."""
import glob, json, struct, sys
PERIOD = 63189736
recs = []
for f in sorted(glob.glob('scratch/prompts_p*.json'), key=lambda s: int(s.split('_p')[1].split('.')[0])):
    d = json.load(open(f))
    marks = d['marks']
    poll = [m for m in marks if m[0] == 0x6FAE][0][1]
    turn = 1 if poll >= PERIOD else 0
    rel = lambda c: c - turn * PERIOD
    w1 = [m for m in marks if m[0] == 0x4233][0][1]
    w2 = [m for m in marks if m[0] == 0x4261][0][1]
    last_sp = any(m[0] == 0x43D2 and m[1] > w2 for m in marks)
    ret = [m for m in marks if m[0] == 0x52FC]
    end = [m for m in marks if m[0] == 0x43D2 and m[1] > w2][0][1] if last_sp else ret[0][1]
    digit = None
    # digit and last key are recorded in the log name order of capture_prompts arguments
    recs.append((d['offset'], d.get('digit', 0), 0 if last_sp else 1, turn, rel(poll), rel(w1), rel(w2), rel(end), bytes.fromhex(d['pages']['players']), bytes.fromhex(d['pages']['press'])))
out = open('scratch/prompts.bin', 'wb')
out.write(b'PRMP' + struct.pack('<I', len(recs)))
for r in recs:
    out.write(struct.pack('<8I', *r[:8]) + r[8] + r[9])
print(len(recs), 'records')
