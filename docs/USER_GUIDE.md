# User guide

For the player's own readme (also in the release zip) see [`../port/release/README.txt`](../port/release/README.txt): how to start, the controls, the menu, the files the program writes and what to do when something goes wrong.

## Starting
Put your own disk image of the game (a `.dsk` file) next to `PacMan.exe` and start it. The first time, the program checks that it is the exact original (the same check as `pacman_verify.exe`, below), builds the game data from it once and keeps it in `%LOCALAPPDATA%\PacmanAppleIIPort\game.pmpk`; afterwards the image is not needed. Without the image and without kept data, the program asks you to choose the file.

From a source build: `Play-Game.bat` starts `out\bat\pacman_play.exe` with `scratch\local.pmpk`, a data pack built by hand.

## The tools
```
out\bat\pacman_verify.exe "C:\path\to\Pacman (1983)(Atari).dsk"
```
Only reads the file; prints one line and sets an exit code:
| Printed word | Meaning | Exit code |
|---|---|---|
| `exact-image` | The whole file is identical to the reference image this project was built from | 0 |
| `game-program` | The disk differs (for example other DOS tracks or volume number) but the `PAC-MAN` program on it is identical to the reference | 0 |
| `not-verified` | A readable DOS 3.3 disk (or not), but not this game; the line says why | 1 |
| `unreadable` | Wrong size or unreadable; only 140K `.dsk`/`.do` files in DOS sector order are supported (not `.po`, `.woz`, `.nib`) | 2 |

"Verified" means *matches the reference image the project was built from*; it does not mean "official release" (no public checksum for this game was found).

```
out\bat\pacman_pack.exe build "C:\path\to\game.dsk" game.pmpk     rem the game data (refuses a disk that is not verified)
out\bat\pacman_pack.exe info game.pmpk                             rem "data pack ok", or why it is refused
out\bat\pacman_play.exe --pack game.pmpk                           rem the game with that data
```
The data pack is private to you: it holds bytes of the original and is never to be shared (it is git-ignored).

## Options of the program
`out\bat\pacman_play.exe` also takes `--scale N`, `--roms <folder>`, `--settings <file>`, `--display fullscreen|borderless|windowed`, `--mute`, `--skip-boot`, `--level N`, `--items N`, `--log <file>` and the test aids `--aid-energizer`, `--aid-wander`, `--aid-endless`, `--aid-toast N`. The environment variables `PACMAN_DATA`, `PACMAN_SETTINGS` and `PACMAN_ACHIEVEMENTS` name other files for the game data, the settings and the achievements.
