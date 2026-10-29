# Known issues and open questions

## Product
* The game is not playable: only the disk verifier exists (milestone M0 of `docs/PORT_PLAN.md`).
* The verifier supports only 140K DOS-order images (`.dsk`/`.do`). `.po`, `.woz`, `.nib`, `.2mg` are refused, not converted.
* The C++ verifier and the Python tool word the "cannot open file" error differently (same exit code).

## Facts not established
* **No public checksum** for this game image exists in what was searched; the reference hashes come only from the supplied image (`docs/VERIFIER_SPEC.md`).
* Which revision or dump the supplied image is: Unknown. Whether its modified DOS was Atari's: Unknown (`docs/BOOT_TRACKS.md`). Copy protection: Unknown (a `.dsk` cannot show nibble-level protection).
* The reported lineage from Taxman (H.A.L. Labs, 1981) is unverified; the Taxman source has no known public host (TASK-023).
* Ghost movement rules, speeds per level, fruit rules, scoring/extra-life rules, sound data and intermission scripts are not yet understood (`docs/PORT_PLAN.md`, section 1).
* 1,309 bytes of identified code have not been executed in any test run (TASK-021).

## Verification limits
* The 6502 reference model is checked against AppleWin only at two checkpoints (players prompt and an Esc-paused moment of play); no frame-synchronised or audio comparison yet (TASK-022).
* The colour model used in our renders is an approximation of Apple II NTSC output.
