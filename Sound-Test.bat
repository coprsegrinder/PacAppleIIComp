@echo off
rem Plays every sound of the game once (about 15 seconds) so you can listen to it.
cd /d "%~dp0"
out\bat\pacman_play.exe --pack scratch\local.pmpk --sound-test
echo.
echo Done. Press a key to close.
pause
