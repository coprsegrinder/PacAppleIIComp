# Task Ledger
Statuses: Planned / In progress / Implemented / Tested / Verified / Blocked / Deferred.

| ID | Task | Status | Depends | Test / acceptance |
|---|---|---|---|---|
| TASK-001 | Inspect environment, hash and catalog the disk | Verified | - | Hash recomputed after tooling; unchanged |
| TASK-002 | Add read-only `tools/inspect_dsk.py`, git repo, `.gitignore` excluding disk images | Verified | 001 | Script output matches manual parse |
| TASK-003 | Write a small 6502 disassembler in Python (local, no download) | Verified (4 unit tests pass; boot bytes match image) | 002 | Round-trips known opcodes; disassembles first 48 bytes as in notes |
| TASK-004 | Extract PAC-MAN body to scratch/ (git-ignored); map zero/data/code regions | Verified (extraction); heuristic map only | 003 | Region map doc; extraction byte-matches image |
| TASK-005a | Recursive-descent disassembly from entry $4027 (`tools/recdis.py`) | Verified (3 unit tests; 30.3% coverage) | 004 | docs/CODE_COVERAGE.md |
| TASK-005b | Confirm $4000 origin with an AppleWin memory dump after startup; investigate $0666 | Verified (origin); $0666 hypothesis only | 005a | 8212/8212 reached code bytes match; 96.7% of body matches |
| TASK-005c | Find the other 70%: look at unreached areas, address tables, RTS-dispatch, RAM code | Verified (analysis); 70% still unclassified | 005b | Findings in docs/CODE_COVERAGE.md: self-modifying operands, pointer-based data; dynamic tracing needed |
| TASK-006 | Inspect DOS/boot tracks 0-2 for modifications or protection | Planned | 002 | Compare with standard DOS 3.3 boot sectors from memory/docs |
| TASK-007 | Web research (no downloads): Apple II refs, existing Pac-Man Apple II disassemblies | Planned | - | Entries in EXTERNAL_RESOURCES.md |
| TASK-008 | Reconcile release identity (1982 vs 1983 Atari) | Planned | 007 | Documented evidence |
| TASK-009 | Decide authoritative expected hash for verifier | Planned | 007,008 | Hash cross-checked against a public source |
| TASK-010 | Obtain reference emulator | Verified (boot) | approval granted 2026-10-29 | AppleWin 1.32.1.0 hash-verified; image boots from a copy (hash re-checked) to the input-selection screen. Title/attract screens still to capture |
| TASK-011 | Maintain `docs/RESUME_PROMPT.md` after every completed task | Ongoing | - | File's Current state/Next task match the ledger |
| TASK-012 | Dump memory during gameplay (confirm $0666 table, find what writes $623A-$65B9) | Planned | 005b | Dump + explanation in notes |
| TASK-013a | Minimal 6502 core in Python (all documented opcodes, flags, BCD) with unit tests | Verified (18 tests + real-stub run); no external reference suite yet | 003 | See docs/TESTING.md |
| TASK-013b | Apple II soft-switch stubs (keyboard, speaker, video switches, paddles, ROM entry points), run game from the startup memory image | Verified (18 unit tests + real run: mode screen text and game memory match AppleWin; accepts '1', reaches 2nd menu) | 013a | tools/apple2.py, tests/test_apple2.py, tools/run_headless.py; see docs/TESTING.md |
| TASK-013c-1 | Execution/read/write coverage on the headless machine; compare with static disassembly | Verified (21 unit tests in test_apple2.py; scenario run for 243M cycles) | 013b | docs/DYNAMIC_COVERAGE.md: 6,724 bytes executed, 1,031 not found statically; JSR $0666 is a patched operand |
| TASK-013c-2 | Cross-check coverage scenario against AppleWin | Partly done in TASK-016: screens and text agree visually (maze, attract screen); no AppleWin memory-dump comparison for the later scenario yet. Gameplay input moved to TASK-017 | 013c-1 | Traces/dumps agree at checkpoints |
| TASK-013d | Feed dynamically found regions into the static disassembler as extra entry points; re-measure | Verified (2 unit tests; coverage 30.2% -> 38.5%) | 013c-1 | tools/static_plus.py; docs/CODE_COVERAGE.md |
| TASK-013e | Classify the remaining 61.5%: gap classification using runtime reads and pointers; who reads the data | Verified (2 unit tests; scenario run) - classification is heuristic | 013d | docs/DATA_REGIONS.md: 38.5% code / 58.8% read-as-data / 2.6% pointer-only; tools/classify_gaps.py, tools/trace_readers.py |
| TASK-014 | Understand the table readers: base values and extents for the patched-operand readers | Verified (1 unit test; scenario run, re-run identical) - roles Inferred | 013e | docs/TABLES.md; tools/trace_tables.py |
| TASK-015 | Render shape frames to images in scratch/ to learn pixel layout | Verified for decoding (7 unit tests); frame identities Inferred from my own viewing of the sheet | 014 | docs/SPRITES.md; tools/render_frames.py |
| TASK-016 | Call-site table for the draw routines; render whole screen; compare with AppleWin window screenshots | Verified (4 unit tests; 3 AppleWin captures; visual comparison only, not pixel/byte) | 015 | docs/SPRITES.md (Call sites and screens); tools/trace_callsites.py, tools/render_screen.py |
| TASK-017 | Start play in the headless machine; record coverage, screens, speaker clicks | Verified, headless only (4 unit tests for tools/play.py); no AppleWin gameplay comparison | 016 | docs/DYNAMIC_COVERAGE.md (Gameplay run): 7,451 bytes executed, all inside the static code map; sound clicks present |
| TASK-018 | Find keyboard controls and the joystick path; drive Pac-Man; broaden coverage | Verified for keys A/Z/arrows/Ctrl-S/space and for both modes running under random input (unit tests added); up/down mapping Inferred; levels/fruit/customised keyboard not covered | 017 | docs/CONTROLS.md; tools/autoplay.py, tools/cov_union.py; union coverage 8,049 bytes, 0 contradictions |
| TASK-020 | Reach unexecuted static code: customised keyboard, Esc, levels/intermissions, fruit | Verified (4 unit tests for the explorer helpers; real-data scenarios); union coverage 9,147 of 10,456 static code bytes (87.5%), 0 contradictions; fruit not specifically shown | 018 | docs/CONTROLS.md, docs/DYNAMIC_COVERAGE.md; tools/explore.py |
| TASK-021 | Close the last 1,309 unexecuted static-code bytes (125 runs) by targeted situations (ghost-specific dispatch routines, bonus events) or by static reading, and mark any that are dead code | Planned | 020 | Coverage of static code >95% or a list of proven-dead code |
| TASK-019 | AppleWin gameplay comparison: same inputs, window screenshots and memory dumps at checkpoints; record speaker click timings for later audio work | Planned | 017 | Screens/memory agree at checkpoints |
