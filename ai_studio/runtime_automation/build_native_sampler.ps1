param([string]$Out = "tmp/perf_tools")
$ErrorActionPreference = "Stop"
$dbg = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64"
New-Item -ItemType Directory -Force $Out | Out-Null
clang -O2 -D_CRT_SECURE_NO_WARNINGS "$PSScriptRoot\native_sampler.c" -o "$Out\native_sampler.exe" -ldbghelp -lwinmm
if ($LASTEXITCODE -ne 0) { throw "clang failed" }
# System32 dbghelp has no symbol server, so the SDK debugger DLLs ship beside the exe.
Copy-Item "$dbg\dbghelp.dll", "$dbg\symsrv.dll" $Out -Force
