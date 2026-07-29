@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "DKP_ROOT="
if defined DEVKITPRO if exist "%DEVKITPRO%\msys2\usr\bin\make.exe" set "DKP_ROOT=%DEVKITPRO%"
if not defined DKP_ROOT if exist "D:\devkitPro\msys2\usr\bin\make.exe" set "DKP_ROOT=D:\devkitPro"
if not defined DKP_ROOT if exist "C:\devkitPro\msys2\usr\bin\make.exe" set "DKP_ROOT=C:\devkitPro"

if not defined DKP_ROOT (
    echo [ERROR] Khong tim thay devkitPro MSYS2 make.exe.
    pause
    exit /b 1
)

set "PATH=%DKP_ROOT%\msys2\usr\bin;%DKP_ROOT%\devkitARM\bin;%DKP_ROOT%\tools\bin;%PATH%"
for /f "usebackq delims=" %%I in (`"%DKP_ROOT%\msys2\usr\bin\cygpath.exe" -u "%DKP_ROOT%"`) do set "DKP_POSIX=%%I"
if not defined DKP_POSIX set "DKP_POSIX=/opt/devkitpro"
set "DEVKITPRO=%DKP_POSIX%"
set "DEVKITARM=%DKP_POSIX%/devkitARM"
"%DKP_ROOT%\msys2\usr\bin\make.exe" clean
set "ERR=%ERRORLEVEL%"
pause
exit /b %ERR%
