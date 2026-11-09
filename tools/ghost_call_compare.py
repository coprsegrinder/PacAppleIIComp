"""TASK-034-TIMING: the port's cycles of every call of $8763 (scratch/ghostcalls_port.csv, written by the test attract_demo_tick_cost_report) against the oracle's
(scratch/ghostcalls_total.json and scratch/ghostcalls.json from tools/ghost_call_profile.py), without the drawing routines. Prints how many calls are equal and, for the others, the
paths of the original that go wrong with the mean error. Development tool.   Usage: python -I tools/ghost_call_compare.py"""
import collections, csv, json
tot = json.load(open('scratch/ghostcalls_total.json'))
full = json.load(open('scratch/ghostcalls.json'))
mine = [(int(r['tick']), int(r['ghost']), int(r['mine'])) for r in csv.DictReader(open('scratch/ghostcalls_port.csv'))]
n = min(len(tot), len(mine))
bad = collections.defaultdict(list)
equal = 0
for i in range(n):
    g, t, d, k = tot[i]
    logic = t - d
    tk, gm, m = mine[i]
    assert g == gm and k == tk, (i, g, gm, k, tk)
    if m == logic:
        equal += 1
    else:
        bad[full[i][2]].append((m - logic, i))
print('calls', n, 'equal', equal, 'differ', n - equal, 'distinct paths that differ', len(bad))
for path, errs in sorted(bad.items(), key=lambda kv: -len(kv[1]))[:25]:
    es = collections.Counter(e for e, _ in errs)
    print('%5d x  err %s  first call %d  path %s' % (len(errs), dict(es.most_common(4)), errs[0][1], path[:60]))
