# Controls and input (TASK-018)

Sources: code at $6F5E-$7040 read statically (V: instruction semantics), and checked by driving the headless machine (V where marked). Tools: `tools/play.py`, `tools/autoplay.py`.

## Flags and variables
| Address | Meaning | Evidence |
|---|---|---|
| $E8 | 1 = demo/attract running, 0 = a real game (set at $43D2 / $54D4) | V: 0 during play, back to 1 at game over |
| $F5 | input device chosen at the mode menu: 0 = keyboard ('1'), 1 = Apple joystick ('2') | V from code at $48CD-$48D4 |
| $63 | requested direction, written by the input poll when a direction key/switch is seen (carry clear) | V |
| $51 | number of players (menu key & 7) | V from code at $4245 |

## Keyboard (mode 1), routine $6F77 (polled directly via $C000/$C010, not through the monitor)
| Key | Code | Effect | Evidence |
|---|---|---|---|
| A | $C1 | direction code 1 | V (driven: $63 = 1) |
| Z | $DA | direction code 0 | V (driven: $63 = 0) |
| Left arrow (Ctrl-H) | $88 | direction code 3 | V |
| Right arrow (Ctrl-U) | $95 | direction code 2 | V |
| (digital joystick, mode 1) | game-port `$C061` read 4 times | the keyboard mode also accepts four switches on the game port (masks 1, 2, 4, 8 = one direction each; everything else = no input), see UNEXECUTED_CODE.md | V (all 16 masks run) |
| Ctrl-S | $93 | toggles sound: flips the low operand byte of the two speaker instructions at $7BB6/$7D07 between $30 (`BIT $C030`, speaker) and $20 (`BIT $C020`, no sound) | V (clicks 1,237 -> 0 -> 120 over equal intervals) |
| Esc | $9B | pause: waits for any key | V (driven in TASK-020: parks at $6F9A, any key resumes) |
| Space | $A0 | in demo ($E8 <> 0): jump to $4144 = `JMP $4217` (players prompt); also tested at $5292 and $52EE (033-4e) | V |

Mapping of codes to directions: **code 1 = UP, 0 = DOWN, 3 = LEFT, 2 = RIGHT (V)**: the customised-keyboard prompts ask for "MOVE LEFT", "MOVE RIGHT", "MOVE UP", "MOVE DOWN" and store the answers in the compare constants for codes 3, 2, 1, 0 respectively (TASK-020). So A = up and Z = down by default. The direction codes are produced through three `BIT abs` skips ($6FC3/$6FC6/$6FC9) that each load a different constant (0, 3, 2, 1): the idiom from CODE_COVERAGE.md.
The compared key constants ($C1, $95, $88, $DA) are patched by the "customised keyboard" option ($498B writes $6FBA): that path ($4955-$4A00) was run in TASK-020 (see below).
Default direction at the start of play: 3 (left) (V: $63 = 3 before any key).
Poll rate: the key poll runs roughly once per game frame, which takes well over 100k instructions in our machine; a key pressed shorter than that may seem ignored when testing (seen once, resolved by waiting longer).

## Joystick mode (mode 2) and the other readers
* $6FD0 (used when $F5 = 0 and no key): reads the annunciators $C058-$C05B in sequence and tests button input $C061 after each, producing a 4-bit code that indexes a 16-byte table at $700E (V: code; values not decoded). I: a keypad/four-switch controller. Annunciator stubs were added to tools/apple2.py for this.
* $7041 / $701E (used when $F5 <> 0): analogue paddle read: strobe $C070, count while $C064/$C065 are high (PREAD style, 99 iterations). The headless paddle model uses 11 cycles per unit (Inferred, unverified).
* Both modes were run headless with random input (tools/autoplay.py): no crash, no unknown I/O, games reach game over.

## Customised keyboard, pause and the hidden menu (TASK-020, V: driven headless)
* Second menu 'C': the game asks for four keys in the order LEFT, RIGHT, UP, DOWN (rejecting repeats) and patches the compare constants $6FBA, $6FB6, $6FB2, $6FBE. Test with J, L, I, M: the patched constants became $CA $CC $C9 $CD and J -> code 3, L -> 2, M -> 0 as expected; I gave 2 at the sample time (I: the up move was blocked at that spot and the game kept the old direction; not shown).
* Esc pauses: the machine parks in the wait loop at $6F9A; any key resumes (V).
* **Hidden menu (not documented in the game's screens):** at "HOW MANY PLAYERS?" press Ctrl-V then Ctrl-C ($96, $83): prompt "RANK OR CARTOON?". Typing C leads to "CARTOON NUMBER? (1-3)" and plays that intermission (routine $8155), then returns to the players prompt (all three played, V). Anything else leads to "RANK NUMBER? (1-21)": the number is stored at $54E1 and becomes the starting level (21 stored as $15: V). A game started at rank 21 ended after 8.2M instructions with random play (V).

## What is not covered
After TASK-020, 9,147 of the 10,456 static code bytes (87.5%) have run; 1,309 bytes in 125 runs have not (list in scratch/unexecuted.txt; the biggest runs are only 76, 49, 48, 47 bytes: $951F, $8F64, $9690, $805D). Several of the 48 dispatch-table routines ($8F6C, $8FBE, $8FED, $951F, $954A, $956B, $95EA, $969D) are among them: they need specific game situations (U). All 9,147 executed bytes are inside the static code map (0 contradictions).

**Update 2026-11-03 (033-4e2):** the hidden menu is ported and checked against the oracle: see docs/M8_PLAN.md (033-4e2 result) for the rank rules (one character: low nibble unless 0; two: nibble x 10 + nibble, below 22) and the cartoon numbers (1, 2, anything else = 3).

Port hotkeys (not game keys): F11 or Alt+Enter = full screen; F8 = title bar and frame on/off (windowed mode); Ctrl+Plus / Ctrl+Minus = window one whole-number scale larger/smaller; F9 = mute. In the borderless window, drag the picture to move it and drag the edges to resize it.

**Esc = pause menu (TASK-034-3):** the original Esc pause (wait for any key) is replaced. Esc opens the menu (Resume / Graphics Settings / Quit Game); Up/Down + Enter or the mouse choose; Esc goes back/closes. The game stands still while it is open.

**Display mode (TASK-034-4):** pause menu (Esc) > Graphics Settings > Display Mode (Enter or Right = next, Left = previous): Fullscreen, Borderless Fullscreen (default), Windowed. F11 / Alt+Enter switch between the window and the full screen mode last used. The choice, the window size and place and the window frame setting are remembered in %LOCALAPPDATA%\PacmanAppleIIPort\settings.ini.
**Display mode (corrected):** only two modes exist: Borderless Fullscreen (default) and Windowed (pause menu > Graphics Settings > Display Mode, or F11 / Alt+Enter). The exclusive "Fullscreen" mode was removed.

**Controllers (Controls section, 2026-11-05):** see docs/MODERNIZATION.md. Keyboard: A / Z / arrow keys move, Space starts, Esc opens the pause menu (Controls page lists everything per device). XInput: D-pad or left stick, A = confirm / start, B = back, Start = menu. DualShock 4 and DualSense: D-pad or left stick, cross = confirm / start, circle = back, Options = menu.

## Joystick mode: the original's paddle routines (TASK-034-PADDLE, 2026-11-08)
Mode 2 of the first menu ("X. CONTROLLER" / "A. CONTROLLER" in the port; the program gives the original the digit 2) sets `$F5` <> 0. The original then reads the stick with its own routines, which the port now runs
(`src/core/input.cpp`): `$701E` triggers the two paddle timers (`$C070`) and counts 99 passes (27 cycles each) of `BIT $C064` / `BIT $C065` into X and Y; `$7041` compares the counts (below `$1C` = 28 is
left / up, `$44` = 68 or more is right / down) and answers a direction 0-3 or `$80` (none), starting with the axis Pac-Man is not moving along (`$62`). **`$7041` never sets the carry, so `$6F6F` always writes `$63`:
with the stick in the middle, or still pushed the way he already moves, `$63` becomes `$80`.**
* **Where the original polls in joystick mode (V: tools/record_joystick_ticks.py, an 8,165-tick game, equals the port in every field):** NOT at the start of every tick (`$7865` is skipped when `$F5` <> 0) but at `$7871`, when Pac-Man's step timer has run out
  (once per step); once at the end of every life / level start (`$5442`); after each of the 29 notes of the intro jingle (`$7D26`); and in the attract demo (`$E8` <> 0), where `$7041` answers `$80` at once (V: tools/record_joystick_demo.py, 4,000 demo ticks equal).
* **Cost:** the paddle timing takes 2,696 cycles plus the decision: about 2,650 cycles more than the keyboard poll, at each poll above (V: the cycles of every vector equal the oracle's). A step of Pac-Man (about 40,000 cycles) is therefore about 6 % longer with a controller (and the ticks between the steps are 67-127 cycles shorter than with the keyboard: the start-of-tick poll is skipped), the jingle 77,000 cycles (75 ms) longer.
* **The paddle hardware is a model (U):** a paddle at position v (0..255) stays high for 11 x v cycles after the trigger (the oracle's own model, tools/apple2.py; a real Apple II paddle is 24 microseconds plus 0.011 microsecond per ohm). The routines are the original's; only this timer is assumed.
* **Port side:** the D-pad pushes its axis to the end (0 or 255); otherwise the left stick's position is used as it is (`padPaddles`, gamepad.cpp), so the original's thresholds (counts 28 and 68, positions about 67 and 165) are the dead zone. Xbox, DualShock 4 and DualSense all use it.
* **Keyboard keys do not steer in this mode** (as in the original: the poll that follows the key stage always overwrites `$63`).
* **Tests:** `tests/paddle_vectors.inc` (made by tools/record_paddle.py from runs of the original routines on the oracle: 256 positions and 6,120 decisions over all four directions and game / demo, with counts, carry, A and cycles) in test_input.cpp; the full-game recordings
  `--joyticks scratch/ticks_JOY.bin` and `--joydemo scratch/joydemo/ticks_DEMO.bin` in test_gamecore.cpp; the jingle's longer gaps in test_sound.cpp.
* **Not covered (U):** the attract screens' own polls (`$4347`, `$46D0`, `$4763`, `$52AA`, `$52F9`) in joystick mode take 63 instead of 62 cycles (V: 3,002 polls on the oracle); the port's title and chase scripts do not add it (about 200 polls a turn, 200 of 63 million cycles). The poll of a life's set-up (`$5442`) is added to the READY wait since 2026-11-09, and the tick costs in joystick mode are checked tick by tick (docs/M8_PLAN.md, TASK-034-TIMING): ticks without a step of Pac-Man do not pay the start-of-tick poll.
