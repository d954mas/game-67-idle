[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BaselineBuildDirectory,
    [Parameter(Mandatory = $true)][string]$CurrentBuildDirectory,
    [string]$BaselineCommit = "0fb94303f",
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Read-CacheValue {
    param([string]$BuildDirectory, [string]$Name)
    $cache = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { throw "Missing configured build: $BuildDirectory" }
    $match = Select-String -LiteralPath $cache -Pattern "^${Name}(?::[^=]+)?=(.*)$" | Select-Object -First 1
    if ($null -eq $match) { throw "Missing $Name in $cache" }
    return $match.Matches[0].Groups[1].Value
}

function Measure-WebBuild {
    param([string]$BuildDirectory)
    & cmake --build $BuildDirectory --target clean | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "clean failed: $BuildDirectory" }
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    & cmake --build $BuildDirectory --target game | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "game build failed: $BuildDirectory" }
    $stopwatch.Stop()
    $js = Join-Path $BuildDirectory "bin\game.js"
    $wasm = Join-Path $BuildDirectory "bin\game.wasm"
    return [ordered]@{
        build_ms = [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 4)
        js_bytes = (Get-Item -LiteralPath $js).Length
        wasm_bytes = (Get-Item -LiteralPath $wasm).Length
    }
}

$baselineBuild = [IO.Path]::GetFullPath($BaselineBuildDirectory)
$currentBuild = [IO.Path]::GetFullPath($CurrentBuildDirectory)
foreach ($build in @($baselineBuild, $currentBuild)) {
    if ((Read-CacheValue $build "CMAKE_BUILD_TYPE") -ne "Release") { throw "Web benchmark requires Release: $build" }
}
$baselineCache = [IO.Path]::GetFullPath((Read-CacheValue $baselineBuild "GAME_EMSCRIPTEN_CACHE_DIR"))
$currentCache = [IO.Path]::GetFullPath((Read-CacheValue $currentBuild "GAME_EMSCRIPTEN_CACHE_DIR"))
if ($baselineCache -ne $currentCache) { throw "Both web builds must use the same GAME_EMSCRIPTEN_CACHE_DIR" }

# Warm the shared compiler cache before measuring either side, then clean and
# time both complete game targets under identical cache conditions.
& cmake --build $baselineBuild --target game | Out-Null
if ($LASTEXITCODE -ne 0) { throw "baseline cache warm-up failed" }
& cmake --build $currentBuild --target game | Out-Null
if ($LASTEXITCODE -ne 0) { throw "current cache warm-up failed" }
$baseline = Measure-WebBuild $baselineBuild
$current = Measure-WebBuild $currentBuild

$evidence = [ordered]@{
    schema = "audio_core.web_build_benchmark.v1"
    measured_at_utc = [DateTime]::UtcNow.ToString("o")
    mode = "clean Ninja game target after configure; Release; shared warmed checkout-local EM_CACHE; identical compiler and engine checkout"
    baseline_commit = $BaselineCommit
    baseline_build_ms = $baseline.build_ms
    current_build_ms = $current.build_ms
    baseline_js_bytes = $baseline.js_bytes
    current_js_bytes = $current.js_bytes
    baseline_wasm_bytes = $baseline.wasm_bytes
    current_wasm_bytes = $current.wasm_bytes
}
$output = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $output) | Out-Null
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($output, ($evidence | ConvertTo-Json -Depth 4), $utf8NoBom)
Write-Host "Web benchmark evidence: $output"
