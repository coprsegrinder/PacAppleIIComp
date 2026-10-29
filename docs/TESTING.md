# Testing

Run everything: `for t in tests/test_*.py; do python -I $t; done` (stdlib `unittest`; no pytest installed).

## Test status (2026-10-29)
| Test file | Covers | Status |
|---|---|---|
| tests/test_dis6502.py | disassembler tables, branch targets, first bytes of the real program | pass (4) |
| tests/test_dsk.py | DOS 3.3 catalog / file extraction on a synthetic disk | pass (2) |
| tests/test_recdis.py | recursive-descent coverage, BIT-skip overlaps, ROM/indirect targets | pass (3) |
| tests/test_apple2.py | Soft switches (keyboard strobe, speaker log, video, buttons, paddle timer, unknown I/O), trapped ROM routines (COUT1, VTAB, RDKEY, GETLN, SETKBD, SETVID), scrolling/wrap; synthetic code only | pass (21; includes 3 coverage-tracking tests) |
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

## Apple II stubs (tools/apple2.py, TASK-013b)
* No ROM image is used. Trapped routines: $FC22 VTAB (recomputes BASL/BASH from CV $25 - confirmed by the game's own `INC $25 / JSR $FC22`; a first version wrongly used A, a test of the real run showed it),
  $FD0C RDKEY, $FD6F GETLN (X = length, line at $0200, A = $8D), $FDF0 COUT1, $FE89, $FE93. Behavior is from documented monitor behavior (Inferred), not a ROM dump.
  Not modelled: RDKEY cursor drawing (the only text-page difference from AppleWin is the cell where the real cursor sits), bell, lowercase handling, flags after VTAB, language card, interrupts.
* Bug found by a unit test and fixed in the code: $C050/$C051 graphics/text were inverted.
* **Real-code check (V, one-off, uses original data so not a unit test):** `python -I tools/run_headless.py` boots the extracted body from $1000, runs ~537k cycles, and parks in RDKEY at the mode screen.
  Compared with AppleWin's dump (scratch/main.bin) at that point: **$4000-$A9FF identical (0 differences, including the $623A-$65B9 table that was runtime-written)**;
  text page 2 identical except the cursor cell ($0B96); soft switches end as text+page 2 (as on the emulator screen). Zero page, stack, $200-$3FF, page 1 text and $AA00+ differ (DOS/monitor/cursor state we do not model; expected).
* Feeding '1' is accepted and the game proceeds to a second menu, "NORMAL OR CUSTOMIZED KEYBOARD (N/C)?" (parked in RDKEY, caller $494E).
* Timing assumption (Inferred, unverified): paddle timers count 11 cycles per unit.

## Coverage run (TASK-013c-1)
* Unit tests: exec/read/write/writer tracking, fetches not counted as reads, stack writes attributed to the JSR, self-modified operand visible. Real-data runs are one-off scripts (tools/trace_coverage.py, tools/cov_report.py), not unit tests.
* Caveat (V): writes made by the Python ROM stand-ins (e.g. COUT1 storing text) are not in the write coverage; only CPU-executed instructions are tracked.
* Caveat: the startup stub's relocation writes are deliberately discarded before the game starts. Result numbers and findings: docs/DYNAMIC_COVERAGE.md. Not yet cross-checked against AppleWin for this scenario (TASK-013c-2).
