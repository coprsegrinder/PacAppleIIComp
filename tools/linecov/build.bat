@echo off
rem Builds tools\linecov\linecov.cpp into out\cov\linecov.exe (the line-coverage runner; see the top of the source file).
setlocal
set "ROOT=%~dp0..\.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%ROOT%\out\cov" mkdir "%ROOT%\out\cov"
cl /nologo /std:c++20 /EHsc /O2 /W4 /Fo"%ROOT%\out\cov\linecov.obj" /Fe"%ROOT%\out\cov\linecov.exe" "%~dp0linecov.cpp"
