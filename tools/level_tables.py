"""Catalogue of every level-indexed table found so far (TASK-031 analysis; development tool, reads original data from scratch/).
All 16-entry tables are indexed by min(level, 15) where the level starts at 1 (entry 0 is never used in play; $6D1E),
the frightened-time table by min(level, 21) ($6D27), the fruit table by min(level, 26) ($6A7C, 4 bytes per entry).
Usage: python -I tools/level_tables.py   -> prints a summary (shape of each table) and writes scratch/level_tables.json"""
import json, sys

ORIGIN = 0x4000
FRUIT_SPRITE0, FRUIT_STRIDE = 27502, 36
# (name, address, entries, index clamp, who reads it, role, V = role confirmed by running, I = from code)
TABLES = [
    ('pacman_delay', 0x7851, 16, 15, '$7874', 'Pac-Man step countdown base ($59)', 'V'),
    ('dot_penalty_now', 0x7A8C, 16, 15, '$7AAD', 'extra delay added to $59 when a dot/energizer is eaten', 'I'),
    ('dot_penalty_next', 0x7A9C, 16, 15, '$7AAD', 'value loaded in $F3 (extra delay on the next steps)', 'I'),
    ('house_ghost2', 0x54A4, 16, 15, '$5420', 'house countdown of ghost 2', 'V'),
    ('house_ghost3', 0x54B4, 16, 15, '$5420', 'house countdown of ghost 3', 'V'),
    ('house_ghost4', 0x54C4, 16, 15, '$5420', 'house countdown of ghost 4', 'V'),
    ('ghost_delay_normal', 0x8723, 16, 15, '$8780', 'ghost step delay, normal', 'I'),
    ('ghost_delay_turn', 0x8733, 16, 15, '$8802', 'ghost step delay right after a change of direction', 'I'),
    ('ghost_delay_frightened', 0x8743, 16, 15, '$8780', 'ghost step delay, frightened', 'I'),
    ('ghost_delay_eyes', 0x8753, 16, 15, '$8780', 'ghost step delay, eyes', 'I'),
    ('eaten_timer_extension', 0x6E19, 16, 15, '$8BA4', 'added to the frightened timer when a ghost is eaten (words)', 'I'),
    ('energizer_blink', 0x5733, 16, 15, '$5716', 'energizer blink period (in Pac-Man ticks)', 'I'),
    ('player_blink', 0x5903, 16, 15, '$58C7', '1UP/player indicator blink period (in Pac-Man ticks)', 'I'),
    ('fruit_score_hundreds', 0x6CD4, 16, 15, '$6CB6', 'fruit score in hundreds (BCD)', 'V'),
    ('frightened_time', 0x6DEF, 22, 21, '$6D84', 'frightened time in ticks (words)', 'V'),
]


def read(body, addr, n, word=False):
    o = addr - ORIGIN
    if word:
        return [body[o + 2 * i] | body[o + 2 * i + 1] << 8 for i in range(n)]
    return list(body[o:o + n])


def fruit_schedule(body):
    rows = []
    for lv in range(0, 27):
        e = read(body, 0x6AFE + 4 * lv, 4)
        ptr, life = e[0] | e[1] << 8, e[2] | e[3] << 8
        rows.append({'level': lv, 'fruit_index': (ptr - FRUIT_SPRITE0) // FRUIT_STRIDE, 'sprite_ptr': ptr, 'lifetime_ticks': life})
    return rows


def main():
    body = open('scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    out = {'tables': {}, 'fruit': fruit_schedule(body)}
    for name, addr, n, clamp, who, role, conf in TABLES:
        word = name in ('eaten_timer_extension', 'frightened_time')
        vals = read(body, addr, n, word)
        out['tables'][name] = {'address': addr, 'values': vals, 'clamp': clamp, 'role': role, 'confidence': conf}
        used = vals[1:clamp + 1]
        mono = 'non-increasing' if all(a >= b for a, b in zip(used, used[1:])) else 'non-decreasing' if all(a <= b for a, b in zip(used, used[1:])) else 'not monotonic'
        print('%-24s $%04X %2d entries (levels 1..%d) level1=%-5d last=%-5d distinct=%2d %s' % (name, addr, n, clamp, vals[1], vals[clamp], len(set(used)), mono))
    f = out['fruit']
    print('fruit table: 27 entries x 4 bytes; fruit kinds', len({r['fruit_index'] for r in f[1:]}), '; first level of each kind',
          [min(r['level'] for r in f[1:] if r['fruit_index'] == k) for k in sorted({r['fruit_index'] for r in f[1:]})],
          '; lifetime range', min(r['lifetime_ticks'] for r in f[1:]), '-', max(r['lifetime_ticks'] for r in f[1:]))
    json.dump(out, open('scratch/level_tables.json', 'w'))


if __name__ == '__main__':
    main()
