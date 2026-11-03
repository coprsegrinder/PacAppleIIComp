"""TASK-033-3b: analyse scratch/deaths_*.json: which parts of the death sequence are constant, which depend on Pac-Man's shift (x mod 7) or on other things.
Usage: python -I tools/death_model_analyse.py"""
import glob, json, collections, statistics
D = []
for f in sorted(glob.glob('scratch/deaths_forced_4*.json')) + sorted(glob.glob('scratch/deaths_multi_4*.json')):
    D += [d for d in json.load(open(f))['deaths'] if 'events' in d]
print(len(D), 'deaths with full events')
rows = []
for d in D:
    ev = d['events']
    times = [e[0] for e in ev]
    gap = [i for i in range(len(times) - 1) if times[i + 1] - times[i] > 700000]
    own = times[gap[0]]
    after = [(e[0] - own, e[1], e[2], e[3], e[4]) for e in ev if e[0] > own]
    rows.append(dict(pac=tuple(d['pac']), own=own, after=after, total=d['total'] - own, zp=bytes.fromhex(d['zp'])))
# do all deaths have the same event name sequence after the stillness?
sig = collections.Counter(tuple(e[1] for e in r['after'][:60]) for r in rows)
print('distinct event-name sequences (first 60 events):', len(sig))
n0 = min(len(r['after']) for r in rows)
print('events after the first gap: min', n0, 'max', max(len(r['after']) for r in rows))
# index of READY (first blit with a == 88)
def idx(r, pred):
    for i, e in enumerate(r['after']):
        if pred(e): return i
iready = [idx(r, lambda e: e[1] == 'blit' and e[2] == 88) for r in rows]
print('READY blit index', collections.Counter(iready).most_common(5))
# per event index (up to READY): spread inside groups of equal shift
byS = collections.defaultdict(list)
for r in rows: byS[r['pac'][0] % 7].append(r)
worst = []
for i in range(min(iready)):
    spread = 0
    for s, rs in byS.items():
        v = [r['after'][i][0] for r in rs]
        spread = max(spread, max(v) - min(v))
    worst.append(spread)
print('max spread of each event time inside one shift group (first 60 events):', worst[:60])

# ---- landmark analysis ----
def landmarks(r):
    a = r['after']
    L = {}
    clears = [i for i, e in enumerate(a) if e[1] == 'mclear']
    for k in range(5): L['c%d' % (k + 1)] = a[clears[k]][0]
    imgi = next(i for i, e in enumerate(a) if e[1] == 'dispatch' and i > clears[4])
    L['img'] = a[imgi][0]
    L['c6'] = a[next(i for i in clears if i > imgi)][0]
    for k in range(1, 7):
        fi = next(i for i, e in enumerate(a) if e[1] == 'dispatch' and e[4] == k)
        L['f%d' % (7 - k)] = a[fi][0]
        L['s%d' % (7 - k)] = next(e[0] for e in a if e[1] == 'deathsound' and e[4] == k)
        L['m%d' % (7 - k)] = next(e[0] for i, e in enumerate(a) if e[1] == 'mclear' and e[4] == k and i > fi)
    last_m = L['m6']
    ti = next(i for i, e in enumerate(a) if e[1] in ('tile', 'boxclear') and e[0] > last_m)
    L['rebuild'] = a[ti][0]
    bl = [e for e in a if e[1] == 'blit' and e[2] == 88]
    L['ready'] = bl[0][0]
    L['last_blit'] = [e for e in a if e[1] == 'blit'][-1][0]
    L['total'] = r['total']
    return L
LM = [landmarks(r) for r in rows]
names = list(LM[0].keys())
print('landmark offsets from the end of the catching tick (min .. max; distinct values)')
for n in names:
    v = [l[n] for l in LM]
    print('  %-9s %9d .. %9d  %3d distinct' % (n, min(v), max(v), len(set(v))))
print('differences between landmarks (min .. max):')
for p, q in [('c1','c2'),('c2','c3'),('c3','c4'),('c4','c5'),('c5','img'),('img','c6'),('c6','f1'),('f1','s1'),('f1','m1'),('m1','f2'),('f2','f3'),('f3','f4'),('f4','f5'),('f5','f6'),('f6','m6'),('m6','rebuild'),('rebuild','ready'),('ready','last_blit'),('last_blit','total')]:
    v = [l[q] - l[p] for l in LM]
    print('  %-8s -> %-8s %9d .. %9d  %3d distinct' % (p, q, min(v), max(v), len(set(v))))
import pickle
pickle.dump((rows, LM), open('scratch/death_landmarks.pkl', 'wb'))
