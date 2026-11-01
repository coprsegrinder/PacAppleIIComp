"""Sound data and click-timing model (TASK-032 analysis; development tool, runs the original code on the Python oracle).
The game has two speaker "tone engines" (all sound is BIT $C030 clicks timed by delay loops):
  engine A  $7BAC/$7BBC: a list of period bytes (0 ends it); each byte P makes 4 clicks, the delay between clicks is a loop of P passes.
  engine B  $7CF7/$7D0D: a list of (P, n) pairs (P = 0 ends it); n clicks per note, then a key poll ($6F5E) and a fixed pause ($7D7B).
This tool reads every sound list from the game, runs the real engine on the oracle, and compares the click times with a closed-form model:
  A:  click spacing inside a note 9*P+11 cycles, from the last click of a note to the first of the next 9*P_next+44 (+1 when the
      list read LDA abs,Y crosses a page: (address low byte + note index) > 255)
  B:  spacing inside a note 31*P+11, between notes  31*P_next + C_B   (C_B = key poll + fixed pause, measured once on the first pair, then checked on all)
Usage: python -I tools/sound_model.py   -> prints a table; writes scratch/sound_lists.json
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from play import make_machine

ORIGIN = 0x4000
# (name, how it is reached, engine, data address). Callers are the addresses that JSR the wrapper (read from the code, V by byte scan).
SOUNDS = [
    ('dot eaten', 'JSR $7BDC from $7B09', 'A', 0x7BE3),
    ('ghost eaten', 'JSR $7BE6 from $7DFC', 'A', 0x7BED),
    ('energizer eaten', 'JSR $7C45 from $7AC7', 'A', 0x7C4C),
    ('sound $7C61', 'JSR $7C61 from $6CD0', 'A', 0x7C68),
    ('table sound 6', 'ptr table $7C8F[2*$0A]', 'A', 0x7C9D),
    ('table sound 5', 'ptr table $7C8F[2*$0A]', 'A', 0x7CB0),
    ('table sound 4', 'ptr table $7C8F[2*$0A]', 'A', 0x7CBD),
    ('table sound 3', 'ptr table $7C8F[2*$0A]', 'A', 0x7CCA),
    ('table sound 2', 'ptr table $7C8F[2*$0A]', 'A', 0x7CD7),
    ('table sound 1', 'ptr table $7C8F[2*$0A]', 'A', 0x7CE4),
    ('bonus life', 'JSR $7C51 from $7132', 'C', 0x7C51),
    ('intro jingle', 'JSR $7D38 from $568A', 'B', 0x7D3F),
]


def read_list(body, addr, engine):
    off = addr - ORIGIN
    if engine == 'C':
        return [0x20] * 80                       # $7C51: 80 tones of period $20 through $7BAC
    if engine == 'A':
        out = []
        while body[off] != 0 and len(out) < 256:
            out.append(body[off])
            off += 1
        return out
    out = []
    while body[off] != 0 and len(out) < 128:
        out.append((body[off], body[off + 1]))
        off += 2
    return out


def oracle_clicks(mc, engine, addr, max_instr=3_000_000):
    """Run the real engine on the oracle; returns click cycle numbers relative to the first click."""
    m, cpu = mc.m, mc.cpu
    m[0xE8] = 0
    m[0x0300:0x0304] = bytes([0x20, 0, 0, 0xEA])
    entry = {'A': 0x7BBC, 'B': 0x7D0D, 'C': 0x7C51}[engine]
    m[0x0301], m[0x0302] = entry & 255, entry >> 8
    cpu.pc, cpu.sp = 0x0300, 0xFD
    cpu.a, cpu.y = addr & 255, addr >> 8
    mc.speaker.clear()
    mc.run(max_instr, stop_pc=0x0303)
    assert cpu.pc == 0x0303, 'engine did not return'
    c = list(mc.speaker)
    return [t - c[0] for t in c]


def model_clicks(engine, notes, c_between=None, addr=0):
    t, out = 0, []
    if engine == 'C':
        for k in range(80):
            t += (9 * 32 + c_between) if k else 0
            for i in range(4):
                if i:
                    t += 9 * 32 + 11
                out.append(t)
        return out
    if engine == 'A':
        for k, p in enumerate(notes):
            t += (9 * p + 44 + (1 if (addr & 255) + k > 255 else 0)) if k else 0
            for i in range(4):
                if i:
                    t += 9 * p + 11
                out.append(t)
        return out
    for k, (p, n) in enumerate(notes):
        t += (31 * p + c_between) if k else 0
        for i in range(n):
            if i:
                t += 31 * p + 11
            out.append(t)
    return out


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = make_machine()
    rows, ok_all = [], True
    for name, how, eng, addr in SOUNDS:
        notes = read_list(body, addr, eng)
        got = oracle_clicks(mc, eng, addr)
        # engine B / C: the constant between notes is measured on the first note boundary, then must hold for all the others
        if eng == 'C':
            c_between = got[4] - got[3] - 9 * 32
        elif eng == 'B':
            first_end = notes[0][1]
            c_between = (got[first_end] - got[first_end - 1]) - 31 * notes[1][0]
        else:
            c_between = None
        want = model_clicks(eng, notes, c_between, addr)
        ok = got == want
        ok_all &= ok
        rows.append({'name': name, 'how': how, 'engine': eng, 'address': addr, 'notes': len(notes), 'clicks': len(got), 'cycles': got[-1],
                     'seconds': round(got[-1] / 1022727, 3), 'distinct_periods': len({n[0] if eng == 'B' else n for n in notes}), 'model_equal': ok})
        print('%-16s eng %s @%04X notes %3d clicks %4d length %7d cycles (%.3f s)  model == oracle: %s' % (
            name, eng, addr, len(notes), len(got), got[-1], got[-1] / 1022727, ok) + ('  (between-note constant %d)' % c_between if c_between else ''))
    print('| all models equal:', ok_all)
    json.dump({'rows': rows}, open('scratch/sound_lists.json', 'w'))


if __name__ == '__main__':
    main()
