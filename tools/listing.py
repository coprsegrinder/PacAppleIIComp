"""Print a disassembly of an address range of the relocated program (development tool; original bytes are only printed to the terminal).
Usage: python -I tools/listing.py <start-hex> <count-bytes> [body.bin]     (program origin $4000; at most ~100 lines are useful per call)
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from dis6502 import disassemble

ORIGIN = 0x4000


def main():
    start = int(sys.argv[1], 16)
    count = int(sys.argv[2])
    path = sys.argv[3] if len(sys.argv) > 3 else 'scratch/extract/PAC-MAN.1000.bin'
    body = open(path, 'rb').read()
    for addr, raw, text in disassemble(body, ORIGIN, start - ORIGIN, count):
        print('%04X  %-9s %s' % (addr, raw.hex(' '), text))


if __name__ == '__main__':
    main()
