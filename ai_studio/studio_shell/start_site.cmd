@echo off
setlocal

set "PORT=%~1"
if "%PORT%"=="" set "PORT=8765"

pushd "%~dp0..\.." >nul
start "AI Studio :%PORT%" /min node ai_studio\studio_shell\server.mjs %PORT%
popd >nul

echo AI Studio starting at http://127.0.0.1:%PORT%/
echo If it does not open, run: node ai_studio\studio_shell\start_site.mjs --restart --open
