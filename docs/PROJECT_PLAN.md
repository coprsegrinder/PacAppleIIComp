# Project Plan (initial, 2026-10-29)

## Local toolchain (V)
Windows 11 Pro, PowerShell 5.1 + Git Bash, Git 2.x, Python 3.14, Visual Studio 18 Build Tools (MSVC 14.51),
WSL present (not inspected), NVIDIA RTX 5070. **Not found:** CMake, Ninja, gcc/clang, any 6502 assembler, any Apple II emulator, 7-Zip.
Python's stdlib + MSVC are enough to start; CMake/MSBuild availability inside the Build Tools is unverified.

## Phases
0. **Facts and tooling** (current): identify image, local inspector, local disassembler.
1. **Static analysis**: layout, entry points, data tables, hardware usage; annotate disassembly (`re/` directory, no disk bytes committed beyond what policy allows - see legal doc).
2. **Reference baseline**: emulator-based observations (title, maze, ghosts, scoring, timing), recorded scenarios.
3. **Tracing harness**: a tiny 6502 core in test tooling to run original code deterministically and dump state for comparison (development only, never in the shipped game).
4. **Reconstruction**: readable C++ modules per subsystem, each checked against traces.
5. **Native Windows app**: window, renderer, audio, input, timing, faithful mode default.
6. **Verifier + runtime-isolation test.**
7. **Modernization roadmap** (scaling, refresh rate, HDR evaluation, gamepad...) behind options.
8. **Docs, packaging, clean-machine test, GitHub release** (only with your approval).

## Ranked risks
1. Unknown release identity and "expected hash" authority (affects verifier and claims).
2. Hidden copy protection or custom loader (could stop work; ask you first).
3. Lack of a reference emulator for ground truth (needs a download).
4. Reconstruction scale: ~27 KB of 6502 code/data with no symbols.
5. Timing fidelity (Apple II has no vblank interrupt; game paced by instruction loops).
6. Audio fidelity (speaker toggling is timing-bound).
7. Redistribution: reconstructed data tables (maze, shapes) are derived from the original; see legal doc later.
8. Windows toolchain (CMake absent; MSVC likely fine).

## Downloads needing approval (none requested yet)
* **Reference emulator**: AppleWin (Windows, GPL, has a debugger) is my recommendation; alternative MAME. Local alternative: write my own 6502 core, but it can't confirm real hardware behavior (video, speaker, language-card quirks) by itself.
* **CMake** (and optionally Ninja) if the Build Tools lack it - alternative: plain MSBuild/`cl` scripts.
No other downloads are planned.

## Update 2026-10-29 (TASK-024)
The CMake item above is obsolete: Visual Studio Build Tools 2026 bundles CMake 4.3.1 (not on PATH) and the Windows SDK 10.0.26100 has Direct3D 11, XAudio2, WASAPI, XInput and DXGI 1.6. No download is needed to build. Port plan: PORT_PLAN.md; architecture: ARCHITECTURE.md.
