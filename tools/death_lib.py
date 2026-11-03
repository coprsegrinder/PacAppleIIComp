"""TASK-033-3b: reading the death recordings of tools/death_timing.py (development tool; original data from scratch/).
load() returns one dict per death with the landmarks of the sequence (cycles after the end of the catching tick's own drawing) and the game state at its start."""
import glob, json


def load(patterns=('scratch/deaths_forced_4*.json', 'scratch/deaths_multi_4*.json', 'scratch/deaths_forced2_*.json')):
    out = []
    for pat in patterns:
        for f in sorted(glob.glob(pat)):
            for d in json.load(open(f))['deaths']:
                if 'events' not in d:
                    continue
                r = landmarks(d)
                r['file'] = f
                out.append(r)
    return out


def landmarks(d):
    ev = d['events']
    t = [e[0] for e in ev]
    gap = [i for i in range(len(t) - 1) if t[i + 1] - t[i] > 700000]
    own = t[gap[0]]
    a = [(e[0] - own, e[1], e[2], e[3], e[4]) for e in ev if e[0] > own]
    L = {}
    clears = [i for i, e in enumerate(a) if e[1] == 'mclear']
    for k in range(5):
        L['c%d' % (k + 1)] = a[clears[k]][0]
    imgi = next(i for i, e in enumerate(a) if e[1] == 'dispatch' and i > clears[4])
    L['img'] = a[imgi][0]
    L['c6'] = a[next(i for i in clears if i > imgi)][0]
    for k in range(1, 7):
        fi = next(i for i, e in enumerate(a) if e[1] == 'dispatch' and e[4] == k)
        L['f%d' % (7 - k)] = a[fi][0]
        L['s%d' % (7 - k)] = next(e[0] for e in a if e[1] == 'deathsound' and e[4] == k)
        L['m%d' % (7 - k)] = next(e[0] for i, e in enumerate(a) if e[1] == 'mclear' and e[4] == k and i > fi)
    ti = next(i for i, e in enumerate(a) if e[1] in ('tile', 'boxclear') and e[0] > L['m6'])
    L['rebuild'] = a[ti][0]
    L['ready'] = next(e[0] for e in a if e[1] == 'blit' and e[2] == 88)
    L['last_blit'] = [e[0] for e in a if e[1] == 'blit'][-1]
    L['total'] = d['total'] - own
    z = bytes.fromhex(d['zp'])
    return dict(L=L, own=own, pac=tuple(d['pac']), zp=z, tick_total=d['total'])
