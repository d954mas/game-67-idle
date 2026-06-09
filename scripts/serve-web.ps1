param(
    [string]$Path = "build/game_67_idle/wasm-debug",
    [int]$Port = 8080
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$dir = Join-Path $root $Path

if (!(Test-Path -LiteralPath $dir)) {
    throw "Web output directory not found: $dir. Build the WASM preset first."
}

Write-Host "Serving $dir at http://localhost:$Port/"
py -3.12 -m http.server $Port -d $dir

