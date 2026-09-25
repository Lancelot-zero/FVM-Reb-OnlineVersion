@echo off
setlocal
chcp 65001 >nul

rem ---- MapEditCreator dev script ----
rem Go and Node are not on PATH on this machine, add them first
set PATH=D:\Program Files\Go\bin;D:\Program Files\nodejs;%PATH%

cd /d "%~dp0"

echo ============================================
echo  MapEditCreator dev mode (hot reload)
echo  Close the app window to exit dev mode
echo ============================================
wails dev

echo.
pause
endlocal
