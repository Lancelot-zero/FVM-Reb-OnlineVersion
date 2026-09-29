@echo off
rem ============================================================
rem build.bat - build the interpreter
rem ============================================================
call "D:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsamd64_x86.bat" >nul
cd /d "%~dp0"
if not exist obj mkdir obj
cl.exe /nologo /W3 /EHsc /std:c++17 /utf-8 /O2 vm.cpp loader.cpp main.cpp log.cpp selftest.cpp vm_api.cpp vm_native.cpp vm_container.cpp vm_instance.cpp vm_builtin.cpp vm_callbin.cpp kernel.cpp names.cpp hot\hot_common.cpp hot\hot_table.cpp hot\hot_card_parent.cpp hot\hot_enemy_parent.cpp hot\hot_enemy_mod.cpp hot\hot_card_mod.cpp hot\hot_bullet_mod.cpp hot\hot_bullet_screen_management.cpp hot\hot_card_slot.cpp hot\hot_player_character.cpp hot\hot_homing_bullet_management.cpp hot\hot_effect_mod.cpp hot\hot_weapon_mod.cpp hot\hot_gem_mod.cpp battle_kernel\step.cpp battle_kernel\battle_start.cpp /Fe:vm.exe /Fo:obj\
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD OK
