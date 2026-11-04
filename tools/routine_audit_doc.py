"""TASK-033-R: writes docs/ROUTINE_AUDIT.md from scratch/audit.json (made by tools/routine_audit.py) and the group table below.
Usage: python -I tools/routine_audit.py > scratch/audit.txt && python -I tools/routine_audit_doc.py"""
import json

rows = json.load(open('scratch/audit.json'))

# (first, last, name, port module, status, evidence)
G = [
 (0x4027, 0x42CF, 'Start-up: boot menus, prompts, hidden menu, text output ($4027, $4144-$4A6A)', 'attractflow.cpp, monitor.cpp, textscreen.cpp', 'EXACT',
  'real monitor ROM runs in oracle and port: test_boot, test_hidden, test_flow (--boot --hidden --prompts), test_monitor, test_textscreen'),
 (0x42D0, 0x4820, 'Attract turn: title, logo, chase and demo scripts ($42D0-$4689, $44E8-$46CC, $5260-$52FC)', 'attract.cpp, attractflow.cpp, cartoon.cpp', 'EXACT + MODELLED',
  'scripts byte/cycle-equal (test_attract, test_hires); the demo ticks use the fitted tick-cost model (total +1,042 cycles of 39.2 M)'),
 (0x52FC, 0x5658, 'Level, life and player set-up, memory saves, HUD of the players ($5303-$5659, $550A)', 'lifestart.cpp, gamecore.cpp, lives.cpp, screen.cpp, attract.cpp', 'EXACT',
  'test_lifestart, test_players (1-5 players, rotation, saves), test_screen'),
 (0x5659, 0x6172, 'Maze, dots, items, score digits drawing ($5659-$6172, $5CE7-$5D6E)', 'screen.cpp, tickdraw.cpp, hires.cpp, attract.cpp', 'EXACT',
  'test_hires, test_dots, test_screen, test_tickdraw: pictures equal the oracle byte for byte'),
 (0x6173, 0x6A7B, 'Sprite and text drawing helpers ($61B6, $693A-$6A20, $6AEB, $6AFB)', 'hires.cpp, tickdraw.cpp, attract.cpp', 'EXACT',
  'test_hires, test_hires_vectors, test_tickdraw, test_script'),
 (0x6A7C, 0x6D2F, 'Fruit, eating, delays ($6A7C, $6CB2, $6CE6-$6CF4, $6D27)', 'eating.cpp, gamecore.cpp, cycleclock.cpp', 'EXACT',
  'test_eating, test_gamecore; busy-wait delays ($6CE6, $7BAC, $7CF7) are cycle counts in the clock/cost model (not run as loops)'),
 (0x6D30, 0x6F5D, 'Ghost timers, modes, house exit ($6D30-$6F2F)', 'ghostmode.cpp, ghosttick.cpp, ghostjunction.cpp, steptimer.cpp, gamecore.cpp', 'EXACT',
  'test_ghostmove, test_ghostjunction, test_ghosthead, test_steptimer, test_ghostworld'),
 (0x6F5E, 0x7090, 'Keyboard and digital 4-switch input; sound switch ($6F5E, $6F77, $6FD0); joystick/paddle ($701E, $7041, $7053, $706B)', 'input.cpp', 'EXACT; JOYSTICK NOT PORTED',
  'test_input (keyboard + 16 switch masks); the analogue paddle reader ($701E, $7041, range checks $7053/$706B) is NOT rebuilt: input.cpp takes a ready direction (no hardware source on a PC)'),
 (0x7091, 0x7160, 'Lives, score, bonus life, HUD text ($7091-$7159)', 'lives.cpp, eating.cpp, screen.cpp, tickdraw.cpp', 'EXACT',
  'test_lives, test_eating, test_screen'),
 (0x7161, 0x7B2E, 'Game tick, Pac-Man move, junctions ($75F1, $7819, $7861, $79A4-$7AAC)', 'gamecore.cpp, pacstep.cpp, junction.cpp, input.cpp', 'EXACT',
  'test_pacstep, test_junction, test_ghostmove, test_gamecore (recorded runs equal tick for tick)'),
 (0x7B2F, 0x7DB7, 'Sound lists and speaker engines ($7BAC-$7D9A)', 'sound.cpp, squarewave.cpp', 'EXACT click lists; PACING MODELLED',
  'test_sound: every list equal the oracle clicks; the busy-loop engines ($7BAC, $7CF7) become click times'),
 (0x7DB8, 0x80C0, 'Ghost eaten freeze, score pop-up, ghost mode switch ($7DB8-$80C1)', 'eating.cpp, ghostmode.cpp, gamecore.cpp, tickdraw.cpp', 'EXACT',
  'test_eating, test_ghostmove; $806F-$807C closed on the oracle (TASK-021b)'),
 (0x80C1, 0x86E9, 'Cartoons (intermissions) ($8155-$84F9)', 'cartoon.cpp', 'EXACT',
  'cartoon models equal the oracle call for call (docs/CARTOONS.md)'),
 (0x86EA, 0x8B12, 'Ghost movement, cell counters, junction logic ($86EA-$8A9C)', 'ghosttick.cpp, ghostjunction.cpp, tickdraw.cpp', 'EXACT',
  'test_ghostmove, test_ghostjunction, test_ghosthead'),
 (0x8B13, 0x8CBD, 'Contact test, death, life loss, player rotation ($8B13-$8C50, $8CA6)', 'gamecore.cpp, lives.cpp, deathtiming.cpp, tickdraw.cpp', 'EXACT; DEATH TIMING MODELLED',
  'test_lives, test_players (rotation), test_deathtiming (frame costs measured; ~0-66 cycles/frame page-crossing residual)'),
 (0x8CBE, 0x96FF, 'Ghost decision routines: 48 table routines + index table ($8E1E-$96C0)', 'ghostdecide_gen.cpp (generated from the table)', 'EXACT',
  'test_ghostdecide: all 48 over complete class tables; 40 executed in the first run, the other 8 closed in TASK-021'),
]


def group(addr):
    if 0x4821 <= addr <= 0x4A7F:
        return G[0]
    if 0x5260 <= addr <= 0x52FB:
        return G[1]
    for g in G:
        if g[0] <= addr <= g[1]:
            return g
    return None


out = []
w = out.append
w('# Routine-by-routine audit of the original code (TASK-033-R, 2026-11-04)')
w('')
w('Question: is all the code of the original program implemented in the port, and how well is each part proven?')
w('Tools: `tools/routine_audit.py` (finds the entry points, measures them, searches the port for them) and `tools/routine_audit_doc.py` (this file). The listing of original code stays in git-ignored `scratch/`; this file has addresses only.')
w('')
w('## 1. What was counted (V: by running the tools)')
n = len(rows)
w('* %d routine entry points of the 27,136-byte program: the start `$4027`, %d JSR targets, 48 ghost dispatch-table routines, and %d JMP targets (most are labels inside a longer routine).' % (n, sum('jsr' in r['kind'] for r in rows), sum('jmp' in r['kind'] for r in rows)))
w('* All of them lie in the 10,456 bytes of code found by recursive disassembly; 10,386 of those were executed on the oracle (99.3 %), see UNEXECUTED_CODE.md. The other 16,680 bytes are data (DATA_REGIONS.md), which the data pack carries.')
w('* %d entries are named (as an address) in the port\'s sources; %d are not. **Naming is only a hint, not proof**: a helper can be rebuilt inline without its address in a comment. Section 3 decides each unnamed entry by reading its code.' % (sum(bool(r['src']) for r in rows), sum(not r['src'] for r in rows)))
w('')
w('## 2. Groups: what the port does, how it was proven')
w('Status words: EXACT = the port\'s output equals the oracle\'s (bytes and/or cycles) in a named test; MODELLED = timing rebuilt from measurements, not run instruction by instruction; NOT PORTED = no code in the port.')
w('')
w('| Entries | Part of the game | Port module | Status | Evidence |')
w('|---|---|---|---|---|')
cnt = {}
for r in rows:
    g = group(int(r['addr'], 16))
    cnt.setdefault(g[2] if g else '?', []).append(r)
for g in G:
    w('| %d | %s | %s | %s | %s |' % (len(cnt.get(g[2], [])), g[2], g[3], g[4], g[5]))
if '?' in cnt:
    w('| %d | NOT GROUPED: %s | | | |' % (len(cnt['?']), ', '.join(r['addr'] for r in cnt['?'])))
w('')
w('## 3. Entries not named in the port sources, decided by reading the code (I = from reading, V = also covered by a test that would fail if it were wrong)')
w('')
w('| Address | Bytes | What it is | Where in the port | Status |')
w('|---|---|---|---|---|')
N = {
 '44E8': ('sets `$0C/$0D` to a start value for the attract screens', 'attract.cpp script builder', 'EXACT (V: script equal)'),
 '46CC': ('the chase animation loop of the attract turn (ghost and Pac-Man moving across, `JMP` loop)', 'attract.cpp (AttractTurn script)', 'EXACT (V: test_attract)'),
 '5273': ('draws a list of 3-byte records ($FD/$FE/$FF end marks) with `$5CF0`, polls the keyboard', 'attract.cpp / hires.cpp (screen draw + key poll in AttractFlow)', 'EXACT (V: test_hires, test_flow)'),
 '5299': ('step to the next record of that list', 'same', 'EXACT (V)'),
 '550E': ('loop that draws the HUD of each player from the last to the first', 'screen.cpp drawHudPlayers', 'EXACT (V: test_players first screens 1-5)'),
 '56FD': ('copies the sprite position and draws with `$6A20` (ghost/cherry sprite helper)', 'tickdraw.cpp', 'EXACT (V: test_tickdraw)'),
 '57BE': ('tests four fixed screen positions and draws an item at each that is set (I: the energizers)', 'tickdraw.cpp / screen.cpp energizer draw', 'EXACT (V: test_screen, test_dots)'),
 '5CE7': ('step an x/y position of the maze draw by one cell', 'hires.cpp maze draw', 'EXACT (V: test_hires)'),
 '5D20': ('maze draw loop test (end of line)', 'hires.cpp', 'EXACT (V)'),
 '5D6E': ('draws 72 records with `$5CF0` and then 70 with `$5D3D` from a table (I: the maze walls)', 'hires.cpp / screen.cpp', 'EXACT (V: test_hires)'),
 '6AEB': ('sets `$04`=16, `$10`=102, `$E7`=0 and calls `$6978` (a sprite draw; I: which sprite)', 'tickdraw.cpp', 'EXACT (V)'),
 '6AFB': ('a `JMP $6AEB`', 'same', 'EXACT'),
 '6CE6': ('busy delay (`$6CE8` loop), 13 bytes', 'cycleclock: cycles counted, not looped', 'MODELLED (exact cycle count)'),
 '6CF3': ('an `RTS`', 'nothing needed', 'n/a'),
 '6D27': ('limit the level to 21 (`$15`) for table look-ups', 'ghostmode.cpp / gamecore.cpp level clamp', 'EXACT (V: levels 22-40 run on the oracle, TASK-021)'),
 '6F72': ('keyboard poll: keep key unless carry', 'input.cpp', 'EXACT (V: test_input)'),
 '6FCE': ('`SEC / RTS` (no digital input)', 'input.cpp', 'EXACT (V: test_input)'),
 '701E': ('analogue paddle reader (counts `$C064/$C065` while high)', 'NONE (input.cpp takes a ready joystick direction)', 'NOT PORTED'),
 '7053': ('paddle X range check', 'NONE', 'NOT PORTED'),
 '706B': ('paddle Y range check', 'NONE', 'NOT PORTED'),
 '70D8': ('draws the HUD label and score of the current player', 'screen.cpp', 'EXACT (V: test_players)'),
 '780A': ('maps `$03` through the 4-entry table at `$7812` (I: direction mapping)', 'pacstep.cpp direction map', 'EXACT (V: test_pacstep)'),
 '7BAC': ('speaker busy-loop engine A: 4 loops with `BIT $C030`', 'sound.cpp (click times)', 'MODELLED pacing'),
 '7CF7': ('speaker busy-loop engine B: tone, same with extra delay', 'sound.cpp (click times)', 'MODELLED pacing'),
 '7D7B': ('delay with X = 30 (`JMP $6CE8`)', 'cycleclock', 'MODELLED (exact cycle count)'),
 '7E13': ('erase, redraw and update a ghost sprite during the freeze', 'tickdraw.cpp', 'EXACT (V: test_tickdraw)'),
 '804E': ('adds 4 to `$0C` and `$10` (sprite offset)', 'tickdraw.cpp', 'EXACT (V)'),
 '805D': ('other ghosts keep moving during the ghost-eaten freeze (eyes ghosts get two extra steps)', 'eating.cpp / ghosttick.cpp', 'EXACT; the eyes branch ($806F) shown on the oracle only by forcing it (TASK-021b)'),
 '8107': ('cartoon actor set-up (`$10,$1A,$04`)', 'cartoon.cpp', 'EXACT (V: call-for-call equal)'),
 '8112': ('cartoon helper: patch the draw operands, jump `$55A6`', 'cartoon.cpp', 'EXACT (V)'),
 '8180': ('cartoon helper: sets `$0C/$10/$11`, `JMP $5D3D`', 'cartoon.cpp', 'EXACT (V)'),
 '8189': ('cartoon 1 actor reset (positions, counters)', 'cartoon.cpp', 'EXACT (V)'),
 '81B7': ('inside cartoon 1 (`$81AE`), by address; not read line by line', 'cartoon.cpp', 'EXACT (V)'),
 '8221': ('inside cartoon 1 (`$81AE`), by address; not read line by line', 'cartoon.cpp', 'EXACT (V)'),
 '83D9': ('inside cartoon 2 (`$83D0`), by address; not read line by line', 'cartoon.cpp', 'EXACT (V)'),
 '848F': ('inside cartoon 3 (`$8486`), by address; not read line by line', 'cartoon.cpp', 'EXACT (V)'),
 '84F9': ('inside cartoon 3 (`$8486`), by address; not read line by line', 'cartoon.cpp', 'EXACT (V)'),
 '8874': ('the plain ghost move (two units, cell counters)', 'ghosttick.cpp ghostMove', 'EXACT (V: test_ghostmove)'),
 '8981': ('multiply by 3 (`A*2+A`)', 'ghostjunction.cpp / inline', 'EXACT (V)'),
 '8999': ('ghost-house exit script queue, "up" case', 'ghostjunction.cpp', 'EXACT (V; `$89AC` closed on the oracle)'),
 '8B13': ('screen address of a maze cell from `$7B/$7C`', 'gamecore.cpp cell addressing', 'EXACT (V: test_dots)'),
 '8B3F': ('distance test: absolute value below 10, bumps `$59` on later levels (I: the contact test)', 'gamecore.cpp contactTest', 'EXACT (V: test_lives, test_players)'),
 '8B7E': ('tail of the contact test (caught)', 'gamecore.cpp', 'EXACT (V)'),
 '8C2E': ('next player: `INC $50`, wrap at `$51`, `JSR $53CF`, back to start of life (read)', 'gamecore.cpp (033-6a)', 'EXACT (V: test_players rotation)'),
 '8CA6': ('the patched `JSR $0666` = ghost decision dispatch; writes the move into the queue', 'ghostdecide_gen.cpp + ghostjunction.cpp', 'EXACT (V: test_ghostdecide)'),
}
for r in rows:
    if not r['src']:
        a = r['addr']
        d = N.get(a)
        if d:
            w('| $%s | %d | %s | %s | %s |' % (a, r['bytes'], d[0], d[1], d[2]))
        else:
            w('| $%s | %d | NOT DECIDED | | |' % (a, r['bytes']))
w('')
w('## 4. Result')
w('* **NOT PORTED (V by this audit): the analogue paddle/joystick path `$701E`, `$7041`, `$7053`, `$706B` (about 150 bytes).** Everything else of the 10,456 code bytes has a counterpart in the port.')
w('* **MODELLED, not instruction-exact:** the cost of the demo and game ticks (a fitted model; typical error 111 cycles per tick, +1,042 cycles over the 39.2 M of a demo turn), the death animation frames (page-crossing residue 0-66 cycles per frame), the busy-wait loops of the sound engines and delays (counted, not run), the idle-tick cost of a real game (2,987, assumed).')
w('* **Not routines at all:** the disk loader and boot sectors (BOOT_TRACKS.md) are replaced by the data pack; the Apple II monitor ROM runs for real (monitor.cpp / cpu6502.cpp).')
w('* The 70 never-executed bytes (UNEXECUTED_CODE.md) are 16 runs; 6 are proven dead by the code itself, the rest are corner paths (page-cross carries in screen addresses, one Space-in-delay path, a one-digit rank answer).')
open('docs/ROUTINE_AUDIT.md', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print('written; undecided:', [r['addr'] for r in rows if not r['src'] and r['addr'] not in N])
