PAC-MAN (Apple II, 1983) - native Windows port
===============================================

This is an unofficial, non-commercial fan project: a native Windows program that plays the 1983 Atari
version of Pac-Man for the Apple II. It is not an emulator. It reproduces the original game's logic
and timing; the game's data comes from YOUR OWN copy of the original disk image, which is not part
of this download.


HOW TO START
------------
1. Put your disk image of the game (a .dsk file, for example "Pacman (1983)(Atari).dsk") in this folder,
   next to PacMan.exe.
2. Double-click PacMan.exe.

The first time, the program reads the disk image once, checks that it is the exact original, and keeps
what it needs in %LOCALAPPDATA%\PacmanAppleIIPort. After that the disk image is not needed any more
(you may move or delete it). If the program cannot find the image it asks you to choose it.

Do not move or delete the "Apple II roms" folder: the program needs the two ROM files in it.


CONTROLS  (Pause menu > Controls shows them for the device you are using)
--------
                       Keyboard            Xbox controller      PlayStation (DualShock 4 / DualSense)
  Move                 A / Z / arrow keys  D-pad or left stick  D-pad or left stick
  Start / confirm      Space / Enter       A                    X (cross)
  Back                 Esc                 B                    O (circle)
  Pause menu           Esc                 Start                Options
  Sound on / off       Ctrl-S
  Mute                 F9
  Full screen          F11 or Alt+Enter

The first screen asks how you want to play: Space for the keyboard, or the confirm button of a
controller that is plugged in (the screen names it: X or A). Controllers are found automatically,
also when plugged in while the program runs. Games for two to five players are started from the keyboard.
Vibration, and on a DualShock 4 or DualSense the light bar (in the game's own colours: white, blue while the ghosts are blue, a flash in a ghost's colour when one is eaten or catches Pac-Man; the DualSense's five small LEDs show the lives), are set under Pause menu > Controls (each Off / Low / Medium / High).
Xbox (XInput) controller support is implemented from Microsoft's documentation only: it has not been tried on a real Xbox controller.


THE MENU (Esc, or the menu button of a controller)
--------
Resume, Graphics Settings, Controls, Achievements, Quit Game.

Graphics Settings: display mode (borderless full screen or window), resolution, window borders,
background (black, ambient glow, an Apple II CRT monitor, the Apple II desktop), colour mode
(original colour or green phosphor), and a CRT filter (scanlines, softness, glow, curvature, ...).
Everything you change is kept for the next time.

Achievements: 22 achievements (190 points) from the RetroAchievements set for this game, checked by the
program while you play. They are kept in this program only; nothing is sent anywhere.


FILES THE PROGRAM WRITES  (all in %LOCALAPPDATA%\PacmanAppleIIPort)
-------------------------
  game.pmpk         the game data built from your disk image
  settings.ini      your settings
  achievements.ini  your achievements
To start from scratch, delete that folder. To remove the program, delete this folder and that one.


IF SOMETHING GOES WRONG
-----------------------
* "not the disk image the program expects": the file has to be the exact original disk of the game
  (Atari, 1983, 140K DOS order). Other dumps, other versions and modified images are refused.
* A controller is not found: unplug it and plug it in again; a DualShock 4 / DualSense works over USB
  or Bluetooth, an Xbox controller as a normal Windows game controller.
* No picture or a black window: the program needs Windows 10 or 11 and a Direct3D 11 graphics card.

See NOTICES.txt for credits and the legal notes.
