"""Run one development tool for many scenario names, a few processes at a time (each replay of the game start alone takes about 2 minutes).
Usage: python -I tools/run_many.py <workers> <tool.py> X1 X2 ...      logs: scratch/<tool>_<X>.log / .err
Start it detached (PowerShell Start-Process) so that it survives the end of a tool call."""
import os, subprocess, sys, time

workers, tool, names = int(sys.argv[1]), sys.argv[2], sys.argv[3:]
stem = os.path.splitext(os.path.basename(tool))[0]
running = []
while names or running:
    running = [p for p in running if p.poll() is None]
    while names and len(running) < workers:
        n = names.pop(0)
        running.append(subprocess.Popen([sys.executable, '-I', tool, n], stdout=open('scratch/%s_%s.log' % (stem, n), 'w'), stderr=open('scratch/%s_%s.err' % (stem, n), 'w')))
    time.sleep(2)
print('all done')
