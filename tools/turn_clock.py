"""Turn clock for the 20-minute rule (workflow tool, no project data).
  python -I tools/turn_clock.py start    remember the start of this turn (writes scratch/turn_start.txt)
  python -I tools/turn_clock.py check    print the elapsed minutes and a verdict: OK (< 12), WRAP UP (12-20: finish, save, commit, report, stop), STOP (>= 20)
  python -I tools/turn_clock.py plan 5 8 3   add up the expected minutes of the planned long steps and say whether they fit into one turn (limit 15 for the sum)"""
import os, sys, time

PATH = os.path.join(os.path.dirname(__file__), '..', 'scratch', 'turn_start.txt')


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    if argv[0] == 'start':
        os.makedirs(os.path.dirname(PATH), exist_ok=True)
        open(PATH, 'w').write(repr(time.time()))
        print('turn clock started')
    elif argv[0] == 'check':
        if not os.path.exists(PATH):
            raise SystemExit('no turn clock: run "start" first')
        m = (time.time() - float(open(PATH).read())) / 60
        print('elapsed %.1f minutes: %s' % (m, 'OK' if m < 12 else 'WRAP UP (finish, save, commit, report, stop)' if m < 20 else 'STOP NOW (limit reached)'))
    elif argv[0] == 'plan':
        total = sum(float(a) for a in argv[1:])
        print('planned %.0f minutes: %s' % (total, 'fits into one turn' if total <= 15 else 'TOO LONG: do only the first steps now and put the rest into a new ledger row'))
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
