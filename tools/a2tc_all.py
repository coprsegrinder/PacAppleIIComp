"""Run tools/a2tc_scenarios.py for many scenarios, a few at a time (TASK-029h2b). Usage: python -I tools/a2tc_all.py <workers> X1 X2 ..."""
import subprocess, sys, time
workers, names = int(sys.argv[1]), sys.argv[2:]
running = []
while names or running:
    running = [p for p in running if p.poll() is None]
    while names and len(running) < workers:
        n = names.pop(0)
        running.append(subprocess.Popen([sys.executable, '-I', 'tools/a2tc_scenarios.py', n], stdout=open('scratch/a2tc_run_%s.log' % n, 'w'), stderr=open('scratch/a2tc_run_%s.err' % n, 'w')))
    time.sleep(2)
print('all done')
