# Task Ledger
Statuses: Planned / In progress / Implemented / Tested / Verified / Blocked / Deferred.

| ID | Task | Status | Depends | Test / acceptance |
|---|---|---|---|---|
| TASK-001 | Inspect environment, hash and catalog the disk | Verified | - | Hash recomputed after tooling; unchanged |
| TASK-002 | Add read-only `tools/inspect_dsk.py`, git repo, `.gitignore` excluding disk images | Verified | 001 | Script output matches manual parse |
| TASK-003 | Write a small 6502 disassembler in Python (local, no download) | Planned | 002 | Round-trips known opcodes; disassembles first 48 bytes as in notes |
| TASK-004 | Extract PAC-MAN body to scratch/ (git-ignored); map zero/data/code regions | Planned | 003 | Region map doc; extraction byte-matches image |
| TASK-005 | Disassemble boot relocation loop; determine real memory layout | Planned | 004 | Layout table in REVERSE_ENGINEERING.md |
| TASK-006 | Inspect DOS/boot tracks 0-2 for modifications or protection | Planned | 002 | Compare with standard DOS 3.3 boot sectors from memory/docs |
| TASK-007 | Web research (no downloads): Apple II refs, existing Pac-Man Apple II disassemblies | Planned | - | Entries in EXTERNAL_RESOURCES.md |
| TASK-008 | Reconcile release identity (1982 vs 1983 Atari) | Planned | 007 | Documented evidence |
| TASK-009 | Decide authoritative expected hash for verifier | Planned | 007,008 | Hash cross-checked against a public source |
| TASK-010 | Obtain reference emulator (needs approval) | Blocked | approval | Boots image, title screen captured |
