# User guide (current state: verifier only)

The game itself is not playable yet (see `docs/PORT_PLAN.md`). What you can do today is check that your disk image is the game this project is built for.

## Checking your disk image
```
out\bat\pacman_verify.exe "C:\path\to\Pacman (1983)(Atari).dsk"
```
The program only reads the file. It prints one line and sets an exit code:
| Printed word | Meaning | Exit code |
|---|---|---|
| `exact-image` | The whole file is identical to the reference image this project was built from | 0 |
| `game-program` | The disk differs (for example other DOS tracks or volume number) but the `PAC-MAN` program on it is identical to the reference | 0 |
| `not-verified` | A readable DOS 3.3 disk (or not), but not this game; the line says why | 1 |
| `unreadable` | Wrong size or unreadable; only 140K `.dsk`/`.do` files in DOS sector order are supported (not `.po`, `.woz`, `.nib`) | 2 |

"Verified" here means *matches the reference image the project was built from*. It does not mean "official release": no public checksum for this game was found (`docs/VERIFIER_SPEC.md`).

## Building and checking the data pack (developer preview)
```
out\bat\pacman_pack.exe build "C:\path\to\game.dsk" out\pack\pacman.pmpk
out\bat\pacman_pack.exe info out\pack\pacman.pmpk
```
`build` refuses disks that are not verified and writes nothing in that case. `info` reports `data pack ok` or why the pack is refused. The pack is private to you: never share or commit it.

## What will come later
A launcher that asks for the disk image once, verifies it, builds a local data pack in your user folder and then runs the game from that pack (decision D1-A in `docs/PORT_PLAN.md`). Game controls of the original are documented in `docs/CONTROLS.md` (A/Z/arrow keys, Ctrl-S sound toggle, Esc pause, customised keys).
