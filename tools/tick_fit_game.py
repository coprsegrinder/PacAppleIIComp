"""TASK-034-TIMING: what a tick of a GAME costs beyond the drawing routines, from scratch/tickfit_joy.csv (written by tests/test_gamedriver.cpp with --joyticks; development tool).
glue = oracle - 2,987 (the idle tick of a game with the keyboard poll) - input (the joystick poll's change) - exact (the drawing routines, attract.h drawCallCycles).
Prints the glue by group of tick and a least-squares fit over the features of the tick.   Usage: python -I tools/tick_fit_game.py [csv]"""
import csv, sys
import numpy as np
rows = list(csv.DictReader(open(sys.argv[1] if len(sys.argv) > 1 else 'scratch/tickfit_joy.csv')))
R = [{k: int(v) for k, v in r.items()} for r in rows]
for r in R:
    r['glue'] = r['oracle'] - 2987 - r['input'] - r['exact'] - (20 if r['z59'] == 1 and r['e5'] != 0 and r['e7'] == 0 else 0)
R = [r for r in R if r['oracle'] < 100000]
print('ticks', len(R))
feat = ['draws', 'acted', 'ate', 'energizer', 'moved', 'eaten', 't1', 't2', 't3', 't4', 't5', 't6', 't9', 't10', 't11', 't12']
for r in R:
    r['draws'] = 1 if r['ncalls'] else 0
X = np.array([[r[f] for f in feat] for r in R], float)
y = np.array([r['glue'] for r in R], float)
w, *_ = np.linalg.lstsq(X, y, rcond=None)
res = y - X @ w
print('fit:', ', '.join('%s %.1f' % (f, c) for f, c in zip(feat, w)))
print('glue sd %.1f -> residual sd %.1f' % (y.std(), res.std()))
import collections
for name, sel in (('no draw', lambda r: r['ncalls'] == 0), ('draw, pac-man steps', lambda r: r['ncalls'] and r['acted']), ('draw, ghosts only', lambda r: r['ncalls'] and not r['acted'])):
    g = [r['glue'] for r in R if sel(r)]
    print('%-22s n %5d mean %8.1f sd %7.1f min %6d max %6d' % (name, len(g), np.mean(g), np.std(g), min(g), max(g)))
