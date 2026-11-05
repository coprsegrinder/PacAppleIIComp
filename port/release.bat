@echo off
rem Builds the release: the program with the C++ runtime linked in (it runs on a machine without Visual Studio), staged with the two Apple II ROM files, the player's readme and the notices,
rem and packed into out\release\PacMan-AppleII.zip. Needs Visual Studio Build Tools (like build.bat). The disk image of the game is NOT part of it: the player puts theirs next to PacMan.exe.
setlocal
set "ROOT=%~dp0"
set "REL=%ROOT%..\out\release"
set "PACMAN_OUT=%REL%\bin"
set "PACMAN_CRT=/MT"
if exist "%REL%" rmdir /s /q "%REL%"
call "%ROOT%build.bat"
if errorlevel 1 exit /b 1
set "STAGE=%REL%\PacMan-AppleII"
mkdir "%STAGE%\Apple II roms"
copy /y "%REL%\bin\pacman_play.exe" "%STAGE%\PacMan.exe" >nul || exit /b 1
copy /y "%ROOT%..\Apple II roms\Apple II plus ROM Pages F8-FF - 341-0020 - Autostart Monitor.bin" "%STAGE%\Apple II roms\" >nul || exit /b 1
copy /y "%ROOT%..\Apple II roms\Apple II Character ROM - 341-0036.bin" "%STAGE%\Apple II roms\" >nul || exit /b 1
copy /y "%ROOT%release\README.txt" "%STAGE%\" >nul || exit /b 1
copy /y "%ROOT%release\NOTICES.txt" "%STAGE%\" >nul || exit /b 1
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%' -DestinationPath '%REL%\PacMan-AppleII.zip' -Force" || exit /b 1
echo Release: %REL%\PacMan-AppleII.zip
exit /b 0
