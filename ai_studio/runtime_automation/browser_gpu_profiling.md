# Browser GPU Profiling

Finds what the Chrome GPU process spends its time on when a web game is slower
than its own GPU and CPU work explains. Windows, Chrome on ANGLE/D3D11.

## When to use

- Browser frame time is above what the game's CPU and GPU passes add up to.
- Trace shows `CrGpuMain` (the GPU process main thread) near saturation.

Frame time alone is vsync-quantised (16.7, 33.3 ms ...). The `CrGpuMain` busy
time per present is not, so judge changes by that.

## Setup

Run every command from the Studio repo root.

- Symbol cache: the Studio's git-ignored `tmp/symbols`. Chrome 156 needs about
  5.7 GB. The first run on a new Chrome version downloads several GB.
- Build the sampler and stage the debugger DLLs beside it (no `.exe` is committed):
  `./ai_studio/runtime_automation/build_native_sampler.ps1 [outDir]` (default
  `tmp/perf_tools`). Needs `clang` and the Windows SDK debugging tools. System32 `dbghelp.dll` has no symbol server; the script copies
  `dbghelp.dll` and `symsrv.dll` from `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\`.
- Symbol path:

  ```powershell
  $env:_NT_SYMBOL_PATH = "srv*$PWD\tmp\symbols*https://chromium-browser-symsrv.commondatastorage.googleapis.com;srv*$PWD\tmp\symbols*https://msdl.microsoft.com/download/symbols"
  ```

## Steps

1. Launch Chrome with `--remote-debugging-port=9222`. Chrome 136+ ignores that
   flag on the default profile, so headed and headless launches both need a
   non-default `--user-data-dir`. Take it from
   `ai_studio/runtime_automation/chrome_profile.py` (`chrome_profile.mjs` in node)
   and dispose it in `finally`. A hand-made throwaway dir is allowed only if it is
   removed after Chrome has exited; never a bare `mkdtemp`.
   Run the game scenario that shows the slowdown and keep it running.
2. Trace the GPU process, 5 s:
   `python ai_studio/runtime_automation/chrome_gpu_trace.py --port 9222 --seconds 5 --out tmp/perf_tools/trace.json`
   (`--summarize tmp/perf_tools/trace.json` re-reads a saved trace). Output: busy %,
   ms per present, presents/s, top inclusive events per present. A present is
   `DXGISwapChainImageBacking::Present` on `CrGpuMain`; `SwapBuffers` counts only
   when no Present event exists. With neither, per-present metrics are reported
   unavailable.
3. Find the GPU process pid:

   ```powershell
   (Get-CimInstance Win32_Process -Filter "name='chrome.exe'" |
     Where-Object { $_.CommandLine -like '*type=gpu-process*' -and $_.CommandLine -like '*<profile dir name>*' }).ProcessId
   ```

4. Sample it while the scenario runs (`_NT_SYMBOL_PATH` set). The exe must sit
   beside `dbghelp.dll` and `symsrv.dll`:

   ```powershell
   $env:SAMPLER_FOLDED = "tmp/perf_tools/folded.txt"; $env:SAMPLER_RIPS = "tmp/perf_tools/rips.txt"
   ./tmp/perf_tools/native_sampler.exe <pid> 8 500 > tmp/perf_tools/profile.txt
   ```

   Args: `pid seconds [interval_us] [tid]`. `SAMPLER_FOLDED` and `SAMPLER_RIPS`
   name the folded-stack and RIP outputs; each file is written only when set.
   Only one thread gets full stack walks (the others contribute RIP samples): by
   default the thread with the earliest creation time, or the thread given as `tid`.
   Ctrl+C stops sampling cleanly and still prints the report. The sampler loads
   symbols only after sampling, so the first symbolization can take minutes with
   a cold cache.
5. Analyse the folded stacks:

   ```powershell
   python ai_studio/runtime_automation/stack_fold.py callers tmp/perf_tools/folded.txt <fn>
   python ai_studio/runtime_automation/stack_fold.py callees tmp/perf_tools/folded.txt <fn>
   python ai_studio/runtime_automation/stack_fold.py paths   tmp/perf_tools/folded.txt <substring>
   ```

   `paths` gives the share of busy stacks that contain the substring, grouped by
   the 3 deepest named frames above it: it answers "who leads to this wait".

## Reading results

- `profile.txt` lists self and inclusive time for the main thread, with lines.
  Wait and timer stubs are excluded from "busy".
- A wait in `rx::Buffer11::BufferStorage::setData -> NativeStorage::map` means
  the game writes a buffer the GPU still reads. Use `paths` on `NativeStorage::map`
  to find the buffer upload, then double-buffer or shrink it.
- Time spent in driver or D3D DLL frames with no game-side cause points at the
  GPU itself being the limit: check the next section before blaming Chrome.

## Pitfalls

- Other GPU users distort the numbers. Check
  `Get-Counter '\GPU Engine(*engtype_3D)\Utilization Percentage'` and close them.
- A synthetic benchmark may not reproduce waits caused by the GPU running behind
  the CPU; profile the real scenario.
- Sampling suspends the GPU thread, so absolute fps under the sampler is lower;
  compare shares, not frame rates.
- Do not load symbols inside the sampling loop: a multi-GB `chrome.dll.pdb` stalls it.
