"""TASK-034-COVERAGE: turns scratch/linecov.tsv (tools/linecov, made by tools/coverage_cpp.sh) into docs/CODE_COVERAGE_PORT.md: per source file of the port, the lines with code, the lines
that ran in the full test run, and the ranges that did not. A line with code is a line the PDB lists; a line counts as covered when any of its instructions ran.
Usage: python -I tools/coverage_report.py [tsv] [out.md]"""
import collections, os, sys

tsv = sys.argv[1] if len(sys.argv) > 1 else 'scratch/linecov.tsv'
out = sys.argv[2] if len(sys.argv) > 2 else 'docs/CODE_COVERAGE_PORT.md'
files = collections.defaultdict(dict)
for row in open(tsv, encoding='utf-8'):
    f, line, hits = row.rstrip('\n').split('\t')
    key = f.split('\\port\\src\\')[-1].replace('\\', '/')
    files[key][int(line)] = max(files[key].get(int(line), 0), int(hits))


def ranges(nums):
    nums = sorted(nums)
    res, i = [], 0
    while i < len(nums):
        j = i
        while j + 1 < len(nums) and nums[j + 1] <= nums[j] + 3:   # lines up to 2 apart (blank or comment lines between) are one range
            j += 1
        res.append(str(nums[i]) if i == j else '%d-%d' % (nums[i], nums[j]))
        i = j + 1
    return res


groups = collections.OrderedDict([('core', 'Game logic (src/core)'), ('app', 'Windows program (src/app)'), ('hires', 'Hi-res drawing (src/hires)'), ('video', 'Colour and monochrome picture (src/video)'),
                                  ('datapack', 'Data pack (src/datapack)'), ('verify', 'Disk image check (src/verify)')])
lines = ['# C++ line coverage of the port', '',
         'Made by `tools/coverage_cpp.sh` (runs the whole test suite with every optional recording under `tools/linecov`, a small debugger-based runner) and `tools/coverage_report.py`. '
         'Microsoft dotnet-coverage was tried first and cannot measure a native program (its native instrumentation is a Visual Studio component). A **line with code** is a line that the PDB lists; it is '
         '**covered** when any instruction of it ran during the test run. Line coverage says a line ran, not that its result was checked: the equality tests against the oracle are what check results.', '']
tot = cov = 0
rows = []
details = []
for name in sorted(files):
    d = files[name]
    n, c = len(d), sum(1 for h in d.values() if h)
    tot += n
    cov += c
    rows.append((name, n, c))
    miss = [l for l, h in d.items() if not h]
    if miss:
        details.append('* `%s` (%d of %d lines not run): %s' % (name, len(miss), n, ', '.join(ranges(miss))))
lines.append('**Total: %d of %d lines with code ran (%.1f %%).**' % (cov, tot, 100.0 * cov / max(1, tot)))
lines.append('')
for g, title in groups.items():
    sel = [r for r in rows if r[0].startswith(g + '/')]
    if not sel:
        continue
    gn, gc = sum(r[1] for r in sel), sum(r[2] for r in sel)
    lines.append('## %s: %d of %d lines (%.1f %%)' % (title, gc, gn, 100.0 * gc / max(1, gn)))
    lines.append('')
    lines.append('| File | Lines | Ran | % |')
    lines.append('|---|---|---|---|')
    for name, n, c in sel:
        lines.append('| `%s` | %d | %d | %.1f |' % (name, n, c, 100.0 * c / max(1, n)))
    lines.append('')
lines.append('## Lines that did not run')
lines.append('')
lines.extend(details if details else ['None.'])
lines.append('')
open(out, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
print('%d of %d lines ran (%.1f %%) in %d files -> %s' % (cov, tot, 100.0 * cov / max(1, tot), len(rows), out))
