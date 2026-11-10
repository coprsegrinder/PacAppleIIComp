# Rebuilding everything from the disk image alone

The repository holds no part of the original game. Everything that is derived from it (the data pack the program plays from, and the recordings of the original program that the tests compare the port with) is made on your machine from **your own disk image**. This note says how, step by step, for a fresh clone with nothing but the disk image added. It was tried that way on 2026-11-10: a fresh clone plus the disk image built without errors, passed all 287 tests, and reproduced the first golden game (scenario A, 8,090 ticks, the stored golden hash) with steps 4 and 5. The other recorders were not all re-run from zero.

You need: Windows 10/11, the Visual Studio Build Tools (C++ workload), Python 3, and the disk image of the 1983 Atari release (a 140 K DOS-order `.dsk`) in the repository root, named `Pacman (1983)(Atari).dsk` (or pass its path to the commands).

## 1. Build the program, the tools and the tests
```
port\build.bat
```
The output is in `out\bat` (`pacman_play.exe`, `pacman_pack.exe`, `pacman_verify.exe`, `pacman_tests.exe`). No third-party library is needed and nothing is downloaded.

## 2. Run the tests that need only the image
```
out\bat\pacman_tests.exe --image "Pacman (1983)(Atari).dsk"
```
This checks the disk image, builds the data pack and runs every test that needs no recordings (287 tests, 0 failed on a clean clone; about 50 of them skip the parts that compare with recordings).

## 3. Play
```
out\bat\pacman_pack.exe build "Pacman (1983)(Atari).dsk" scratch\local.pmpk
out\bat\pacman_play.exe --pack scratch\local.pmpk --scale 3
```
or, for a release zip: `port\release.bat`, which writes `out\release\PacMan-AppleII.zip`.

## 4. Extract the original program for the reference model (the "oracle")
The Python tools in `tools/` run the original program in a model of the Apple II. They read it from `scratch/extract/`, which is git-ignored:
```
python -I tools/extract_binary.py "Pacman (1983)(Atari).dsk" PAC-MAN scratch/extract
```
It writes `scratch/extract/PAC-MAN.1000.bin` (27,136 bytes, SHA-256 `EFF5B2635EDF0E3B6A227BEE0A477EB697AAEBC29EBC57089167DF5F10EE182C`). The model also loads the Autostart Monitor ROM from `Apple II roms/`, which is in the repository.

## 5. Make the recordings
Each recorder documents itself in its first lines. For example, the first golden game (about three minutes; it refuses to write unless the replay reproduces the stored golden hash from `tests/golden/gameplay_traces.json`):
```
python -I tools/record_ticks.py A
```
The recordings that the full test run uses, and the exact options, are listed in `tools/full_suite.sh`; the recorders that make them are `record_ticks.py` (A-D), `record_special.py` (tunnel, demo, bonus), `record_levels.py`, `record_levelend.py`, `record_eatchain.py` (ghosts eaten in a row), `record_ghost_ram.py` (the ghost memory and the cycle files), `record_tick_calls.py` (the drawing calls and screens), and the others named in `docs/TESTING.md`. Making all of them takes hours, not minutes. Then:
```
bash tools/full_suite.sh
```
runs the whole C++ suite with every recording.

## What is not needed to play, and not in the repository
* The recordings (derived from the original program).
* The AppleWin emulator and a DOS 3.3 reference disk (`third_party/`), used only by some verification checks; the tests that need them skip when they are absent.
* Other Apple II ROM dumps used during research.
