# The flow of the original between the boot menus, the attract loop and a game (TASK-033-4a)

Measured on the oracle (the headless Apple II model running the original code), 2026-11-03. Tools: `tools/trace_flow.py` (every call from the menu/attract/game-start code with its cycle), `tools/trace_markers.py` (iteration boundaries, demo ticks), `tools/trace_window.py` (drawing events in a cycle window), `tools/death_timing.py gameover` (the GAME OVER path). Everything below with cycle numbers is **V** (measured); readings of what a screen shows are **I** unless a page was compared (none of the attract screens has been compared yet).

## 1. The program's top level
| Where | What | Evidence |
|---|---|---|
| `$4027` | `STA $51`, `JSR $54D4` (new-game state), clears `$00-$02`/`$58`, `$1D=1`, **`$E8=1` (demo/attract flag)**, sets the reset vectors, `JSR $4821` (**the two boot menus**), `JMP $43D2` | disassembly |
| `$4821` | the boot menus on the Apple II **text screen (ROM font, GETLN/RDKEY)**: "PACMAN CAN BE PLAYED WITH / 1. KEYBOARD / 2. APPLE JOYSTICK / CHOOSE A MODE (1 - 2)", then (after `1`) "NORMAL OR CUSTOMIZED KEYBOARD (N/C)?" | text pages read at the key waits, cycles 525,410 and 536,997 |
| `$43D2` | **the attract loop** (`$E8=1`), see section 2 | trace |
| key `Space` during the attract | the key poll `$6F5E` (called after every glyph of the title text and in the chase loops) jumps to `$4144` when `$E8<>0` and the key is Space | disassembly `$6FA2-$6FAE` |
| `$4144`-`$4217` | "RANK? / NUMBER?" style prompts for the **hidden menu** (Ctrl-V Ctrl-C) and the **players prompt** ("HOW MANY PLAYERS? (1 - 5)"), then `$4217`: "PRESS <RETURN> TO PLAY / OR <SPACE> TO GO BACK TO DEMO." (RDKEY loop, Return = start, Space = back to the demo, `$FD0C`) | `docs/DYNAMIC_COVERAGE.md`, disassembly |
| Return | `$52FC` starts a game: `$E8=0`, screen build, **intro jingle once (`$568A`, `$F4`)**, 3.05 s READY wait, ticks | 033-2, `docs/SOUND.md` |

All the text-screen prompts use the **Apple II ROM character generator**. The port has no copy of it (and must not ship one): see the open decision in section 4.

## 2. One turn of the attract loop (first turn counted from `JSR $52BB` at cycle 537,024 = T0; the loop repeats every **63,189,736 cycles = 61.8 s** with identical timing: 8 turns seen in 452M cycles)
| T0 + cycles | What happens | Code |
|---|---|---|
| 0 | `$52BB`: graphics + lo-res page 2, fills `$0800-$0BFF` with `$DD`; page clear `$6F46` at +6,425; `$5260` at +97,010; **hires on `$6F2F` at +1,966,866**; eleven delay calls `$6CE8` pace an effect until +4,755,687 | `$52BB` |
| +4,755,687 | hires page clear; the title screen "Character / Nickname" is typed: first text routine `$42D0` at +4,846,293 (a glyph row routine `$4316` + OR blit `$55A6` + key poll `$6F5E` per character); the first character is blitted about 0.83 M cycles after the page clear | `$43D9...` |
| +6,323,904 / +7,146,486 / +8,215,368 / +9,120,110 | the four ghosts are shown one after the other (`$44E8` draws the ghost, `$88A2` its animation, two more text lines each with `$42D0`) | `$43F3-$4450` |
| +9,942,779 | the dot and the energizer line (`$6172`, `$7159`, `$61B6`, `$44F1`), the last text lines (`$44D8-$44E2`) until about +11,182,910 | `$445B-$44E2` |
| +12,250,257 | the chase scene starts: loop A (`$46B5-$4711`): ghosts and Pac-Man moving, `$86FC/$8712` steps, key poll `$6F5E` every 32nd pass | `$46xx`, until +15,721,774 |
| +15,722,152 | loop B (`$475x-$47C0`), with the **ghost-eaten animation** (`$467C` -> `$7DB8`) four times at +16,917,745, +18,709,391, +20,478,157, +22,219,337 | `$47xx` |
| +22,486,416 | the **demo game**: game setup `$531E`, screen `$5659` (page clear at +22,519,321), start of life `$53E6`; first tick at +23,955,118 | `$5303-$530C` |
| +23,955,118 .. +60,824,039 | **exactly 7,894 ticks** of the game loop with `$E8=1` (the recorded demo keys play, nothing else) | `$530F-$5318` |
| +61,592,028 | the demo ends with Pac-Man being caught: the ordinary death animation without sound (the steps are 135,015 cycles apart because the 137,052 sound gaps are not there), then `$52BB` of the next turn | trace_window |
| +63,189,736 | next turn | |

## 3. GAME OVER of a real game
After the death animation of the last life (033-3b) and the words GAME OVER (last letter drawn at m6 + 20,637, see `docs/M8_PLAN.md`) the original waits **4,868,255 cycles** (4.76 s) and then runs `$52BB` = **T0 of an attract turn**; the next title text is blitted 10,453,613 cycles after the last letter (the 033-3b figure). The demo of that turn starts 27,387,576 cycles after the last letter. (Measured on one `gameover` run with `$52BB` hooked, seed 91; the five earlier runs agree on the 10,453,613.)

## 4. What the port needs, in order (sub-tasks of TASK-033-4; each verified against the oracle before the next)
| Task | Content | Verification |
|---|---|---|
| 033-4b | the title screen (T0 effect, the "Character / Nickname" typing, the four ghosts, dot/energizer line) | page hashes at the checkpoints of the oracle, the typed text from the pack's strings (`$434F-$43D1`, glyphs `$4553`) |
| 033-4c | the chase loops A and B and the ghost-eaten animations | draw-call log equality, timing from the oracle |
| 033-4d | the demo game: start screen, 7,894 ticks of `GameCore` in demo mode with the recorded keys, the death at the end | state equal to the oracle's demo recording (exists: `ticks_DEMO.bin`), page hashes |
| 033-4e | the loop (63,189,736 cycles), Space -> players prompt -> "PRESS RETURN" -> the game with jingle and READY wait; GAME OVER -> 4.87 s -> attract | flow test against the oracle's marker list |
| 033-4f | the two boot menus | the typed keys and the screens |
| 033-4g | wire everything into `pacman_play`: boot menus, attract, prompts, game, GAME OVER, attract | live run, marker times equal to this table |

**Open decision for the user (blocks 033-4e/4f only):** the boot menus, the players prompt and the "PRESS RETURN" prompt are Apple II **text-mode** screens drawn with the ROM font. The port cannot ship that font. Options: (a) draw these three prompts with the game's own hires letters (`$4553` glyph set, as the attract title does) - looks different from the original text screens, texts and key handling identical; (b) a font of my own making in the style of the Apple II (not the original glyphs); (c) the user supplies a legal copy of the Apple II character ROM at run time (never shipped or committed), and the port reads it like it reads the disk image. I recommend (c) with (a) as the fallback when the file is absent.
