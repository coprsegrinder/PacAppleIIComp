"""TASK-034-TIMING: the port's call costs of $8763 in a scenario (scratch/tickfit_ticks_<X>.bin.csv.ghosts, written by the keyboard cost test) against the oracle's
(scratch/ghostcalls_<X>.json from tools/ghost_call_profile_scenario.py). Prints the calls that differ with the oracle's path.   Usage: python -I tools/ghost_call_compare_scenario.py <X>"""
import collections, csv, json, sys
X = sys.argv[1]
orc = json.load(open('scratch/ghostcalls_%s.json' % X))
mine = [(int(r['tick']), int(r['ghost']), int(r['mine'])) for r in csv.DictReader(open('scratch/tickfit_ticks_%s.bin.csv.ghosts' % X))]
by = {}
for g, t, d, k, path in orc:
    by.setdefault((k + 1, g), (t - d, path))   # (the replay hooks in at the first tick: its numbering starts one later)
bad = collections.defaultdict(list)
n = eq = 0
for k, g, m in mine:
    if (k, g) not in by:
        continue
    n += 1
    logic, path = by[(k, g)]
    if logic == m:
        eq += 1
    else:
        bad[path].append((m - logic, k, g))
print('calls compared', n, 'equal', eq, 'differ', n - eq)
for path, errs in sorted(bad.items(), key=lambda kv: -len(kv[1]))[:12]:
    print(len(errs), collections.Counter(e for e, _, _ in errs).most_common(3), 'first tick %d ghost %d' % (errs[0][1], errs[0][2]), path[:60])
# the drawing routines of the ghost calls ($6A20, $699E, $6172, $5CF0), per tick
dsum = collections.defaultdict(int)
for g, t, d, k, path in orc:
    dsum[k + 1] += d
mine_draw = {int(r['tick']): int(r['mine']) for r in csv.DictReader(open('scratch/tickfit_ticks_%s.bin.csv.ghosts' % X)) if int(r['ghost']) == 0}
off = [(k, mine_draw[k] - dsum[k]) for k in mine_draw if k in dsum and mine_draw[k] != dsum[k]]
print('ticks whose ghost drawing differs:', len(off), off[:10])
