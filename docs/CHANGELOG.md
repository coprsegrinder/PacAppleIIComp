# Changelog

## 2026-10-29
* **Data pack (TASK-026, milestone M1):** `pacman_pack build/info`, a 60-range generated manifest (16,680 data bytes), loader that refuses any damaged or wrong-game pack (27 C++ tests).
* Sound analysis (TASK-032a): two tone engines, 12 note lists and a cycle-exact click-time model (`docs/SOUND.md`, `tools/sound_model.py`, `tools/trace_sound.py`).
* Ghost decision rules (TASK-030b): complete per-routine tables checked against the original code; the personalities depend on Pac-Man's direction (`docs/GHOST_RULES.md`, `tools/ghost_rules.py`).
* Pac-Man side analysis (TASK-031a): dot and energizer scoring, lives, bonus life, ghost-eating score, Pac-Man speed rule, turning (read), collision rules (`docs/PACMAN_RULES.md`, `tools/trace_pacman.py`).
* **Port skeleton (TASK-025, milestone M0):** `port/` with CMake and `build.bat`, a dependency-free test runner, SHA-256, a bounds-checked DOS 3.3 reader and the verification rule in C++ (16 tests; same answers as `tools/verify_image.py` on 7 of 8 comparison inputs, the 8th differs only in operating-system error wording). Project documents added (README, BUILDING, USER_GUIDE, CHANGELOG, KNOWN_ISSUES, LEGAL_AND_ATTRIBUTION).
* Port plan and architecture written; user decisions D1-A (local data pack) and D2-A (hand reconstruction checked against the oracle); constraint: the final result is based only on the supplied `.dsk`.
* Verifier rule confirmed by the user; release identity reconciled (Atari 1983); boot tracks found to be a modified DOS; gameplay memory compared with AppleWin.
* Earlier the same day: disassembler, 6502 core and Apple II model, coverage and rendering tools, control and table analysis (see `docs/TASK_LEDGER.md`).
* Correction: the first "read as data" statistics were inflated by the startup stub's own reads; recomputed (`docs/DATA_REGIONS.md`).
