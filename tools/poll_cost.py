"""TASK-034-TIMING: the cycles of the key poll $6F5E (entry to RTS) in the attract game, with the keyboard (mode 1) or the paddles (mode 2) chosen on the mode screen (development tool; original data from scratch/).
Prints (cycles, $E8, $F5) with the number of polls.   Usage: python -I tools/poll_cost.py 1|2"""
import os, sys, collections
sys.path.insert(0, os.path.dirname(__file__))
from play import do_step
from apple2 import NeedKey
from cov_close import factory
mode=sys.argv[1]
mc=factory()
for st in ('key:'+mode,'key:cr'): do_step(mc,st,lambda *_:None)
cpu,m=mc.cpu,mc.m
step=cpu.step
S={'t0':None,'sp':0}
res=collections.Counter(); n=[0]
def hook():
    pc=cpu.pc
    if pc==0x6F5E:
        S['t0']=cpu.cycles; S['sp']=cpu.sp
    op=m[pc]
    r=step()
    if op==0x60 and S['t0'] is not None and cpu.sp==S['sp']+2:
        res[(cpu.cycles-S['t0'], m[0xE8], m[0xF5])]+=1; S['t0']=None; n[0]+=1
    return r
cpu.step=hook
while n[0]<3000 and cpu.cycles<400_000_000:
    try: mc.run(150_000)
    except NeedKey: mc.push_key(13)
for k,v in sorted(res.items()): print(k,v)
