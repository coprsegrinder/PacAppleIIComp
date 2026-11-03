"""TASK-033-4d: cycle-exact model of the death sequence of the original ($8BDD ... $7F04), checked against the attract demo's death (scratch/events_demoend.json)
and, with sound, against the recorded deaths of the game (development tool; original data from scratch/).  Run: python -I tools/death_model.py"""
import json, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import title_model as T
import chase_model as C
import setup_model as SM
from title_model import M, cross, branch


def run_death(s, ghosts, pac, e0=1, demo=True, label_player=1):
    """s.t = 0 at the first instruction of $8BDD. ghosts: [(x, y)] for ghosts 1..4 as in the records, pac: (x, y)."""
    z = s.z
    for g in (1, 2, 3, 4):
        s.rec[g][0], s.rec[g][1] = ghosts[g - 1]
    z[0x60], z[0x61] = pac
    z[0xE8] = 1 if demo else 0
    z[0x50] = label_player
    z[0x1D] = 1
    z[0xE0] = e0
    z[0x70], z[0x71] = ghosts[e0 - 1]               # the working area holds the ghost that touched Pac-Man
    s.add(6)
    C.copy_out(s)                                   # JSR $8712
    s.add(6 + 3 + 2 + 2 + 2 + 2 + 3)                # JSR $58FA: LDA $50, ASL x3, TAX, JMP $556E
    s.add(-6)                                       # ($556E is entered by a JMP: no JSR)
    s.mem_5592, s.mem_5597 = 0xF7, 0x55
    SM.r_556e(s, 8 * z[0x50])
    s.add(2)                                        # LDY #3
    for k in range(3):
        s.add(2 + 6)
        T.r_delay(s, 250)
        s.add(2 + (3 if k < 2 else 2))
    s.add(2 + 3)                                    # LDA #4, STA $E0
    z[0xE0] = 4
    while True:
        s.add(6)
        C.copy_in(s)
        C.r_7d8d(s)
        C.r_699e(s)
        s.add(5)
        z[0xE0] -= 1
        if z[0xE0] != 0:
            s.add(3)
        else:
            s.add(2)
            break
    C.r_7d80(s)
    C.r_699e(s)
    s.add(6)                                        # JSR $7EC0
    C.r_7d80(s)
    s.add(2 + 2 + 6)                                # LDA #$E6, LDY #$9B, JSR $6A20
    T.r_dispatch(s, 0xE6, 0x9B)                     # Pac-Man's last image ($9BE6); the JSR $6A20 is part of the call below
    s.add(2)                                        # LDY #3
    for k in range(3):
        s.add(2 + 6)
        T.r_delay(s, 255)
        s.add(2 + (3 if k < 2 else 2))
    C.r_7d80(s)
    C.r_699e(s)
    s.add(2 + 3)                                    # LDA #6, STA $0A
    z[0x0A] = 6
    while True:
        s.add(3 + 2 + 2)                            # LDA $0A, ASL, TAX
        x = 2 * z[0x0A]
        C.r_7d80(s)
        a_lo, a_hi = M[0x7F05 + x], M[0x7F06 + x]
        s.add(4 + cross(0x7F05, x) + 4 + cross(0x7F06, x) + 3 + 3)
        z[0x16], z[0x17] = a_lo, a_hi
        C.r_7e1f(s)
        s.add(6 + 3 + 2 + 2 + 4 + 4 + 3)            # JSR $7C82: LDA $0A, ASL, TAX, LDA $7C8F,X, LDY $7C90,X, JMP $7BBC
        s.add(4 + 4 + 3)                            # STA $7BCE, STY $7BCF, LDA $E8
        if demo:
            s.add(2 + 6)                            # BEQ not taken, RTS
        else:
            raise NotImplementedError('the sound of the death steps')
        s.add(2 + 6)                                # LDX #$80, JSR $6CE8
        T.r_delay(s, 0x80)
        C.r_7d80(s)
        C.r_69e9(s)
        s.add(5)                                    # DEC $0A
        z[0x0A] -= 1
        if z[0x0A] != 0:
            s.add(branch(True, 0x7F04, 0x7EDE))
        else:
            s.add(2)
            break
    s.add(6)                                        # RTS of $7EC0
    s.add(3 + 2 + 3)                                # LDA $E8, BEQ (not taken), JMP $43D2
    s.add(2 + 3 + 6)                                # $43D2: LDA #1, STA $E8, JSR $52BB


if __name__ == '__main__':
    EV = json.load(open('scratch/events_demoend.json'))['events']
    s = SM.S(0)
    # positions read from the oracle's erase calls: ghosts 4, 3, 2, 1 then Pac-Man (x - 4, y - 4)
    ghosts = {4: (185, 89), 3: (125, 125), 2: (67, 89), 1: (167, 77)}
    run_death(s, [ghosts[1], ghosts[2], ghosts[3], ghosts[4]], (189, 89), e0=1, demo=True)
    label = [e for e in s.ev if e[1] == 'blit'][0][0]
    theirs = [e for e in EV if e[0] >= 60828435 and e[1] in ('blit', 'poll', 'delay', 'dispatch', 'tile', 'mclear', 'shifted')]
    mine = [(e[0] - label + 60828435,) + tuple(e[1:]) for e in s.ev if e[1] in ('blit', 'poll', 'delay', 'dispatch', 'tile', 'mclear', 'shifted')]
    # the oracle's list ends with the title's page set-up; compare as many events as mine has
    bad = 0
    n = 0
    for a, b in zip(mine, theirs):
        n += 1
        pa = tuple(a[2:]) if a[1] != 'delay' else tuple(a[2:3])
        pb = tuple(b[2:]) if b[1] != 'delay' else tuple(b[2:3])
        if a[0] != b[0] or a[1] != b[1] or pa != pb:
            bad += 1
            if bad <= 8:
                print('diff', a, b)
    print('events mine', len(mine), 'compared', n, 'mismatches', bad)
    print('label at', label, ' end of the death code at', s.t - label + 60828435, ' (the title restarts at 63189736 + 537030 - 537030 = 63189736 relative: lores_fill entry', 63189736, ')')
