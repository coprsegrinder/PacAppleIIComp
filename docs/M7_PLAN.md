# M7 plan: sound (TASK-032, written 2026-11-03)

Goal: the sounds of the Apple II game in the native port: the same click times (the speaker is a plain toggle), the same game freezes while a sound plays, played through Windows audio. Same method as M4-M6: rebuild from the analysis (docs/SOUND.md), test against the oracle, then integrate.

## What exists
* Analysis (TASK-032a): the two speaker instructions (`$7BB5` engine A, `$7D06` engine B), the formulas for engines A, B, C, 12 lists, a closed-form click-time model equal to the original code on every click of all 12 lists, and equal in play for 103 of 103 dot/energizer/ghost-eaten sounds. Tools: tools/sound_model.py, trace_sound.py, save_clicks.py; tests/test_sound_tools.py.
* The data-pack manifest already has several sound ranges (`$7BE3`, `$7BED`, `$7C4C`, `$7C68`, `$7C8F`, `$7D3F`); whether ALL 11 lists + the pointer table are covered is the first thing 032-1 checks.
* No audio code in the port yet. Windows audio will use winmm `waveOut` (part of Windows, nothing to download).

## Sub-tasks (each one turn; split again if it overruns)
| Row | Content | Test |
|---|---|---|
| 032-1 | Data audit: are all 11 note lists and the 6-word death table in the pack (manifest)? Extend the manifest/pack if not; a C++ reader `SoundData` | pack test: the bytes equal the original's |
| 032-2 | Click-list generator in C++: engines A, B, C with the cycle formulas (9P+11, 9P+44 +1 page cross, 31P+11, 31P+30,624, 37) for all 12 sounds | click lists equal the oracle's (golden click files from tools/save_clicks / sound_model, local) for all 12 lists |
| 032-3 | Sound events in the core: which tick starts which sound (dot, energizer, ghost eaten, 6 death steps, fruit eaten, bonus life; the 4 cartoon sites and the intro jingle are separate) and the game freeze length per sound | per-tick event list equals the oracle's sound starts on scenarios A-D (+ fruit, death, bonus recordings) |
| 032-4 | Audio output: click list -> PCM square wave (cycle to sample mapping, no filtering at first), waveOut device, mute (Ctrl-S, F9) | render test: sample positions of the edges equal the cycle times; device smoke test (can only be heard by the user) |
| 032-5 | Integration in pacman_play: sounds queued from the events, the game freeze added to the cycle budget (extraCycles), cartoon and intro sounds | pacman_play runs; user listens (the order and lengths) |
| 032-6 | Open questions: the intro jingle trigger, the cartoon sound sites (do they fire?), key during the jingle; AppleWin audio comparison (TASK-022) only if the user wants | documented answers |

## Risks / unknowns (from SOUND.md section 5)
* Real-speaker shaping is not modelled: a plain square wave is the faithful reference.
* The jingle (5 s) blocks the game: whether to keep that blocking is a pacing decision (M8: "faithful by default, options separate").
* I cannot hear the result: 032-4/032-5 need you to listen.
