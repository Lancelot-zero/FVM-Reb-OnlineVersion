@echo off
rem Sync VM functions from vmfuncs_spec.json into the editor (see README.md).
rem Usage: sync_vmfuncs.bat              write files
rem        sync_vmfuncs.bat --dry-run    preview only
rem Also update manually: help.md and updateLog in app.go
chcp 65001 >nul
cd /d "%~dp0"
python sync_vmfuncs.py %*
pause
