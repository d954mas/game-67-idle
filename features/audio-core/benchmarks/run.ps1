[CmdletBinding()]
param(
    [ValidateRange(1, 50)]
    [int]$CompileRuns = 5,
    [string]$BaselineCommit = "0fb94303f",
    [string]$Compiler = "C:\Program Files\LLVM\bin\clang.exe",
    [switch]$RuntimeDevice,
    [string]$WebEvidencePath = "",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [switch]$Capture
    )
    Push-Location -LiteralPath $WorkingDirectory
    try {
        if ($Capture) {
            $output = & $Executable @Arguments 2>&1
        } else {
            & $Executable @Arguments
            $output = @()
        }
        if ($LASTEXITCODE -ne 0) {
            throw "$Executable failed with exit code $LASTEXITCODE"
        }
        return $output
    } finally {
        Pop-Location
    }
}

function Get-Median {
    param([double[]]$Values)
    $sorted = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2.0
}

function Get-PercentDelta {
    param([long]$Baseline, [long]$Current)
    if ($Baseline -eq 0) { return $null }
    return [Math]::Round((($Current - $Baseline) * 100.0) / $Baseline, 4)
}

function Get-DefineValue {
    param([string]$Path, [string]$Name)
    $match = Select-String -LiteralPath $Path -Pattern "^#define\s+$Name\s+(\d+)" | Select-Object -First 1
    if ($null -eq $match) { throw "Missing #define $Name in $Path" }
    return [int]$match.Matches[0].Groups[1].Value
}

function Get-ObjectSize {
    param([string]$LlvmSize, [string]$ObjectPath)
    $lines = @(& $LlvmSize $ObjectPath)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -lt 2) { throw "llvm-size failed for $ObjectPath" }
    $columns = @($lines[-1].Trim() -split "\s+")
    return [ordered]@{
        text_bytes = [long]$columns[0]
        data_bytes = [long]$columns[1]
        bss_bytes = [long]$columns[2]
    }
}

$benchmarkRoot = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $benchmarkRoot "..\..\..")).Path
$featureRoot = (Resolve-Path (Join-Path $benchmarkRoot "..")).Path
$engineRoot = (Resolve-Path (Join-Path $repoRoot "external\neotolis-engine")).Path
$engineGitPath = $engineRoot.Replace("\", "/")
$templateRoot = (Resolve-Path (Join-Path $repoRoot "templates\template")).Path

if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Compiler not found: $Compiler"
}
$llvmSize = Join-Path (Split-Path -Parent $Compiler) "llvm-size.exe"
if (-not (Test-Path -LiteralPath $llvmSize -PathType Leaf)) {
    throw "llvm-size not found next to compiler: $llvmSize"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $benchmarkRoot "results\latest"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$webEvidence = $null
if (-not [string]::IsNullOrWhiteSpace($WebEvidencePath)) {
    $WebEvidencePath = [IO.Path]::GetFullPath($WebEvidencePath)
    if (-not (Test-Path -LiteralPath $WebEvidencePath -PathType Leaf)) {
        throw "Web evidence not found: $WebEvidencePath"
    }
    $webEvidence = Get-Content -Raw -LiteralPath $WebEvidencePath | ConvertFrom-Json
    if ($webEvidence.schema -ne "audio_core.web_build_benchmark.v1") {
        throw "Unsupported web evidence schema: $($webEvidence.schema)"
    }
    if ($webEvidence.baseline_commit -ne $BaselineCommit) {
        throw "Web evidence baseline $($webEvidence.baseline_commit) does not match $BaselineCommit"
    }
    foreach ($field in @("baseline_build_ms", "current_build_ms", "baseline_js_bytes", "current_js_bytes", "baseline_wasm_bytes", "current_wasm_bytes")) {
        if ([double]$webEvidence.$field -le 0) { throw "Web evidence field must be positive: $field" }
    }
}

$runId = "audio-benchmark-{0}-{1}" -f $PID, [DateTime]::UtcNow.ToString("yyyyMMddHHmmss")
$workRoot = Join-Path ([IO.Path]::GetTempPath()) $runId
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd("\") + "\"
$resolvedWorkRoot = [IO.Path]::GetFullPath($workRoot)
if (-not $resolvedWorkRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing benchmark work directory outside the system temp root: $resolvedWorkRoot"
}
$baselineEnginePath = $null

try {
    $compilerVersion = @(& $Compiler --version)[0]
    $currentCommit = (@(Invoke-Checked -Executable "git" -Arguments @("rev-parse", "HEAD") -WorkingDirectory $repoRoot -Capture)[0]).ToString().Trim()
    $dirtyLines = @(Invoke-Checked -Executable "git" -Arguments @("status", "--short") -WorkingDirectory $repoRoot -Capture)

    $baselineEngineLine = (@(Invoke-Checked -Executable "git" -Arguments @("ls-tree", $BaselineCommit, "external/neotolis-engine") -WorkingDirectory $repoRoot -Capture)[0]).ToString()
    $baselineEngineCommit = ($baselineEngineLine -split "\s+")[2]
    $engineCommit = (@(Invoke-Checked -Executable "git" -Arguments @("-c", "safe.directory=$engineGitPath", "-C", $engineRoot, "rev-parse", "HEAD") -WorkingDirectory $repoRoot -Capture)[0]).ToString().Trim()
    $engineDirty = @(Invoke-Checked -Executable "git" -Arguments @("-c", "safe.directory=$engineGitPath", "-C", $engineRoot, "status", "--porcelain") -WorkingDirectory $repoRoot -Capture)
    if ($engineCommit -ne $baselineEngineCommit -or $engineDirty.Count -ne 0) {
        throw "Baseline comparison requires clean engine commit $baselineEngineCommit; found $engineCommit with $($engineDirty.Count) dirty lines"
    }

    $includeArguments = @(
        "-I$($featureRoot)\include",
        "-I$($featureRoot)\src",
        "-I$($featureRoot)\vendor\miniaudio",
        "-I$engineRoot\engine",
        "-I$engineRoot\shared\include"
    )
    $productionWarningArguments = @(
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wshadow",
        "-Wconversion",
        "-Wdouble-promotion",
        "-Wformat=2",
        "-Wundef",
        "-Wno-unused-parameter",
        "-Werror"
    )
    $translationUnits = @(
        [ordered]@{ name = "audio"; source = Join-Path $featureRoot "src\audio.c" },
        [ordered]@{ name = "audio_resource"; source = Join-Path $featureRoot "src\audio_resource.c" },
        [ordered]@{ name = "audio_backend_miniaudio"; source = Join-Path $featureRoot "src\audio_backend_miniaudio.c" },
        [ordered]@{ name = "audio_miniaudio_impl"; source = Join-Path $featureRoot "src\audio_miniaudio_impl.c" }
    )
    $objectRoot = Join-Path $workRoot "objects"
    New-Item -ItemType Directory -Force -Path $objectRoot | Out-Null
    $compileResults = @()
    $staticBssBytes = 0L
    foreach ($unit in $translationUnits) {
        $times = @()
        $lastObject = ""
        for ($run = 1; $run -le $CompileRuns; ++$run) {
            $lastObject = Join-Path $objectRoot ("{0}-{1}.obj" -f $unit.name, $run)
            $arguments = @("-std=c17", "-O3", "-DNDEBUG", "-DNT_INTROSPECT_ENABLED=0") +
                $productionWarningArguments + $includeArguments + @("-c", $unit.source, "-o", $lastObject)
            $stopwatch = [Diagnostics.Stopwatch]::StartNew()
            Invoke-Checked -Executable $Compiler -Arguments $arguments -WorkingDirectory $repoRoot
            $stopwatch.Stop()
            $times += [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 4)
        }
        $objectSize = Get-ObjectSize -LlvmSize $llvmSize -ObjectPath $lastObject
        $staticBssBytes += $objectSize.bss_bytes
        $relativeSource = $unit.source.Substring($repoRoot.Length).TrimStart("\").Replace("\", "/")
        $compileResults += [ordered]@{
            name = $unit.name
            source = $relativeSource
            runs_ms = $times
            minimum_ms = [Math]::Round(($times | Measure-Object -Minimum).Minimum, 4)
            median_ms = [Math]::Round((Get-Median -Values $times), 4)
            object = $objectSize
        }
    }

    $baselineZip = Join-Path $workRoot "baseline.zip"
    $baselineRoot = Join-Path $workRoot "baseline"
    Invoke-Checked -Executable "git" -Arguments @("archive", "--format=zip", "--output=$baselineZip", $BaselineCommit) -WorkingDirectory $repoRoot
    Expand-Archive -LiteralPath $baselineZip -DestinationPath $baselineRoot
    $baselineEnginePath = Join-Path $baselineRoot "external\neotolis-engine"
    if (Test-Path -LiteralPath $baselineEnginePath) {
        Remove-Item -LiteralPath $baselineEnginePath -Recurse -Force
    }
    New-Item -ItemType Junction -Path $baselineEnginePath -Target $engineRoot | Out-Null

    $builds = [ordered]@{}
    foreach ($side in @("baseline", "current")) {
        if ($side -eq "baseline") {
            $source = Join-Path $baselineRoot "templates\template"
        } else {
            $source = $templateRoot
        }
        $build = Join-Path $workRoot ("build-" + $side)
        $configureArguments = @(
            "-S", $source,
            "-B", $build,
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DGAME_DEVAPI_ENABLED=OFF",
            "-DCMAKE_C_COMPILER=$Compiler"
        )
        Invoke-Checked -Executable "cmake" -Arguments $configureArguments -WorkingDirectory $repoRoot -Capture | Out-Null
        $gameBuildStopwatch = [Diagnostics.Stopwatch]::StartNew()
        Invoke-Checked -Executable "cmake" -Arguments @("--build", $build, "--target", "game") -WorkingDirectory $repoRoot -Capture | Out-Null
        $gameBuildStopwatch.Stop()
        $gamePath = Join-Path $build "bin\game.exe"
        $packPath = Join-Path $build "pack\game.ntpack"
        $builds[$side] = [ordered]@{
            build_ms = [Math]::Round($gameBuildStopwatch.Elapsed.TotalMilliseconds, 4)
            game_bytes = (Get-Item -LiteralPath $gamePath).Length
            pack_bytes = (Get-Item -LiteralPath $packPath).Length
        }
    }

    $runtime = [ordered]@{
        status = "unverified"
        reason = "Run without -RuntimeDevice; no real output device was opened."
        device_available = $null
        played = $null
        init_ms = $null
        load_ms = $null
        unlock_ms = $null
        first_play_submit_ms = $null
    }
    if ($RuntimeDevice) {
        $runtimeBuild = Join-Path $workRoot "runtime"
        Invoke-Checked -Executable "cmake" -Arguments @(
            "-S", $benchmarkRoot,
            "-B", $runtimeBuild,
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_C_COMPILER=$Compiler",
            "-DNT_REPO_ROOT=$repoRoot"
        ) -WorkingDirectory $repoRoot -Capture | Out-Null
        Invoke-Checked -Executable "cmake" -Arguments @("--build", $runtimeBuild, "--target", "audio_benchmark_native") -WorkingDirectory $repoRoot -Capture | Out-Null
        $runtimeExe = Join-Path $runtimeBuild "audio_benchmark_native.exe"
        $wavPath = Join-Path $templateRoot "assets\audio\sfx\ui_click.wav"
        $runtimeJson = (@(Invoke-Checked -Executable $runtimeExe -Arguments @($wavPath, "--play") -WorkingDirectory $repoRoot -Capture)[-1]).ToString()
        $measuredRuntime = $runtimeJson | ConvertFrom-Json
        if ($measuredRuntime.device_available -and $measuredRuntime.played) {
            $runtime.status = "measured"
            $runtime.reason = "Windows real-device run; one 75 ms, gain=0.05 UI-click submission. first_play_submit_ms measures API submission, not acoustic onset."
        } elseif (-not $measuredRuntime.device_available) {
            $runtime.reason = "No real output device was available; null backend is rejected by production code."
        } else {
            $runtime.reason = "A real device initialized, but the first play submission failed."
        }
        foreach ($property in @("device_available", "played", "init_ms", "load_ms", "unlock_ms", "first_play_submit_ms")) {
            $runtime[$property] = $measuredRuntime.$property
        }
    }

    $clipCap = Get-DefineValue -Path (Join-Path $featureRoot "include\features\audio\audio.h") -Name "AUDIO_MAX_CLIPS"
    $voiceCap = Get-DefineValue -Path (Join-Path $featureRoot "include\features\audio\audio.h") -Name "AUDIO_MAX_VOICES"
    $nativeClipCap = Get-DefineValue -Path (Join-Path $featureRoot "src\audio_backend_miniaudio.c") -Name "AUDIO_NATIVE_CLIPS"
    $nativeVoiceCap = Get-DefineValue -Path (Join-Path $featureRoot "src\audio_backend_miniaudio.c") -Name "AUDIO_NATIVE_VOICES"
    $webSource = Get-Content -Raw -LiteralPath (Join-Path $featureRoot "web\audio_web.library.js")
    $webClipCap = [int]([regex]::Match($webSource, "CLIP_CAPACITY:\s*(\d+)").Groups[1].Value)
    $webVoiceCap = [int]([regex]::Match($webSource, "VOICE_CAPACITY:\s*(\d+)").Groups[1].Value)

    $baseline = $builds.baseline
    $current = $builds.current
    $compileMinimumTotal = [Math]::Round((@($compileResults | ForEach-Object { $_.minimum_ms }) | Measure-Object -Sum).Sum, 4)
    $compileMedianTotal = [Math]::Round((@($compileResults | ForEach-Object { $_.median_ms }) | Measure-Object -Sum).Sum, 4)
    $miniaudioImplementation = @($compileResults | Where-Object { $_.name -eq "audio_miniaudio_impl" })[0]
    $miniaudioCompileShare = [Math]::Round($miniaudioImplementation.minimum_ms * 100.0 / $compileMinimumTotal, 2)
    $result = [ordered]@{
        schema_version = 1
        measured_at_utc = [DateTime]::UtcNow.ToString("o")
        measurement_scope = if ($null -eq $webEvidence) { "Windows native only" } else { "Windows native plus paired Emscripten Release build" }
        platform = [ordered]@{
            os = [Environment]::OSVersion.VersionString
            architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            processor = $env:PROCESSOR_IDENTIFIER
            compiler = $compilerVersion
            cmake = (@(& cmake --version)[0]).ToString()
            ninja = (@(& ninja --version)[0]).ToString()
        }
        source = [ordered]@{
            current_commit = $currentCommit
            working_tree_dirty = $dirtyLines.Count -ne 0
            working_tree_change_count = $dirtyLines.Count
            baseline_commit = $BaselineCommit
            engine_commit = $engineCommit
        }
        compile = [ordered]@{
            mode = "clean object output per run; Release -O3; production warning policy with -Werror; warm OS filesystem cache allowed"
            runs_per_tu = $CompileRuns
            minimum_sum_ms = $compileMinimumTotal
            median_sum_ms = $compileMedianTotal
            miniaudio_implementation_minimum_share_percent = $miniaudioCompileShare
            translation_units = $compileResults
        }
        game_binary = [ordered]@{
            baseline_build_ms = $baseline.build_ms
            current_build_ms = $current.build_ms
            build_delta_ms = [Math]::Round($current.build_ms - $baseline.build_ms, 4)
            build_delta_percent = Get-PercentDelta -Baseline ([long]($baseline.build_ms * 10000.0)) -Current ([long]($current.build_ms * 10000.0))
            baseline_bytes = $baseline.game_bytes
            current_bytes = $current.game_bytes
            delta_bytes = $current.game_bytes - $baseline.game_bytes
            delta_percent = Get-PercentDelta -Baseline $baseline.game_bytes -Current $current.game_bytes
        }
        pack = [ordered]@{
            baseline_bytes = $baseline.pack_bytes
            current_bytes = $current.pack_bytes
            delta_bytes = $current.pack_bytes - $baseline.pack_bytes
            delta_percent = Get-PercentDelta -Baseline $baseline.pack_bytes -Current $current.pack_bytes
            source_audio_bytes = (Get-Item -LiteralPath (Join-Path $templateRoot "assets\audio\sfx\ui_click.wav")).Length +
                (Get-Item -LiteralPath (Join-Path $templateRoot "assets\audio\music\demo_jingle.mp3")).Length
        }
        runtime_latency = $runtime
        caps = [ordered]@{
            public_clip_slots = $clipCap
            public_voice_slots = $voiceCap
            native_clip_slots = $nativeClipCap
            native_voice_slots = $nativeVoiceCap
            web_clip_slots_source_inspection_only = $webClipCap
            web_voice_slots_source_inspection_only = $webVoiceCap
            native_audio_object_static_bss_bytes = $staticBssBytes
            decoded_pcm_per_clip_byte_cap = 128MB
            decoded_pcm_total_byte_cap = 256MB
            decoded_pcm_note = "Native and WebAudio backends enforce identical 128 MiB per-clip and 256 MiB aggregate decoded-PCM caps; backend/device heap outside decoded PCM remains dynamic."
        }
        unmeasured = @()
    }
    if ($null -ne $webEvidence) {
        $result["web_build"] = [ordered]@{
            measured_at_utc = $webEvidence.measured_at_utc
            mode = $webEvidence.mode
            baseline_commit = $webEvidence.baseline_commit
            baseline_build_ms = [double]$webEvidence.baseline_build_ms
            current_build_ms = [double]$webEvidence.current_build_ms
            delta_ms = [Math]::Round([double]$webEvidence.current_build_ms - [double]$webEvidence.baseline_build_ms, 4)
            delta_percent = Get-PercentDelta -Baseline ([long]([double]$webEvidence.baseline_build_ms * 10000.0)) -Current ([long]([double]$webEvidence.current_build_ms * 10000.0))
            baseline_js_bytes = [long]$webEvidence.baseline_js_bytes
            current_js_bytes = [long]$webEvidence.current_js_bytes
            js_delta_bytes = [long]$webEvidence.current_js_bytes - [long]$webEvidence.baseline_js_bytes
            baseline_wasm_bytes = [long]$webEvidence.baseline_wasm_bytes
            current_wasm_bytes = [long]$webEvidence.current_wasm_bytes
            wasm_delta_bytes = [long]$webEvidence.current_wasm_bytes - [long]$webEvidence.baseline_wasm_bytes
        }
        $result.unmeasured = @("Linux compile/build/runtime", "Web runtime latency", "Acoustic output onset", "Peak backend/device heap bytes outside capped decoded PCM")
    } else {
        $result.unmeasured = @("Linux compile/build/runtime", "Web compile/build/runtime", "Acoustic output onset", "Peak backend/device heap bytes outside capped decoded PCM")
    }

    $jsonPath = Join-Path $OutputDirectory "result.json"
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($jsonPath, ($result | ConvertTo-Json -Depth 10), $utf8NoBom)

    $runtimeLine = if ($runtime.status -eq "measured") {
        "Measured on a real Windows device: init $($runtime.init_ms) ms; WAV load/decode $($runtime.load_ms) ms; unlock $($runtime.unlock_ms) ms; first-play API submission $($runtime.first_play_submit_ms) ms."
    } else {
        "Unverified: $($runtime.reason)"
    }
    $tuRows = @($compileResults | ForEach-Object {
        "| $($_.name) | $($_.minimum_ms) | $($_.median_ms) | $($_.object.bss_bytes) |"
    })
    $runtimeArgument = if ($RuntimeDevice) { " -RuntimeDevice" } else { "" }
    $relativeOutputDirectory = [IO.Path]::GetRelativePath($repoRoot, $OutputDirectory).Replace("\", "/")
    $reproduceCommand = "powershell -ExecutionPolicy Bypass -File features/audio-core/benchmarks/run.ps1 -CompileRuns $CompileRuns$runtimeArgument"
    if ($null -ne $webEvidence) {
        $relativeWebEvidence = [IO.Path]::GetRelativePath($repoRoot, $WebEvidencePath).Replace("\", "/")
        $reproduceCommand += " -WebEvidencePath $relativeWebEvidence"
    }
    $reproduceCommand += " -OutputDirectory $relativeOutputDirectory"
    $webReportLines = @()
    if ($null -ne $webEvidence) {
        $webReportLines = @(
            "",
            "## Clean Emscripten Release game build",
            "",
            "Both sides used the same warmed checkout-local Emscripten cache, compiler, engine checkout, generated asset header, and clean game target after configure. Baseline is $BaselineCommit.",
            "",
            "| Metric | Baseline | Current | Delta |",
            "| --- | ---: | ---: | ---: |",
            "| build wall time | $($result.web_build.baseline_build_ms) ms | $($result.web_build.current_build_ms) ms | $($result.web_build.delta_ms) ms ($($result.web_build.delta_percent)%) |",
            "| game.js | $($result.web_build.baseline_js_bytes) B | $($result.web_build.current_js_bytes) B | $($result.web_build.js_delta_bytes) B |",
            "| game.wasm | $($result.web_build.baseline_wasm_bytes) B | $($result.web_build.current_wasm_bytes) B | $($result.web_build.wasm_delta_bytes) B |"
        )
    }
    $reportLines = @(
        "# Windows native audio benchmark",
        "",
        "Measured at: $($result.measured_at_utc)",
        "Scope: **$($result.measurement_scope)**",
        "Current source: working tree at $currentCommit ($($dirtyLines.Count) changed/untracked paths)",
        "Baseline: $BaselineCommit",
        "Compiler: $compilerVersion",
        "",
        "## Results",
        "",
        "| Metric | Baseline | Current | Delta |",
        "| --- | ---: | ---: | ---: |",
        "| clean native game build | $($baseline.build_ms) ms | $($current.build_ms) ms | $($result.game_binary.build_delta_ms) ms ($($result.game_binary.build_delta_percent)%) |",
        "| game.exe | $($baseline.game_bytes) B | $($current.game_bytes) B | $($result.game_binary.delta_bytes) B ($($result.game_binary.delta_percent)%) |",
        "| game.ntpack | $($baseline.pack_bytes) B | $($current.pack_bytes) B | $($result.pack.delta_bytes) B ($($result.pack.delta_percent)%) |",
        "",
        "The packed WAV+MP3 sources total $($result.pack.source_audio_bytes) bytes. Pack delta includes pack framing/alignment, so it is measured independently rather than inferred from source sizes. Binary and pack deltas compare the complete dirty working tree with the baseline; they are not symbol-level attribution to audio alone."
    ) + $webReportLines + @(
        "",
        "## Clean native audio translation-unit compile",
        "",
        "Each Release TU was compiled $CompileRuns times to a fresh object with the production warning policy and -Werror. These are process wall-clock measurements with a warm OS filesystem cache; the minimum is the requested low-noise figure and the median shows normal local variation.",
        "",
        "| TU | Minimum ms | Median ms | Object BSS bytes |",
        "| --- | ---: | ---: | ---: |"
    ) + $tuRows + @(
        "",
        "Sum of per-TU minima: **$compileMinimumTotal ms**; sum of medians: **$compileMedianTotal ms**. The pinned Miniaudio implementation TU contributes **$miniaudioCompileShare%** of the minimum sum.",
        "",
        "Total fixed BSS across the four measured audio objects: **$staticBssBytes bytes**. This is object static storage, not total runtime memory.",
        "",
        "## Runtime latency",
        "",
        $runtimeLine,
        "",
        "first_play_submit_ms ends when the audio API accepts the voice. It is not a microphone-based audible-onset measurement.",
        "",
        "## Fixed caps and memory boundary",
        "",
        "- Public runtime: $clipCap clip slots and $voiceCap simultaneous voice slots.",
        "- Native backend: $nativeClipCap clip slots and $nativeVoiceCap voice slots.",
        "- Web source constants: $webClipCap clip slots and $webVoiceCap voice slots; inspected only, not measured in a browser.",
        "- Fixed native audio-object BSS measured from Release COFF objects: $staticBssBytes bytes.",
        "- Decoded PCM is capped at 128 MiB per clip and 256 MiB aggregate on both native and web; Miniaudio device/backend heap outside decoded PCM remains dynamic.",
        "",
        "## Not measured",
        "",
        ($result.unmeasured -join "; ") + ".",
        "",
        "Raw evidence: result.json. Reproduce with:",
        "",
        "~~~powershell",
        $reproduceCommand,
        "~~~"
    )
    $report = $reportLines -join "`n"
    $reportPath = Join-Path $OutputDirectory "REPORT.md"
    [System.IO.File]::WriteAllText($reportPath, $report, $utf8NoBom)
    Write-Host "Benchmark report: $reportPath"
    Write-Host "Raw result:      $jsonPath"
} finally {
    if ($null -ne $baselineEnginePath -and (Test-Path -LiteralPath $baselineEnginePath)) {
        # Remove the junction itself before recursively deleting the temp tree;
        # never let cleanup traverse into the real engine checkout.
        [System.IO.Directory]::Delete($baselineEnginePath)
    }
    if (Test-Path -LiteralPath $workRoot) {
        Remove-Item -LiteralPath $workRoot -Recurse -Force
    }
}
