# Building and testing (port/)

Checked on 2026-10-29 on Windows 11 with Visual Studio Build Tools 2026 (MSVC 14.51), C++20, `/W4 /WX`. **Nothing needs to be downloaded.**

## Prerequisites
* Visual Studio Build Tools 2026 with the C++ desktop workload (provides `cl`, `nmake`, MSBuild, and a bundled CMake 4.3.1 that is not on the PATH).
* Windows SDK 10.0.26100 (needed later for Direct3D 11, XAudio2, XInput; the verifier needs none of it).

## Route 1: plain `build.bat` (no CMake)
From PowerShell in `port\`:
```
& .\build.bat          # builds ..\out\bat\pacman_verify.exe and pacman_tests.exe
& .\build.bat test     # builds and runs the tests (real-image tests are skipped: see below)
```
(Use `& .\build.bat` from PowerShell; `cmd /c "build.bat"` did not find the file in our test.)

## Route 2: CMake with the Visual Studio generator
```
$cm = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
& "$cm\cmake.exe" -S port -B out\build -G "Visual Studio 18 2026" -A x64 -DPACMAN_DSK="<your game.dsk>" -DPACMAN_REFERENCE_DSK="<another DOS 3.3 disk>"
& "$cm\cmake.exe" --build out\build --config Release
& "$cm\ctest.exe" --test-dir out\build -C Release --output-on-failure
```
The two `-D` options are optional: they add the `real_image` test, which checks the verifier against the real images (which stay outside the repository). Without them only the `unit` test is registered.

## Running the tests by hand
`pacman_tests.exe [--image <game.dsk>] [--reference <other DOS 3.3 disk>]`. Tests that need a real image print `SKIPPED` when the path is not given. Exit code 0 = all passed.

## Python side (development tools)
`for t in tests/test_*.py; do python -I $t; done` (stdlib `unittest`, no pytest; Python 3.14). See `docs/TESTING.md`.

## Known build notes
* The output folder `out\` is git-ignored.
* Warnings are errors (`/WX`): a new warning stops the build by design.
