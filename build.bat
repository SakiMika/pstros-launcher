@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "DKP_ROOT="
if defined DEVKITPRO if exist "%DEVKITPRO%\msys2\usr\bin\make.exe" set "DKP_ROOT=%DEVKITPRO%"
if not defined DKP_ROOT if exist "D:\devkitPro\msys2\usr\bin\make.exe" set "DKP_ROOT=D:\devkitPro"
if not defined DKP_ROOT if exist "C:\devkitPro\msys2\usr\bin\make.exe" set "DKP_ROOT=C:\devkitPro"

if not defined DKP_ROOT (
    echo [ERROR] Khong tim thay devkitPro MSYS2 make.exe.
    echo Hay cai hoac cap nhat goi nds-dev.
    >"last_build.log" echo [ERROR] Khong tim thay devkitPro MSYS2 make.exe.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_with_log.ps1" -DkpRoot "%DKP_ROOT%"
set "ERR=%ERRORLEVEL%"

echo.
if "%ERR%"=="0" (
    echo [OK] Build completed. Xem ten ROM trong last_build.log.
) else (
    echo [ERROR] Build that bai. Ma loi: %ERR%
)
echo [LOG] Gui file last_build.log neu can sua loi tiep.
pause
exit /b %ERR%
