# Comparison with AppleWin (TASK-019)

Tools: `tools/compare_dump.py` (memory diff against an AppleWin dump), `tools/save_clicks.py` (speaker click timings), `tools/awin_ctl.ps1` (window-only capture and debugger commands).
Dump file and screenshots stay in git-ignored `scratch/` (pl.bin, aw_prompt.png, aw_dbg_pl*.png).

## Method (V)
1. AppleWin 1.32.1.0 launched from `scratch/` with a copy of the disk (`scratch/work.dsk`, hash re-checked equal to the original before and after).
2. Keys sent by `awin_ctl.ps1`: `1` (+Enter) at the mode menu, ~12 s of attract, then Space (+Enter): the game shows "HOW MANY PLAYERS?" and waits in a key loop. A window-only screenshot confirmed the prompt.
3. F7 (debugger), then `BSAVE "pl.bin",0,C000` dumped $0000-$BFFF to `scratch/pl.bin` (49,152 bytes). Cycle counter at the dump: 45,962,051 (including the DOS boot).
   Lesson: calling `powershell -File awin_ctl.ps1 -Text '...'` from PowerShell strips the double quotes and the debugger only printed BSAVE usage; calling the script in-process (`& .\tools\awin_ctl.ps1 -Text 'BSAVE "pl.bin",0,C000'`) works.
4. The same keys on our machine (`our_state`): `1`, Enter, N instructions of attract, Space, Enter, run to the key wait at $FD0C. The prompt is a quiet state, so the comparison does not depend on exact timing.
5. AppleWin was killed afterwards (0 processes).

## Result at N = 3,000,000 attract instructions (8.3M cycles; V: measured)
| Region | Differing bytes |
|---|---|
| **Game $4000-$A9FF (27,136 bytes)** | **8** (all inside static code: table-pointer operands) |
| text page 2 ($0800) | 1 (the cursor cell: AppleWin draws a cursor, our stub does not) |
| zero page / stack / $200-$3FF / text page 1 / $0C00-$1FFF | 129 / 149 / 252 / 35 / 64: DOS, monitor ROM and //e firmware state that we do not model (expected) |
| hires page 1 | 700 (picture drawn at a different moment of the attract animation) |
| $AA00-$BFFF | 4,006: DOS/ROM-resident data above the game (expected) |
The 8 game bytes are exactly the self-modified operands of the table readers: $55B2-$55B3, $6951-$6952, $695A-$695B, $6A4B-$6A4C (the last base address each reader was given depends on which sprite was drawn last, i.e. on the attract animation moment).
**Everything else in the game area is byte-identical to AppleWin, including the 896-byte runtime table at $623A-$65B9, the dispatch tables and all patched code.** This is a strong check of the 6502 core, the soft-switch/ROM stubs and the relocation.
With longer attract runs (10M / 25M instructions) the game area differs in 68 / 173 bytes (12 / 16 inside code): more moving-animation variables differ because the attract loop is at a different point (expected, not analysed byte by byte).

## Speaker click timings (our machine only; NOT verified against AppleWin)
`python -I tools/save_clicks.py` saves the cycle count of every $C030 access from the Enter that starts a game (scratch/clicks.json, 2,576 clicks in 35 s of Apple time with no steering).
The intro jingle is a sequence of constant-pitch runs; the first ones are: 96 clicks at spacing 2,026 cycles (~252 Hz toggle rate = square wave of that frequency), then 225 Hz, 189 Hz, 298 Hz runs (spacing 2,274 / 2,708 / 1,716 cycles).
Timing is cycle-based on our core (V for the core's cycle counts: unit tests; the 6502 instruction timings were not checked against real hardware). A faithful port must reproduce the click times, not just the pitch.
AppleWin cannot be compared here without audio capture (not done).

## Limits
One checkpoint (the players prompt) in one scenario; no gameplay dump comparison (would need frame-exact input timing); the hi-res picture and animation variables cannot be compared without synchronised timing; the keyboard 'Enter' appended by the helper reaches the players prompt as a harmless ignored key.

# Gameplay checkpoint (TASK-012)

Session 2026-10-29, same method as above plus the Esc pause as a quiet state: AppleWin from `scratch/`, keys `1`, ~12 s attract, Space, `1` (Enter appended: starts the game), ~8 s of play, **Esc** (`-Key Esc`), F7. The debugger showed **PC = $6F9A**, the pause loop of the keyboard poll (V: the code listing on screen is the routine decoded in CONTROLS.md), then `BSAVE "gp.bin",0,C000` (49,152 bytes, cycle counter 57,990,439). AppleWin killed afterwards; disk hashes unchanged. Screenshot: real gameplay, score 30, Pac-Man and four ghosts.
Our side: `python -I tools/compare_dump.py scratch/gp.bin play:3_000_000:3_500_000` (start a game after 3M attract instructions, play 3.5M instructions, press Esc, run to $6F9A).

## Result (V: measured). Because play is not frame-synchronised, four play lengths were tried; the best is reported and the others are listed
| Play length (instructions) | Game area $4000-$A9FF differing bytes (code operands / data) | Hi-res page 1 differing bytes |
|---|---|---|
| 2.0M | 13 (10 / 3) | 182 of 8,192 |
| **3.5M** | **7 (4 / 3)** | **140** |
| 5.0M | 10 (6 / 4) | 217 |
| 7.0M | 48 (14 / 34) | 200 |
At 3.5M: zero page game variables $63, $E8, $F5, $51 are identical to AppleWin ($63 = 3 left, $E8 = 0 in play, $F5 = 0 keyboard, $51 = 1 player); text page 2 identical; the pause loop PC matches.
The 7 differing game bytes: the usual **table-pointer operands** ($6951-$6952, $695A-$695B: 4 bytes) and **3 bytes of the playfield table** ($6331, $6351, $6371: ours 0, AppleWin 1), i.e. AppleWin's Pac-Man had not yet eaten three dots our run had; this is a timing difference, not a logic difference.

## Answers to the TASK-012 questions
1. **Is there a table at $0666 during play?** No. $0660-$066F holds $A0 (the blank text-page fill) in both AppleWin and our machine. The `$0666` operands in the file are placeholders patched at runtime (confirmed in two independent emulators; TASK-013c-1).
2. **What writes $623A-$65B9?** Code at **$5345** copies a fresh 896-byte source block into it at the start of each game (890 bytes written per game start), and code at **$7B10** (`STA ($16),Y`) changes single bytes during play: 6 bytes at stride $20 ($6331, $6351, ...) in 3.5M instructions. Values seen: 0, 1, 2.
3. **What is the block? (I)** A **playfield map of 32 columns x 28 rows (896 bytes)**: 1 = dot, 2 = energizer, 0 = empty/eaten, rewritten from the master copy at every new game; a byte is cleared when its dot is eaten (stride $20 = one row of 32). Not yet checked cell by cell against the picture.
