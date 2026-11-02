@echo off
rem Starts the current state of the Pac-Man port (the ghosts do not move yet).
rem The game data is built ONCE from YOUR OWN disk image (it is never shipped): put "Pacman (1983)(Atari).dsk" next to this file or one folder up.
cd /d "%~dp0"
set "DSK=Pacman (1983)(Atari).dsk"
if not exist "%DSK%" set "DSK=..\Pacman (1983)(Atari).dsk"
if not exist "game.pmpk" (
  if not exist "%DSK%" (
    echo Cannot find "Pacman (1983)(Atari).dsk" next to this file or one folder up.
    pause
    exit /b 1
  )
  "%~dp0pacman_pack.exe" build "%DSK%" game.pmpk
  if errorlevel 1 (
    echo The disk image was not accepted - it must be the exact original image.
    pause
    exit /b 1
  )
)
start "" "%~dp0pacman_play.exe" --pack game.pmpk --scale 3
