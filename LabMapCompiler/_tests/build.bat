@echo off
call "D:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
if %errorlevel% neq 0 (
    echo vcvars FAILED with error %errorlevel%
    pause
    exit /b 1
)
cl.exe /EHsc /std:c++17 /utf-8 compiler.cpp /Fe:LabMapCompiler.exe
if %errorlevel% equ 0 (
    echo Compile SUCCESS
) else (
    echo Compile FAILED
)
pause
