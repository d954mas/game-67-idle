param(
  [int]$Port = 8765,
  [switch]$Restart,
  [switch]$Open
)

$ErrorActionPreference = "Stop"

function Quote-Ps([string]$Value) {
  return "'" + $Value.Replace("'", "''") + "'"
}

function Test-StudioUrl([string]$Url) {
  try {
    $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 1
    return ($response.StatusCode -ge 200 -and $response.StatusCode -lt 500 -and $response.Content.Contains("AI Studio"))
  } catch {
    return $false
  }
}

function Open-Url([string]$Url) {
  $psi = [System.Diagnostics.ProcessStartInfo]::new()
  $psi.FileName = $Url
  $psi.UseShellExecute = $true
  [System.Diagnostics.Process]::Start($psi) | Out-Null
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$serverPath = Join-Path $scriptDir "server.mjs"
$stateDir = Join-Path $repoRoot ".tmp\ai_studio"
$pidFile = Join-Path $stateDir "studio_shell_$Port.pid"
$outLog = Join-Path $stateDir "studio_shell_$Port.out.log"
$errLog = Join-Path $stateDir "studio_shell_$Port.err.log"
$runner = Join-Path $stateDir "studio_shell_$Port.runner.ps1"
$url = "http://127.0.0.1:$Port/"

New-Item -ItemType Directory -Force -Path $stateDir | Out-Null

if ($Restart -and (Test-Path -LiteralPath $pidFile)) {
  $oldPid = [int](Get-Content -LiteralPath $pidFile -Raw)
  if ($oldPid -gt 0) {
    taskkill /PID $oldPid /T /F *> $null
  }
  Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue
}

if (Test-StudioUrl $url) {
  if ($Open) { Open-Url $url }
  Write-Host "AI Studio already running: $url"
  exit 0
}

$node = (Get-Command node.exe -ErrorAction Stop).Source
$runnerContent = @(
  '$ErrorActionPreference = "Stop"',
  "Set-Location -LiteralPath $(Quote-Ps $repoRoot)",
  "& $(Quote-Ps $node) $(Quote-Ps $serverPath) $Port >> $(Quote-Ps $outLog) 2>> $(Quote-Ps $errLog)"
) -join [Environment]::NewLine
Set-Content -LiteralPath $runner -Value $runnerContent -Encoding UTF8

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = (Join-Path $PSHOME "powershell.exe")
$psi.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$runner`""
$psi.WorkingDirectory = $repoRoot
$psi.UseShellExecute = $true
$psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
[System.Diagnostics.Process]::Start($psi) | Out-Null

$deadline = (Get-Date).AddSeconds(10)
while ((Get-Date) -lt $deadline) {
  if (Test-StudioUrl $url) {
    $recordedPid = if (Test-Path -LiteralPath $pidFile) { (Get-Content -LiteralPath $pidFile -Raw).Trim() } else { "unknown" }
    if ($Open) { Open-Url $url }
    Write-Host "AI Studio started: $url"
    Write-Host "pid: $recordedPid"
    Write-Host "logs: $outLog | $errLog"
    exit 0
  }
  Start-Sleep -Milliseconds 250
}

$stderrTail = if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog -Tail 40 | Out-String } else { "(missing)" }
$stdoutTail = if (Test-Path -LiteralPath $outLog) { Get-Content -LiteralPath $outLog -Tail 40 | Out-String } else { "(missing)" }

Write-Error @"
AI Studio failed to start at $url.
PID file: $pidFile
stdout: $outLog
stderr: $errLog

stderr tail:
$stderrTail

stdout tail:
$stdoutTail
"@
exit 1
