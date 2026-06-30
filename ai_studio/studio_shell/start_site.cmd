@echo off
setlocal

set "PORT=%~1"
if "%PORT%"=="" set "PORT=8765"

pushd "%~dp0..\.." >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio\studio_shell\start_site_windows.ps1 -Port %PORT% -Restart -Open
popd >nul
