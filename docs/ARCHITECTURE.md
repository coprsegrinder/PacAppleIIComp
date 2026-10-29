# Architecture of the native Windows port (proposal; no code written yet)

Status: **proposal for the user's approval** (TASK-024). Facts below are marked V (verified in this project), I (inferred) or U (unknown). The plan, options and milestones are in PORT_PLAN.md.

## What the original program is (V unless marked)
* One 27,136-byte program at $4000-$A9FF (after its startup copy): **10,456 bytes of code (5,165 instructions, 129 routines called by JSR, 48 more reached through a jump table)**, the rest is data (docs/CODE_COVERAGE.md, DATA_REGIONS.md).
* It draws **directly into the Apple II hi-res screen memory** (280x192, 8 KB page at $2000) with two blitters that OR 1-, 3-, 4-, 5- and 7-byte-wide shapes in; text uses a 7x9 glyph font, menus use text page 2 (TABLES.md, SPRITES.md).
* The playfield is a **32x28 byte map** that is rebuilt at each game start and cleared one byte per dot eaten (I; TASK-012).
* Input is polled directly (keys A/Z/arrows or a joystick/paddle routine), sound is **speaker clicks timed by instruction counts**, and there is **no vertical-blank pacing**: game speed comes from how many cycles each pass of the main loop takes (below).
* It uses no operating system service after startup; the only monitor-ROM routines are text output, key wait and line input (six entry points, all stubbed in our oracle).

## Pacing facts that shape the design (V: measured on our machine, tools in TASK-013c/019)
One pass of the main loop (`$530F`) costs **2,987 cycles in 59% of passes** and 6,000-28,000 cycles in the rest (mean 4,823; 2,266 passes in 8M instructions). At 1.0227 MHz that is about 340 passes/s when cheap and about 40/s when heavy; the game therefore **slows down when many things are drawn**, exactly as the original did. A faithful port needs a cycle clock, not a fixed time step.
The speaker log from the intro jingle shows constant-pitch runs with spacings of 1,716-2,708 cycles (225-300 Hz), so the **click times must be reproduced in cycles** (docs/EMULATOR_COMPARISON.md).

## Proposed module layers (each can be built and tested alone)
| Layer | Responsibility | Depends on | Contains original data? |
|---|---|---|---|
| `verify` | Disk-image rule from docs/VERIFIER_SPEC.md (exact image or PAC-MAN program hash) | nothing | no (only the two expected hashes) |
| `datapack` | Builds a local data pack from a *verified* image and loads it at start | verify | **yes: the pack is original data, built on the user's machine, never committed or shipped** |
| `hires` | 8 KB hi-res page model, row-address math, the two blit routines, font/text drawing, pixel decoding | datapack (shapes) | no (code only) |
| `video` | Turns the hi-res page into pixels (Apple II color rules: palette bit, odd/even columns, neighbor blending) and presents them (Direct3D 11; scaling, optional high refresh; HDR evaluated later) | hires | no |
| `clock` | Virtual 1.0227 MHz cycle counter; each reconstructed routine adds its measured cycle cost; paces real time (faithful mode) | - | cycle costs are numbers measured from the oracle |
| `audio` | Takes (cycle, toggle) events and renders them to 48 kHz samples (XAudio2/WASAPI), mute toggle (Ctrl-S behavior) | clock | no |
| `input` | Keyboard (original keys, customised keys, Esc pause), gamepad (XInput) mapped to the four direction codes, paddle emulation for joystick mode | - | no |
| `game` | Reconstructed logic in readable C++: state (replacing the zero-page variables), playfield map, Pac-Man and ghost movement, dots/energizers/fruit, scoring, lives, levels, 3 intermissions, hidden rank/cartoon menu | hires, clock, audio, input, datapack | no (logic written by us; tables come from the pack) |
| `screens` | Menus, attract sequence ("Character / Nickname", copyright line, "Press Space bar"), player-count and play prompts | game, hires | strings come from the pack |
| `app` | Windows window, message loop, settings, launcher text for the verification step | all | no |

## The oracle (development only, never shipped)
`tools/cpu6502.py` + `tools/apple2.py` (Python) run the original program and are compared with AppleWin (docs/EMULATOR_COMPARISON.md: 8 of 27,136 game bytes differ at a quiet checkpoint, 7 during play, all explained). Every reconstructed routine is checked against this oracle:
1. **Routine tests:** record the oracle's registers/memory when a routine is entered and left, replay those vectors on the C++ routine (vectors made from original data stay local; tests that need them are skipped when absent).
2. **Scenario tests:** run the same key script in the oracle and in the port's headless mode; compare the hi-res page and key variables at checkpoints (score, lives, level, playfield map, ghost positions).
3. **Spot checks** against AppleWin screenshots and memory dumps (as in TASK-016/019/012).

## Data flow at run time
`start -> locate disk image (path chosen once by the user) -> verify (read once, hash) -> data pack found? yes: check it was built from the verified hash; no: build it -> game runs entirely from the pack and the C++ code; the disk image is not read again`.
