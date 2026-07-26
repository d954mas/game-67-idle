# Windows process-loopback helper

This private recorder helper captures audio rendered by one Windows process
tree into a finite PCM WAV file. It is a feasibility implementation for the
FFmpeg capture spike, not a public recorder backend.

The implementation uses the Windows
`AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK` contract documented by Microsoft
and follows the activation sequence demonstrated by Microsoft's MIT-licensed
[ApplicationLoopback sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/ApplicationLoopback).
It does not copy the sample's WIL/Media Foundation implementation and has no
NuGet dependency.

Requirements:

- Windows build 20348 or newer;
- Visual Studio 2022 C++ tools and a current Windows SDK;
- an owned positive target PID and its Windows creation-time identity.

Build:

```powershell
cmake -S ai_studio/runtime_automation/capture/native/windows_process_loopback `
  -B tmp/capture/windows-process-loopback-build -A x64
cmake --build tmp/capture/windows-process-loopback-build --config Release
```

Run:

```powershell
tmp/capture/windows-process-loopback-build/Release/windows_process_loopback.exe `
  --pid 1234 --expected-creation-time-100ns 134295417401802263 `
  --include-tree --output tmp/capture/game.wav --duration-ms 10000
```

Successful stdout is one JSON object. Diagnostics go to stderr and a failure
returns non-zero. The helper verifies creation time, retains the process handle,
reports target exit and timeline discontinuities, and limits classic RIFF output
to six hours. The Python boundary supplies a unique staging path and publishes
only a validated WAV. The current finite-WAV interface exists to prove routing
and isolation. A production FFmpeg adapter still needs a streaming transport
and shared start/stop synchronization.
