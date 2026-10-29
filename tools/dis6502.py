"""Minimal NMOS 6502 disassembler (documented opcodes only; others print as .byte).
Library: disassemble(data, origin) -> list of (addr, bytes, text).
CLI: python -I tools/dis6502.py <binfile> <origin-hex> [start-offset-hex] [count-bytes-hex]"""
import sys

# mode -> (operand size, format)
MODES = {
    'imp': (0, ''), 'acc': (0, 'A'), 'imm': (1, '#${:02X}'), 'zp': (1, '${:02X}'),
    'zpx': (1, '${:02X},X'), 'zpy': (1, '${:02X},Y'), 'abs': (2, '${:04X}'),
    'abx': (2, '${:04X},X'), 'aby': (2, '${:04X},Y'), 'ind': (2, '(${:04X})'),
    'izx': (1, '(${:02X},X)'), 'izy': (1, '(${:02X}),Y'), 'rel': (1, None),
}
# (mnemonic, {mode: opcode})
_T = {
 'ADC': dict(imm=0x69, zp=0x65, zpx=0x75, abs=0x6D, abx=0x7D, aby=0x79, izx=0x61, izy=0x71),
 'AND': dict(imm=0x29, zp=0x25, zpx=0x35, abs=0x2D, abx=0x3D, aby=0x39, izx=0x21, izy=0x31),
 'ASL': dict(acc=0x0A, zp=0x06, zpx=0x16, abs=0x0E, abx=0x1E),
 'BCC': dict(rel=0x90), 'BCS': dict(rel=0xB0), 'BEQ': dict(rel=0xF0), 'BMI': dict(rel=0x30),
 'BNE': dict(rel=0xD0), 'BPL': dict(rel=0x10), 'BVC': dict(rel=0x50), 'BVS': dict(rel=0x70),
 'BIT': dict(zp=0x24, abs=0x2C), 'BRK': dict(imp=0x00),
 'CLC': dict(imp=0x18), 'CLD': dict(imp=0xD8), 'CLI': dict(imp=0x58), 'CLV': dict(imp=0xB8),
 'CMP': dict(imm=0xC9, zp=0xC5, zpx=0xD5, abs=0xCD, abx=0xDD, aby=0xD9, izx=0xC1, izy=0xD1),
 'CPX': dict(imm=0xE0, zp=0xE4, abs=0xEC), 'CPY': dict(imm=0xC0, zp=0xC4, abs=0xCC),
 'DEC': dict(zp=0xC6, zpx=0xD6, abs=0xCE, abx=0xDE), 'DEX': dict(imp=0xCA), 'DEY': dict(imp=0x88),
 'EOR': dict(imm=0x49, zp=0x45, zpx=0x55, abs=0x4D, abx=0x5D, aby=0x59, izx=0x41, izy=0x51),
 'INC': dict(zp=0xE6, zpx=0xF6, abs=0xEE, abx=0xFE), 'INX': dict(imp=0xE8), 'INY': dict(imp=0xC8),
 'JMP': dict(abs=0x4C, ind=0x6C), 'JSR': dict(abs=0x20),
 'LDA': dict(imm=0xA9, zp=0xA5, zpx=0xB5, abs=0xAD, abx=0xBD, aby=0xB9, izx=0xA1, izy=0xB1),
 'LDX': dict(imm=0xA2, zp=0xA6, zpy=0xB6, abs=0xAE, aby=0xBE),
 'LDY': dict(imm=0xA0, zp=0xA4, zpx=0xB4, abs=0xAC, abx=0xBC),
 'LSR': dict(acc=0x4A, zp=0x46, zpx=0x56, abs=0x4E, abx=0x5E), 'NOP': dict(imp=0xEA),
 'ORA': dict(imm=0x09, zp=0x05, zpx=0x15, abs=0x0D, abx=0x1D, aby=0x19, izx=0x01, izy=0x11),
 'PHA': dict(imp=0x48), 'PHP': dict(imp=0x08), 'PLA': dict(imp=0x68), 'PLP': dict(imp=0x28),
 'ROL': dict(acc=0x2A, zp=0x26, zpx=0x36, abs=0x2E, abx=0x3E),
 'ROR': dict(acc=0x6A, zp=0x66, zpx=0x76, abs=0x6E, abx=0x7E),
 'RTI': dict(imp=0x40), 'RTS': dict(imp=0x60),
 'SBC': dict(imm=0xE9, zp=0xE5, zpx=0xF5, abs=0xED, abx=0xFD, aby=0xF9, izx=0xE1, izy=0xF1),
 'SEC': dict(imp=0x38), 'SED': dict(imp=0xF8), 'SEI': dict(imp=0x78),
 'STA': dict(zp=0x85, zpx=0x95, abs=0x8D, abx=0x9D, aby=0x99, izx=0x81, izy=0x91),
 'STX': dict(zp=0x86, zpy=0x96, abs=0x8E), 'STY': dict(zp=0x84, zpx=0x94, abs=0x8C),
 'TAX': dict(imp=0xAA), 'TAY': dict(imp=0xA8), 'TSX': dict(imp=0xBA),
 'TXA': dict(imp=0x8A), 'TXS': dict(imp=0x9A), 'TYA': dict(imp=0x98),
}
OPS = {op: (m, mode) for m, d in _T.items() for mode, op in d.items()}

def disassemble(data, origin=0, start=0, count=None):
    out, i = [], start
    end = len(data) if count is None else min(len(data), start + count)
    while i < end:
        op, addr = data[i], origin + i
        if op not in OPS or i + MODES[OPS[op][1]][0] >= len(data):
            out.append((addr, data[i:i+1], f".byte ${op:02X}")); i += 1; continue
        mn, mode = OPS[op]; size, fmt = MODES[mode]
        raw = data[i:i+1+size]
        if mode == 'rel':
            off = raw[1] - 256 if raw[1] > 127 else raw[1]
            text = f"{mn} ${(addr + 2 + off) & 0xFFFF:04X}"
        else:
            val = int.from_bytes(raw[1:], 'little')
            text = mn + ((' ' + fmt.format(val)) if fmt else '')
        out.append((addr, raw, text)); i += 1 + size
    return out

if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    org = int(sys.argv[2], 16)
    st = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0
    cn = int(sys.argv[4], 16) if len(sys.argv) > 4 else None
    for a, r, t in disassemble(data, org, st, cn):
        print(f"{a:04X}  {r.hex(' '):<9} {t}")
