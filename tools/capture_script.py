"""TASK-028f: record the COMPLETE ordered sequence of drawing calls of one cartoon (parameters only, no pages) plus the SHA-256 of the final hi-res page,
so the C++ layer can replay a whole composed screen sequence and compare the final page. Development tool; the output stays local (scratch/, git-ignored).
Reuses the Collector of tools/capture_hires_vectors.py with its de-duplication switched off. Page clear ($6F46) is recorded as type 10.
Format: 'CSCR', u32 count, 32 bytes final-page SHA-256, then per call: u8 type + 8 x i32 parameters (types as in capture_hires_vectors.py).
Usage: python -I tools/capture_script.py [cartoon number, default 1]   (about 2-3 minutes)"""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
import capture_hires_vectors as H
from cov_close import factory


class Always(set):
    def __contains__(self, k):
        return False

    def __len__(self):
        return 0


def main(n=1):
    mc = factory()
    col = H.Collector(mc)
    col.seen = {t: Always() for t in range(1, 10)}
    H.LIMIT = 10 ** 9
    inner = col.hook_factory()
    clears = []

    def hook():
        if mc.cpu.pc == 0x6F46:
            clears.append(len(col.records) + len(col.pending))
        return inner()
    mc.cpu.step = hook
    H.cartoon(col, n)
    page = bytes(mc.m[0x2000:0x4000])
    open('scratch/cartoon%d_final_page.bin' % n, 'wb').write(page)  # the original's final page (local only), for the window test
    recs = list(col.records)
    out = []
    ci = 0
    for i, r in enumerate(recs):
        while ci < len(clears) and clears[ci] <= i:
            out.append((10, [0] * 8))
            ci += 1
        out.append((r[0], r[1]))
    with open('scratch/cartoon%d_script.bin' % n, 'wb') as f:
        f.write(b'CSCR' + struct.pack('<I', len(out)) + hashlib.sha256(page).digest())
        for t, p in out:
            f.write(struct.pack('<B8i', t, *p))
    import collections
    print('calls', len(out), dict(collections.Counter(t for t, _ in out)), 'clears', len(clears), 'pending', len(col.pending), flush=True)


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
