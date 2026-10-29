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
| TASK-013a | Minimal 6502 core in Python (all documented opcodes, flags, BCD) with unit tests | Planned | 003 | Known-instruction tests pass; optionally a standard 6502 functional test if approved/obtained |
| TASK-013b | Apple II soft-switch stubs (keyboard, speaker, video switches, paddles, ROM entry points), run game from the startup memory image | Planned | 013a | Game reaches the mode screen and accepts '1' headless |
| TASK-013c | Execution/read/write coverage logging; cross-check against AppleWin (same inputs, same memory) | Planned | 013b | Coverage map replaces heuristic region map; traces agree with AppleWin on checkpoints |
