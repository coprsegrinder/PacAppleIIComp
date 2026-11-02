"""Merge apple2tc run-data files (tools/a2tc_scenarios.py outputs): union of branch targets and branches; generation 0 once, then the other
generations of every file, identical ones (same code segments) only once.
Usage: python -I tools/a2tc_merge.py out.json in1.json in2.json ..."""
import json, sys

files = [json.load(open(p)) for p in sys.argv[2:]]
targets, edges, gens, seen = set(), {}, [files[0]['generations'][0]], set()
for f in files:
    targets |= set(f['BranchTargets'])
    for k, v in f['Branches'].items():
        edges.setdefault(int(k), set()).update(v)
    for g in f['generations'][1:]:
        key = json.dumps(g['code'], sort_keys=True)
        if key not in seen:
            seen.add(key)
            gens.append(g)
bs = dict(files[0]['BaseStats'])
bs['limit'] = sum(f['BaseStats']['limit'] for f in files)
out = {'BaseStats': bs, 'BranchTargets': sorted(targets), 'Branches': {str(k): sorted(v) for k, v in sorted(edges.items())}, 'generations': gens}
json.dump(out, open(sys.argv[1], 'w'), separators=(',', ':'))
print('merged %d files: %d targets, %d branch origins, %d generations (%d distinct code states)' % (len(files), len(targets), len(edges), len(gens), len(seen)))
