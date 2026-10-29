# Pac-Man (Apple II, Atari 1983): documented decompilation and native Windows port

A non-commercial hobby project with two goals:
1. **Document and reconstruct** the 1983 Atari/Atarisoft Apple II version of Pac-Man (the program on a DOS 3.3 disk image the owner already has), tracking what is *recovered*, *reconstructed* and *rewritten*.
2. **Build a native Windows 11 program** that plays the same game without running an emulator and without reading the disk image while playing. The disk image is used only to verify it is the expected game and to build a local data pack on the player's own machine.

**Status (2026-10-29): the game is not playable yet.** What exists: the analysis tools and findings, a verified 6502 reference model (development only), the port plan, and the first C++ piece: a disk-image verifier with tests.

## What this repository does NOT contain
No disk image, no extracted program bytes, no original graphics or sound data, no ROM, no emulator. They are listed in `.gitignore` on purpose (`scratch/`, `third_party/`, `*.dsk`, `*.bin`). You must supply your own legitimate disk image. See `docs/LEGAL_AND_ATTRIBUTION.md`.

## Quick start (verifier only)
```
port\build.bat test                                  rem builds with cl and runs the tests
out\bat\pacman_verify.exe "path\to\your game.dsk"    rem tells you whether the image is the expected game
```
Details: `docs/BUILDING.md`, `docs/USER_GUIDE.md`.

## Layout
| Folder | Content |
|---|---|
| `port/` | The C++ port (CMake project and `build.bat`): currently `src/verify` (SHA-256, DOS 3.3 reader, verification rule), `src/app` (command-line verifier), `tests/` |
| `tools/`, `tests/` | Python development tools and their unit tests: disassembler, 6502 core and Apple II model (the "oracle"), coverage, rendering and comparison tools. Development only, never shipped |
| `docs/` | All notes. Start with `PORT_PLAN.md` and `ARCHITECTURE.md` (plan), `TASK_LEDGER.md` (what is done and verified), `REVERSE_ENGINEERING.md` (findings), `TESTING.md` (what each test proves and does not prove) |
| `scratch/`, `third_party/` | Local only (git-ignored): extracted data, screenshots, the AppleWin reference emulator |

## Principles
* Faithful original behavior by default; modern options (scaling, high refresh, gamepad, HDR evaluation) are separate and tested one by one.
* Everything in the final result must be based on the supplied `.dsk` file (user decision, 2026-10-29).
* Claims are marked Verified / Inferred / Unknown; a test is never weakened to make it pass.
