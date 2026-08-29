@echo off
setlocal
for %%I in ("%~dp0.") do set "GAME_ROOT=%%~fI"
set "STUDIO_ROOT=%GAME_ROOT%"
:find_studio
if exist "%STUDIO_ROOT%\ai_studio\dev_environment\python_run.mjs" if exist "%STUDIO_ROOT%\ai_studio\runtime_automation\capture_game.py" goto run_capture
for %%I in ("%STUDIO_ROOT%\..") do set "PARENT=%%~fI"
if /I "%PARENT%"=="%STUDIO_ROOT%" (
  echo ERROR: Studio root not found above "%GAME_ROOT%" 1>&2
  exit /b 1
)
set "STUDIO_ROOT=%PARENT%"
goto find_studio
:run_capture
node "%STUDIO_ROOT%\ai_studio\dev_environment\python_run.mjs" "%STUDIO_ROOT%\ai_studio\runtime_automation\capture_game.py" "%GAME_ROOT%" %*
exit /b %errorlevel%
