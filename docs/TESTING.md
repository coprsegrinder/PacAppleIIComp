# Testing

Run everything: `for t in tests/test_*.py; do python -I $t; done` (stdlib `unittest`; no pytest installed).

## Test status (2026-10-29)
| Test file | Covers | Status |
|---|---|---|
| tests/test_dis6502.py | disassembler tables, branch targets, first bytes of the real program | pass (4) |
| tests/test_dsk.py | DOS 3.3 catalog / file extraction on a synthetic disk | pass (2) |
| tests/test_recdis.py | recursive-descent coverage, BIT-skip overlaps, ROM/indirect targets | pass (3) |
| tests/test_apple2.py | Soft switches (keyboard strobe, speaker log, video, buttons, paddle timer, unknown I/O), trapped ROM routines (COUT1, VTAB, RDKEY, GETLN, SETKBD, SETVID), scrolling/wrap; synthetic code only | pass (22; includes 3 coverage-tracking tests and an annunciator test) |
| tests/test_static_plus.py | table-pointer reading, extra entry points find otherwise unreachable code (synthetic) | pass (2) |
| tests/test_classify_gaps.py | gap extraction, absolute and #lo/#hi pointer detection (synthetic) | pass (2) |
| tests/test_trace_tables.py | patched-operand read tracker: base after patch, offsets read (synthetic) | pass (1) |
| tests/test_render_frames.py | hi-res byte decoding (bit order, palette, parity, adjacency), PNG writer, contact sheet layout, blit-row merging (synthetic) | pass (7) |
| tests/test_callsites.py | call-site logging (caller = JSR address, params, loop-back excluded), hi-res row base addresses, screen render size (synthetic) | pass (4) |
| tests/test_play.py | scripted-session steps: keys, run with underscores, key-wait message, text page, info, CPU error reporting (synthetic) | pass (4) |
| tests/test_cov_union.py | coverage merge/compare helpers, auto-player stop conditions and paddle driving (synthetic) | pass (3) |
| tests/test_explore.py | explorer helpers: key constants, send, until_key, text page 2 reader (synthetic) | pass (4) |
| tests/test_save_clicks.py | grouping clicks into constant-pitch runs (synthetic) | pass (4) |
| tests/test_compare_dump.py | run merging, regions tile $0000-$BFFF, scenario parsing | pass (3) |
| tests/test_boot_compare.py | boot-track diff, owned-sector walk (catalog, T/S lists), deleted entries, hidden-data scan on synthetic disks | pass (5) |
| tests/test_verify_image.py | verifier rule on synthetic disks: exact, program-only match, tampered program, wrong name/type/load, wrong size, damaged catalog | pass (6) |
| tests/test_make_manifest.py | manifest generator: gaps between code, rendering without data, content hash order | pass (4) |
| tests/test_ghost_tools.py | running a routine on the oracle, dependency/outputs analysis, dispatch check, event hook (synthetic code) | pass (7) |
| tests/test_junction_graph.py | graph parsing, symmetry/geometry/missing-link detection, tunnel wrap, routine-vs-exit check (synthetic) | pass (5) |
| tests/test_trace_modes.py | simulated JSR injection, watched-variable log with writer pc, routine entry counters, ghost record flags, zero-page reference finder (synthetic) | pass (5) |
| tests/test_movement_tools.py | eyes-table walk (steps, broken walks, loops), eyes route extraction, record flag setting, house-record change log (synthetic) | pass (5) |
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

## Gap classification (TASK-013e)
Real-data results (one-off scripts, not unit tests): tools/classify_gaps.py, tools/trace_readers.py; numbers in docs/DATA_REGIONS.md. The trace_readers run was repeated once after turning the scratch script into the committed tool; both gave the same top readers.
Limits: the classification only knows what the 243M-cycle scenario read; "data" means used as data, a gap may also contain unreached code; no AppleWin cross-check yet (TASK-013c-2).

## Table tracing (TASK-014)
Real-data runs of tools/trace_tables.py are one-off (not unit tests); results in docs/TABLES.md. Re-run after refactoring the tracker produced byte-identical output. Limits: scenario-bound (only tables used in the 243M-cycle run), sizes are read extents.

## Frame rendering (TASK-015)
Decoder tested on synthetic bytes. The real contact sheet was checked by eye by me only (not yet compared with an AppleWin screenshot); colours are an approximation. Images stay in scratch/ (git-ignored).

## Screen comparison (TASK-016)
Three AppleWin window-only captures (mode screen, maze being drawn, attract screen) were compared by eye with the headless render: same text, layout and maze geometry. No pixel comparison (AppleWin timing/NTSC colours differ). AppleWin was killed afterwards; original disk and scratch/work.dsk hashes unchanged.

## Input/auto-play (TASK-018)
Runs of tools/autoplay.py are deterministic (seeded) real-data scenarios, not unit tests; results in docs/CONTROLS.md. Gap: no AppleWin comparison of steering/joystick behavior (TASK-019).

## Rarely used paths (TASK-020)
Scenarios of tools/explore.py are real-data one-off runs (results in docs/CONTROLS.md and DYNAMIC_COVERAGE.md). A shell quoting accident while writing the explorer put raw carriage returns into the source; found by a syntax error before any run, fixed, no results affected.

## AppleWin memory comparison (TASK-019)
Result and method: docs/EMULATOR_COMPARISON.md (game area identical except 8 table-pointer operands at the players prompt). This is the first memory-level check of our machine against the emulator since TASK-013b.

## Boot tracks (TASK-006)
Real-data comparison: docs/BOOT_TRACKS.md. Limit: only one reference DOS build.

## Verifier (TASK-009)
Real-data checks (one-off): supplied image -> exact-image; System Master -> not-verified; boot-track byte / volume number changed in memory -> game-program; one program byte flipped -> not-verified. .po and other containers untested (unsupported).

## Gameplay dump (TASK-012)
Real-data comparison at an Esc-paused moment of play: docs/EMULATOR_COMPARISON.md. Four play lengths tried because AppleWin timing cannot be matched exactly; best case 7 of 27,136 game bytes differ.

## C++ port tests (TASK-025)
`port/tests`: SHA-256 (FIPS vectors, padding boundaries, 1M bytes, values from Python hashlib), DOS 3.3 reader bounds, the verification rule on synthetic disks (mirrors tests/test_verify_image.py), and two real-image tests (run with --image/--reference or through CTest when configured with -DPACMAN_DSK; otherwise skipped and reported as skipped). Negative check: pointing --image at the System Master makes the real-image test fail as it should. Cross-check: tools/verify_image.py and pacman_verify.exe compared on 8 inputs (real image, System Master, 3 tampered copies in scratch/, a 5-byte file, a zero-filled image, a missing file): same text and exit code except the missing-file OS wording.
Not covered: no fuzzing of the DOS reader beyond the damaged-catalog and out-of-range cases; no 32-bit build; only MSVC 14.51.

## Data pack (TASK-026)
C++ (port/tests/test_datapack.cpp, 10 tests; plus a real-image test): round trip, exact bytes, code/DOS not stored (size arithmetic), identical pack for other DOS tracks, refusal of unverified images and invalid manifests, **every single-byte change and every truncation refused**, wrong-program pack refused, inconsistent tables with valid hashes refused. Real image: content hash equals tools/make_manifest.py's independent value; `python -I tools/make_manifest.py --check` confirms the committed manifest matches a fresh analysis. Limit: integrity only; the manifest is only as good as the code map (docs/DATAPACK_SPEC.md).

## Ghost analysis (TASK-030a)
Real-data runs: trace_ghosts (2 games, seed 3: 183 decisions), ghost_routines (48 routines x 60 inputs), junction_graph (checks printed as counts). Corrections made while testing: my first dependency check compared whole register tuples (every routine looked input-dependent); fixed to compare A only. A junction-graph test expectation had wrong arithmetic (code was right); corrected and tightened. Limits: only 183 decisions, one seed; the grid is not a proof of equivalence (030b).

## Ghost modes (TASK-030c)
Real-data checks (one-off): trace_modes with the energizer routine called on the oracle at the top of the main loop (an earlier attempt called it in the middle of a ghost update and produced corrupted state: injected at a quiet moment instead; the printout also read the flags before the routine finished, fixed). Results in docs/GHOST_MODES.md. Limits: one level (1), one run; injected energizer, not a played one; eaten-ghost path not run.

## Movement, house and eyes (TASK-030d)
Real-data: trace_house (one run, 12M instructions), trace_eyes (ghost 1, one path), junction_graph (eyes table fully checked statically). The eyes experiment sets a flag between passes (not a played ghost-eating). Limits: single seed/level; Pac-Man movement is read, not traced.

## Full-suite checkpoint (run on request, not after every step)
`bash tools/full_suite.sh python` builds the C++ tests, runs them with EVERY optional input and then the Python tests (the script lists the exact inputs). `--ticks` takes the recordings valid for the logic tests (A-D, B10, N00, T00, L04-L21); `--drawticks` (new, 2026-11-02) takes the recordings for the drawing test (A-D plus the poked/custom scenarios F G H J K L M P Q R S); logic tests must not get the poked scenarios (their tick-exact checks do not model the pokes).
Result of the first checkpoint (2026-11-02, after TASK-029g14): C++ 102 tests, 0 failed, nothing skipped (drawing: 15 scenarios, all equal; restart/level-end/death/game-over sequences all EQUAL); Python 40 files, 172 tests, all OK (run 6-24 at a time; about 14 minutes when 40 run at once).
