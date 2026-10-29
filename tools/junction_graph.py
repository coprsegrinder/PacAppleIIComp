"""Decode and check the maze junction graph at $760A (64 records x 8 bytes) against the ghost decision routines.
Record: X, Y, neighbour junction index for direction 0 (down), 1 (up), 2 (right), 3 (left) ($FF = no exit), then two bytes of unknown meaning.
Usage: python -I tools/junction_graph.py     (needs scratch/extract/PAC-MAN.1000.bin and scratch/ghost_routines.json from tools/ghost_routines.py)
Development tool; original bytes are only read, results are printed as counts.
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))

ORIGIN = 0x4000
TABLE, COUNT, RECORD = 0x760A, 64, 8
INDEX, WORDS = 0x8CBE, 0x8DBE
NONE = 0xFF
DOWN, UP, RIGHT, LEFT = 0, 1, 2, 3
OPPOSITE = {DOWN: UP, UP: DOWN, RIGHT: LEFT, LEFT: RIGHT}


def parse_records(body, table=TABLE, count=COUNT, origin=ORIGIN):
    out = []
    for i in range(count):
        r = body[table - origin + i * RECORD: table - origin + (i + 1) * RECORD]
        out.append({'x': r[0], 'y': r[1], 'next': list(r[2:6]), 'extra': list(r[6:8])})
    return out


def exits(record):
    return {d for d in range(4) if record['next'][d] != NONE}


def check_graph(recs):
    """Returns a dict of problem lists: asymmetric links, links to missing junctions, geometry violations."""
    problems = {'missing_target': [], 'not_symmetric': [], 'geometry': []}
    for j, r in enumerate(recs):
        for d in range(4):
            n = r['next'][d]
            if n == NONE:
                continue
            if n >= len(recs):
                problems['missing_target'].append((j, d, n))
                continue
            t = recs[n]
            if t['next'][OPPOSITE[d]] != j:
                problems['not_symmetric'].append((j, d, n))
            ok = {DOWN: t['x'] == r['x'] and t['y'] > r['y'], UP: t['x'] == r['x'] and t['y'] < r['y'],
                  RIGHT: t['y'] == r['y'] and t['x'] > r['x'], LEFT: t['y'] == r['y'] and t['x'] < r['x']}[d]
            if not ok:
                problems['geometry'].append((j, d, n))
    return problems


def routine_exit_check(recs, index_table, routine_outputs):
    """For every junction and ghost: are all answers the routine can give among that junction's real exits?
    routine_outputs[r] = set of directions routine r can return. Returns (checked, violations list)."""
    bad, checked = [], 0
    for j, r in enumerate(recs):
        allowed = exits(r)
        for g in range(4):
            routine = index_table[j * 4 + g]
            checked += 1
            if not routine_outputs[routine] <= allowed:
                bad.append((j, g + 1, routine, sorted(routine_outputs[routine] - allowed)))
    return checked, bad


def follow_table(recs, table, doors=(23, 24)):
    """Follow `table[junction]` (a direction per junction) from every junction.
    Returns {junction: steps to a door junction, or None if the walk is not a real exit or loops}."""
    result = {}
    for start in range(len(recs)):
        j, steps, seen = start, 0, set()
        while True:
            if j in doors:
                result[start] = steps
                break
            if j in seen or table[j] > 3 or recs[j]['next'][table[j]] == NONE:
                result[start] = None
                break
            seen.add(j)
            j = recs[j]['next'][table[j]]
            steps += 1
    return result


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    recs = parse_records(body)
    index_table = body[INDEX - ORIGIN:INDEX - ORIGIN + 256]
    p = check_graph(recs)
    links = sum(len(exits(r)) for r in recs)
    print('junctions: %d | directed links: %d | grid columns %d, rows %d' % (len(recs), links, len({r['x'] for r in recs}), len({r['y'] for r in recs})))
    print('graph checks: links to missing junctions %d, not symmetric %d, geometry violations %d' % tuple(len(p[k]) for k in ('missing_target', 'not_symmetric', 'geometry')))
    data = json.load(open('scratch/ghost_routines.json'))
    outputs = {}
    for row in data['rows']:
        outputs[row['routine']] = {v for vals in row['outputs_by_dir'].values() for v in vals}
    checked, bad = routine_exit_check(recs, index_table, outputs)
    print('routine answers inside the junction exits: %d of %d (junction, ghost) pairs; violations: %d' % (checked - len(bad), checked, len(bad)))
    for v in bad[:12]:
        print('   junction %d ghost %d routine %d can also answer %s (not an exit)' % v)
    degree = {}
    for r in recs:
        degree[len(exits(r))] = degree.get(len(exits(r)), 0) + 1
    print('junctions by number of exits:', dict(sorted(degree.items())))
    shared = {}
    for j in range(COUNT):
        shared.setdefault(tuple(index_table[j * 4:j * 4 + 4]), []).append(j)
    print('(junction -> routines) patterns: %d distinct' % len(shared))
    table = list(body[0xA360 - ORIGIN:0xA360 - ORIGIN + COUNT])
    walk = follow_table(recs, table)
    good = {j: n for j, n in walk.items() if n is not None}
    print('eyes table $A360: %d of %d junctions lead to a house-door junction (23/24) by following the table; longest walk %d junctions; broken: %s' % (
        len(good), len(walk), max(good.values()), [j for j, n in walk.items() if n is None]))


if __name__ == '__main__':
    main()
