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
| Ctrl-S | $93 | toggles sound: flips the low operand byte of the two speaker instructions at $7BB6/$7D07 between $30 (`BIT $C030`, speaker) and $20 (`BIT $C020`, no sound) | V (clicks 1,237 -> 0 -> 120 over equal intervals) |
| Esc | $9B | pause: waits for any key | code only (not run) |
| Space | $A0 | in demo ($E8 <> 0): jump to $4144 (players prompt) | V |

Mapping of codes to up/down: A = 1 and Z = 0 (Apple II convention A = up, Z = down; **Inferred**: one ambiguous frame, not yet shown conclusively). The direction codes are produced through three `BIT abs` skips ($6FC3/$6FC6/$6FC9) that each load a different constant (0, 3, 2, 1): the idiom from CODE_COVERAGE.md.
The compared key constants ($C1, $95, $88, $DA) are patched by the "customised keyboard" option ($498B writes $6FBA): that path ($4955-$4A00, 171 bytes) has not been run.
Default direction at the start of play: 3 (left) (V: $63 = 3 before any key).
Poll rate: the key poll runs roughly once per game frame, which takes well over 100k instructions in our machine; a key pressed shorter than that may seem ignored when testing (seen once, resolved by waiting longer).

## Joystick mode (mode 2) and the other readers
* $6FD0 (used when $F5 = 0 and no key): reads the annunciators $C058-$C05B in sequence and tests button input $C061 after each, producing a 4-bit code that indexes a 16-byte table at $700E (V: code; values not decoded). I: a keypad/four-switch controller. Annunciator stubs were added to tools/apple2.py for this.
* $7041 / $701E (used when $F5 <> 0): analogue paddle read: strobe $C070, count while $C064/$C065 are high (PREAD style, 99 iterations). The headless paddle model uses 11 cycles per unit (Inferred, unverified).
* Both modes were run headless with random input (tools/autoplay.py): no crash, no unknown I/O, games reach game over.

## What is not covered
Customised keyboard, Esc pause, high levels (intermissions) and fruit/bonus items are unexecuted: 2,407 bytes of the 10,456 static code bytes (23%) have not run (list in scratch/unexecuted.txt; largest runs: $8180 344 bytes, $8486 196, $83D0 174, $4955 171, $4147 149). All 8,049 executed bytes are inside the static code map (0 contradictions).
