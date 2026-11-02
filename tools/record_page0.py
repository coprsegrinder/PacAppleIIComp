"""TASK-029g: save the oracle's hi-res page at the first two ticks of a game (development tool; original data from scratch/).
Writes scratch/page_tick0.bin and scratch/page_tick1.bin (8,192 bytes each; LOCAL ONLY, git-ignored). Usage: python -I tools/record_page0.py (about 1 minute)"""
import sys, os
sys.path.insert(0,'tools')
from apple2 import NeedKey
from autoplay import start_game
from play import do_step
from cov_close import factory
mc=factory()
for st in ('key:1','key:cr','70_000_000'): do_step(mc,st,lambda *_:None)
import os as _os
if _os.environ.get('PLAYERS') == '2':
    for st in ('key:sp', '3_000_000', 'key:2', '3_000_000', 'key:cr', '1_000_000'):
        do_step(mc, st, lambda *_: None)
else:
    start_game(mc)
pages=[]
step=mc.cpu.step
def hook():
    if mc.cpu.pc==0x7861 and mc.m[0xE8]==0 and len(pages)<2:
        pages.append(bytes(mc.m[0x2000:0x4000]))
        if len(pages)==2: raise SystemExit
    return step()
mc.cpu.step=hook
try:
    for _ in range(60):
        try: mc.run(150_000)
        except NeedKey: mc.push_key(13)
except SystemExit: pass
open('scratch/page_tick0%s.bin' % ('_2p' if _os.environ.get('PLAYERS') == '2' else ''),'wb').write(pages[0]); open('scratch/page_tick1%s.bin' % ('_2p' if _os.environ.get('PLAYERS') == '2' else ''),'wb').write(pages[1])
print('saved', len(pages))
