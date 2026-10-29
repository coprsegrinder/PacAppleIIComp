"""Boot the extracted PAC-MAN body headless on tools/apple2.py (uses original data from scratch/; development only).
Usage: python -I tools/run_headless.py [scratch/extract/PAC-MAN.1000.bin] [keys-to-type]
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from apple2 import Machine, NeedKey


def boot(body):
    mc = Machine()
    mc.init_text_screen()
    mc.load(0x1000, body)
    mc.cpu.pc = 0x1000
    return mc


def run_until_key(mc, limit=5_000_000):
    try:
        mc.run(limit)
        return 'limit'
    except NeedKey:
        return 'needkey'


if __name__ == '__main__':
    body = open(sys.argv[1] if len(sys.argv) > 1 else 'scratch/extract/PAC-MAN.1000.bin', 'rb').read()
    mc = boot(body)
    print('stop:', run_until_key(mc), 'pc=%04X' % mc.cpu.pc, 'cycles', mc.cpu.cycles)
    print('\n'.join(mc.text_lines()))
    print('rom calls', mc.rom_calls, 'unknown io', mc.unknown_io)
