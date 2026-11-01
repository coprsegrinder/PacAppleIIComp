# Changelog

## 2026-10-29
* **Data pack (TASK-026, milestone M1):** `pacman_pack build/info`, a 60-range generated manifest (16,680 data bytes), loader that refuses any damaged or wrong-game pack (27 C++ tests).
* TASK-034a: golden gameplay traces (4 scenarios, tick-keyed inputs, logical-state hash chain; docs/GOLDEN_TRACES.md).
* TASK-027e: hi-res colour model checked against an AppleWin screenshot (100% of 2,172 isolated pixels agree after fixing my sampling offset; the first 97.3% and its filter explanation were wrong).
* TASK-027d: wall routine and text printer modelled and verified against the oracle (including the forced $1D = 0 mode); drawing tables listed.
* TASK-027c: hi-res addressing and pixel model verified, colour model written, 22 golden screen hashes (docs/GRAPHICS.md).
* TASK-027b: all hi-res writers decoded, cartoon big-sprite routine found (docs/GRAPHICS.md).
* TASK-027a: inventory of screen writers and video switches (docs/GRAPHICS.md).
* TASK-021d: 100% of the static code map executed; the last 38 bytes under forced state.
* TASK-021c: Space in the attract delay loop and the one-digit rank prompt executed; the four carry fix-ups shown dead in practice; 38 unexecuted bytes left, none reachable.
* TASK-021b: five unexecuted runs proven dead, two more executed; corrected the sound labels (the long sound is the ghost-eaten sound).
* Unexecuted code (TASK-021): static-code coverage raised from 87.5% to 99.3%; the keyboard mode also reads a 4-switch digital joystick (`docs/UNEXECUTED_CODE.md`).
* Level tables and fruit (TASK-031 analysis): 15 level-indexed tables, fruit schedule, lifetimes, scores and sound (`docs/LEVEL_TABLES.md`, `tools/level_tables.py`, `tools/trace_fruit.py`).
* Sound analysis (TASK-032a): two tone engines, 12 note lists and a cycle-exact click-time model (`docs/SOUND.md`, `tools/sound_model.py`, `tools/trace_sound.py`).
* Ghost decision rules (TASK-030b): complete per-routine tables checked against the original code; the personalities depend on Pac-Man's direction (`docs/GHOST_RULES.md`, `tools/ghost_rules.py`).
* Pac-Man side analysis (TASK-031a): dot and energizer scoring, lives, bonus life, ghost-eating score, Pac-Man speed rule, turning (read), collision rules (`docs/PACMAN_RULES.md`, `tools/trace_pacman.py`).
* **Port skeleton (TASK-025, milestone M0):** `port/` with CMake and `build.bat`, a dependency-free test runner, SHA-256, a bounds-checked DOS 3.3 reader and the verification rule in C++ (16 tests; same answers as `tools/verify_image.py` on 7 of 8 comparison inputs, the 8th differs only in operating-system error wording). Project documents added (README, BUILDING, USER_GUIDE, CHANGELOG, KNOWN_ISSUES, LEGAL_AND_ATTRIBUTION).
* Port plan and architecture written; user decisions D1-A (local data pack) and D2-A (hand reconstruction checked against the oracle); constraint: the final result is based only on the supplied `.dsk`.
* Verifier rule confirmed by the user; release identity reconciled (Atari 1983); boot tracks found to be a modified DOS; gameplay memory compared with AppleWin.
* Earlier the same day: disassembler, 6502 core and Apple II model, coverage and rendering tools, control and table analysis (see `docs/TASK_LEDGER.md`).
* Correction: the first "read as data" statistics were inflated by the startup stub's own reads; recomputed (`docs/DATA_REGIONS.md`).
