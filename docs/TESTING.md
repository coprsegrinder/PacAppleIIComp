# Testing

Run everything: `for t in tests/test_*.py; do python -I $t; done` (stdlib `unittest`; no pytest installed).

## Test status (2026-10-29)
| Test file | Covers | Status |
|---|---|---|
| tests/test_dis6502.py | disassembler tables, branch targets, first bytes of the real program | pass (4) |
| tests/test_dsk.py | DOS 3.3 catalog / file extraction on a synthetic disk | pass (2) |
| tests/test_recdis.py | recursive-descent coverage, BIT-skip overlaps, ROM/indirect targets | pass (3) |
| tests/test_cpu6502.py | 6502 core: flags, stack, JSR/RTS, BRK/RTI, JMP-indirect bug, cycle counts, ADC/SBC (binary + decimal) | pass (18) |

## CPU core (tools/cpu6502.py) - what the tests prove and do not prove
* Proven: hand-computed cases; SBC == ADC of the complement for all 131,072 input combinations; decimal ADC/SBC match an independent
  arithmetic model for all valid-BCD inputs; cycle counts for common modes, branch and page-cross penalties; a 257-cycle copy loop.
* **Real-code check (V):** running the game's own startup relocation on the core (116,173 instructions, 464,455 cycles) produces a relocated image identical to the
  extracted file, and identical to AppleWin's memory dump on the same 26,240 bytes. (One-off check, not in the unit tests because it uses original data.)
* **Not proven:** no standard 6502 functional-test ROM has been run (it would need to be downloaded, which requires approval). Decimal-mode behavior for *invalid* BCD
  inputs and undocumented opcodes (which raise CPUError) are not verified. Interrupts (IRQ/NMI) are not implemented.
* Test-correction note: one early test checked the wrong stack addresses (my mistake); the core was right. Fixed the test, not the core.

## Emulator cross-check
AppleWin 1.32.1.0 memory dumps are the current ground truth for memory state (see REVERSE_ENGINEERING.md section 7).
