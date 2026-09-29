@echo off
rem ============================================================
rem DLL_VM\tests\build.bat - build the inner + outer layer self-test
rem   usage:  cd DLL_VM\tests  &&  build.bat
rem ============================================================
call "D:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
if not exist obj mkdir obj
rem NOTE: keep this file ASCII-only - cmd.exe reads .bat in the console codepage
rem       (GBK here), so UTF-8 comments can break the batch parser.
rem /W4: whole tree only warns on fopen (C4996), silenced by _CRT_SECURE_NO_WARNINGS.
rem       Why /W4: unused parameters (C4100) do get reported, so "(void)x;" means
rem       something - it is how we say "this parameter is deliberately ignored".
rem NOTE: host\host_layout.h (shared-memory layout contract) has no .cpp of its
rem       own - header-only, pulled in by host_shared.cpp (host_layout_version)
rem       and selftest.cpp. Regenerate: python tools\gen_host_layout.py
rem       (NOTE: never put a "rem" line inside the ^ continuation below)
cl.exe /nologo /W4 /EHsc /std:c++17 /utf-8 /O2 /D_CRT_SECURE_NO_WARNINGS ^
    ..\core\vm.cpp ..\core\pool.cpp ..\core\instance.cpp ..\core\call.cpp ..\core\const_str.cpp ^
    ..\outer\fn_table.cpp ..\outer\forward.cpp ..\outer\callbin_fn.cpp ..\outer\dispatch.cpp ^
    ..\outer\fn_dll.cpp ^
    ..\outer\container.cpp ..\outer\inst_fn.cpp ..\outer\builtin.cpp ..\outer\host_fn.cpp ^
    ..\outer\host_real.cpp ..\outer\host_state_in.cpp ..\business\native_biz.cpp ^
    ..\outer\game_api.cpp ..\outer\game_self.cpp ..\outer\game_a.cpp ..\outer\game_b.cpp ^
    ..\outer\game_c.cpp ..\outer\hot\hot_table.cpp ^
    ..\outer\host_push.cpp ^
    ..\outer\packet.cpp ^
    ..\host\host_input.cpp ..\host\host_draw.cpp ..\host\host_file.cpp ^
    ..\host\host_buffer.cpp ..\host\host_net.cpp ..\host\host_shared.cpp ^
    ..\host\host_state.cpp ..\host\host_packet.cpp ^
    selftest.cpp ^
    /Fe:selftest.exe /Fo:obj\
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD OK
