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
