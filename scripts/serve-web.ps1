param(
    [string]$Path = "build/game_67_idle/wasm-qa",
    [int]$Port = 8080
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$dir = Join-Path $root $Path

if (!(Test-Path -LiteralPath $dir)) {
    throw "Web output directory not found: $dir. Build the WASM preset first."
}

if (!(Test-Path -LiteralPath (Join-Path $dir "index.html"))) {
    throw "Web index not found in $dir. Run: cmake --build --preset game-wasm-qa"
}

if (!(Test-Path -LiteralPath (Join-Path $dir "assets/game_67_idle.ntpack"))) {
    throw "Game pack not found in $dir/assets. Run: cmake --build --preset game-wasm-qa"
}

Write-Host "Serving $dir at http://localhost:$Port/"
$python = $null
foreach ($candidate in @(
    @("py", "-3.12"),
    @("python"),
    @("python3"),
    @("C:\Python312\python.exe")
)) {
    $command = Get-Command $candidate[0] -ErrorAction SilentlyContinue
    if ($command) {
        $candidateArgs = @()
        if ($candidate.Length -gt 1) {
            $candidateArgs = $candidate[1..($candidate.Length - 1)]
        }
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $candidate[0] @candidateArgs --version *> $null
            $candidateExitCode = $LASTEXITCODE
        } catch {
            $candidateExitCode = 1
        } finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        if ($candidateExitCode -ne 0) {
            continue
        }
        $python = $candidate
        break
    }
}

if (!$python) {
    throw "Python not found. Install Python or run: python -m http.server $Port -d $dir"
}

$pythonArgs = @()
if ($python.Length -gt 1) {
    $pythonArgs = $python[1..($python.Length - 1)]
}
& $python[0] @pythonArgs -m http.server $Port -d $dir
