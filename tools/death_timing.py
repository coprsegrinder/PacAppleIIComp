"""TASK-033-3b: exact timing of every Pac-Man death (and of GAME OVER) on the oracle (development tool; original data from scratch/).
Random play (seed), lives topped up every 100 ticks (mode 'multi') or ONE life and no top-up (mode 'gameover'); a cycle-stamped hook records the drawing routines, the
death sounds and the tick starts. For every tick interval longer than 1,000,000 cycles it stores the offsets (from the tick start) of the events of the death sequence, Pac-Man's
place and the ghosts' places at the catch. After the last tick it stores what happens until the run ends (GAME OVER).
Writes scratch/deaths_<mode>_<seed>.json.   Usage: python -I tools/death_timing.py <seed> <ticks> [multi|gameover|forced|forced2] [players 1-5]   (TASK-033-6: players answers the players prompt; the file name gets _p<N>)"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import golden_trace as G

seed, ticks = int(sys.argv[1]), int(sys.argv[2])
mode = sys.argv[3] if len(sys.argv) > 3 else 'multi'
PLAYERS = int(sys.argv[4]) if len(sys.argv) > 4 else 1
if PLAYERS != 1:
    def _start(mc):
        from play import do_step
        for st in ('key:sp', '3_000_000', 'key:%d' % PLAYERS, '3_000_000', 'key:cr', '1_000_000'):
            do_step(mc, st, lambda *_: None)
    G.start_game = _start
name = 'DT%d' % seed
G.SCENARIOS[name] = dict(seed=seed, ticks=ticks, pokes=[])
inputs = G.make_inputs(seed, ticks)
pokes = [[t, 0x58, 0x83] for t in range(100, ticks, 100)] if mode in ('multi', 'forced') else [[5, 0x58, 0x81]]
if mode == 'forced2':   # other levels and other numbers of lives on the screen (the rebuild draws one icon per spare life and a fruit per level)
    pokes = [[5, 0x55, seed % 21 + 1]] + [[t, 0x58, 0x82 + (t // 100) % 4] for t in range(100, ticks, 100)]
NAMES = {0x69A1: 'mclear', 0x6A20: 'dispatch', 0x7C82: 'deathsound', 0x693A: 'tile', 0x696A: 'boxclear', 0x6EE2: 'img12x3', 0x8259: 'bigsprite', 0x55A6: 'blit', 0x7861: 'tick', 0x52BB: 'lores_fill', 0x6F46: 'pageclear', 0x6F2F: 'hires_on', 0x6F3C: 'text_on', 0x6172: 'dot', 0x5CF0: 'hline', 0x5D3D: 'vline', 0x53CF: 'load', 0x550A: 'hud', 0x5659: 'screen', 0x6113: 'dots', 0x5D53: 'walls', 0x8A7D: 'x8a7d', 0x70CC: 'score'}
ev = []
holder = {}
tickCount = [0]
orig_factory = G.factory


def factory():
    mc = orig_factory()
    step = mc.cpu.step

    def hook():
        pc = mc.cpu.pc
        n = NAMES.get(pc)
        if mode in ('forced', 'forced2') and pc == 0x7861 and mc.m[0xE8] == 0:
            tickCount[0] += 1
            if tickCount[0] % 60 == 30:   # put Pac-Man on a ghost: he is caught in this tick, at a place that depends on where the ghost is
                g = (tickCount[0] // 60) % 4
                mc.m[0x60], mc.m[0x61] = mc.m[0x84 + 20 * g], mc.m[0x85 + 20 * g]
        if n and (pc != 0x7861 or mc.m[0xE8] == 0):
            ev.append((mc.cpu.cycles, n, mc.cpu.a, mc.cpu.y, mc.m[0x0A], mc.m[0x60], mc.m[0x61], mc.m[0x0C] | mc.m[0x0D] << 8, mc.m[0x58], bytes(mc.m[0:256]) if pc == 0x7861 else None, bytes(mc.m[0x623A:0x65BA]) if pc == 0x7861 else None))
        return step()
    mc.cpu.step = hook
    holder['mc'] = mc
    return mc


G.factory = factory
G.run(name, inputs, pokes)
if mode == 'gameover':   # let the original run on after the GAME OVER text to see how long it waits and what comes next
    try:
        holder['mc'].run(60_000_000)
    except Exception as e:   # a wait for a key ends it
        print('continued run ended:', type(e).__name__, flush=True)
tk = [i for i, e in enumerate(ev) if e[1] == 'tick']
out = {'seed': seed, 'mode': mode, 'ticks': len(tk), 'deaths': []}
for a, b in zip(tk, tk[1:]):
    c, d = ev[a][0], ev[b][0]
    if d - c < 1_000_000:
        continue
    seg = ev[a + 1:b]
    snd = [e for e in seg if e[1] == 'deathsound']
    if len(snd) != 6:
        out['deaths'].append({'tick_index': tk.index(a), 'total': d - c, 'note': 'not a death (%d death sounds)' % len(snd), 'events': [(e[0] - c, e[1], e[2], e[3], e[4]) for e in seg if e[1] != 'blit'][:60]})
        continue
    frames = [e for e in seg if e[1] == 'dispatch' and e[4] in (6, 5, 4, 3, 2, 1) and e[0] >= snd[0][0] - 20000]
    out['deaths'].append({
        'tick_index': tk.index(a), 'total': d - c, 'pac': [snd[0][5], snd[0][6]], 'lives_before': ev[a][8], 'zp': ev[a][9].hex(), 'map': ev[a][10].hex(),
        'sounds': [e[0] - c for e in snd], 'frames': [e[0] - c for e in frames],
        'clears': [e[0] - c for e in seg if e[1] == 'mclear'],
        'tiles': [e[0] - c for e in seg if e[1] == 'tile'][:4], 'first_blit': [e[0] - c for e in seg if e[1] == 'blit'][:1],
        'last_event': seg[-1][0] - c, 'ready_blit': [e[0] - c for e in seg if e[1] == 'blit' and e[2] == 88][:1],
        'events': [(e[0] - c, e[1], e[2], e[3], e[4]) for e in seg],
        'dispatches': [(e[0] - c, e[2], e[3], e[4], e[7]) for e in seg if e[1] == 'dispatch']})
# what follows the last tick (GAME OVER when the last life was lost)
last = tk[-1]
c = ev[last][0]
out['zp_last_tick'] = ev[last][9].hex()
out['after_last_tick'] = [(e[0] - c, e[1], e[2], e[3], e[4]) for e in ev[last + 1:] if e[1] in ('lores_fill', 'pageclear', 'hires_on', 'text_on', 'deathsound') or (e[1] == 'blit' and e[2] >= 190) or e[1] == 'mclear'][:3000]
out['end_cycle_offset'] = ev[-1][0] - c
json.dump(out, open('scratch/deaths_%s_%d%s.json' % (mode, seed, '_p%d' % PLAYERS if PLAYERS != 1 else ''), 'w'))
print(mode, seed, 'ticks', len(tk), 'deaths', len([d for d in out['deaths'] if 'pac' in d]), 'other long intervals', len([d for d in out['deaths'] if 'pac' not in d]), flush=True)
