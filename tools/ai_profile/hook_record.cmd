@echo off
setlocal
set "HOOK_DIR=%~dp0"
if exist "%HOOK_DIR%hook_record_fast.exe" (
  "%HOOK_DIR%hook_record_fast.exe" %*
) else (
  node "%HOOK_DIR%hook_record.mjs" %*
)
exit /b 0
