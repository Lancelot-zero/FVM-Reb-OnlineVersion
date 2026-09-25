@echo off
setlocal
chcp 65001 >nul

rem ---- MapEditCreator build script ----
rem Go and Node are not on PATH on this machine, add them first
set PATH=D:\Program Files\Go\bin;D:\Program Files\nodejs;%PATH%

cd /d "%~dp0"

echo ============================================
echo  Building MapEditCreator.exe ...
echo ============================================
wails build

echo.
if exist "build\bin\MapEditCreator.exe" (
    echo [OK] build\bin\MapEditCreator.exe

    rem ---- compiler is no longer embedded, ship it side by side ----
    if exist "LabMapCompiler.exe" (
        copy /y "LabMapCompiler.exe" "build\bin\LabMapCompiler.exe" >nul
        echo [OK] build\bin\LabMapCompiler.exe
    ) else (
        echo [WARN] LabMapCompiler.exe not found in project root
        echo [WARN] the editor will build, but compiling will fail at runtime
    )

    echo [TIP] You can rename the editor exe to any name, e.g. the Chinese title.
    echo [TIP] Distribute BOTH exe files together in one folder / zip.
) else (
    echo [FAIL] build failed, see messages above
)

echo.
pause
endlocal
