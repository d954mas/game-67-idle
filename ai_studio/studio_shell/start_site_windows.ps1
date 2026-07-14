param(
  [int]$Port = 8765,
  [switch]$Restart,
  [switch]$Open
)

$ErrorActionPreference = "Stop"

if ($Port -lt 1 -or $Port -gt 65535) {
  Write-Error "Port must be between 1 and 65535."
  exit 2
}

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

function Read-RecordedPid([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  $value = 0
  if (-not [int]::TryParse((Get-Content -LiteralPath $Path -Raw).Trim(), [ref]$value) -or $value -le 0) {
    return $null
  }
  return $value
}

function Get-ProcessRecord([int]$ProcessId) {
  $process = $null
  try {
    if ($null -eq ("StudioProcessCommandLine" -as [type])) {
      Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class StudioProcessCommandLine {
  [StructLayout(LayoutKind.Sequential)]
  private struct UnicodeString {
    public ushort Length;
    public ushort MaximumLength;
    public IntPtr Buffer;
  }

  [DllImport("ntdll.dll")]
  private static extern int NtQueryInformationProcess(
    IntPtr processHandle,
    int processInformationClass,
    IntPtr processInformation,
    int processInformationLength,
    out int returnLength);

  public static string Read(IntPtr process) {
    const int ProcessCommandLineInformation = 60;
    if (process == IntPtr.Zero) return null;
    int required;
    NtQueryInformationProcess(process, ProcessCommandLineInformation, IntPtr.Zero, 0, out required);
    if (required <= 0) return null;
    IntPtr buffer = Marshal.AllocHGlobal(required);
    try {
      int status = NtQueryInformationProcess(process, ProcessCommandLineInformation, buffer, required, out required);
      if (status != 0) return null;
      UnicodeString value = (UnicodeString)Marshal.PtrToStructure(buffer, typeof(UnicodeString));
      return value.Buffer == IntPtr.Zero ? null : Marshal.PtrToStringUni(value.Buffer, value.Length / 2);
    } finally {
      Marshal.FreeHGlobal(buffer);
    }
  }
}
'@
    }
    $process = [System.Diagnostics.Process]::GetProcessById($ProcessId)
    $handle = $process.Handle
    return [pscustomobject]@{
      ProcessId = $ProcessId
      ExecutablePath = $process.Path
      CommandLine = [StudioProcessCommandLine]::Read($handle)
      Process = $process
      Handle = $handle
    }
  } catch {
    if ($null -ne $process) { $process.Dispose() }
    return $null
  }
}

function Dispose-ProcessRecord($ProcessRecord) {
  if ($null -ne $ProcessRecord -and $null -ne $ProcessRecord.Process) {
    $ProcessRecord.Process.Dispose()
  }
}

function Test-MatchingStudioProcess($ProcessRecord, [string]$ServerPath, [int]$ServerPort) {
  if ($null -eq $ProcessRecord) { return $false }
  $executableName = [System.IO.Path]::GetFileName([string]$ProcessRecord.ExecutablePath)
  if ($executableName -notmatch '^(?i:node(?:\.exe)?)$') { return $false }
  $commandLine = [string]$ProcessRecord.CommandLine
  if ([string]::IsNullOrWhiteSpace($commandLine)) { return $false }
  $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($ServerPath) + '"?\s+' + [regex]::Escape([string]$ServerPort) + '(?:\s|$)'
  return [regex]::IsMatch($commandLine, $pattern)
}

function Get-RecordedStudioProcess([string]$Path, [string]$ServerPath, [int]$ServerPort) {
  $recordedPid = Read-RecordedPid $Path
  if ($null -eq $recordedPid) { return $null }
  $record = Get-ProcessRecord $recordedPid
  if (Test-MatchingStudioProcess $record $ServerPath $ServerPort) { return $record }
  Dispose-ProcessRecord $record
  return $null
}

function Test-PortOpen([int]$ServerPort) {
  $client = [System.Net.Sockets.TcpClient]::new()
  try {
    $connect = $client.ConnectAsync("127.0.0.1", $ServerPort)
    return ($connect.Wait(500) -and $client.Connected)
  } catch {
    return $false
  } finally {
    $client.Dispose()
  }
}

function Remove-StateFile([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) { return }
  for ($attempt = 0; $attempt -lt 20; $attempt++) {
    try {
      Remove-Item -LiteralPath $Path -Force -ErrorAction Stop
      return
    } catch {
      if ($attempt -eq 19) { throw }
      Start-Sleep -Milliseconds 100
    }
  }
}

function Remove-OrphanRunnerFiles([string]$StateDir, [int]$ServerPort) {
  Remove-StateFile (Join-Path $StateDir "studio_shell_$ServerPort.runner.ps1")
  Get-ChildItem -LiteralPath $StateDir -Filter "studio_shell_$ServerPort.runner_*.ps1" -File -ErrorAction SilentlyContinue |
    ForEach-Object { Remove-StateFile $_.FullName }
}

function Stop-MatchingStudioProcess($ProcessRecord) {
  if ($null -eq $ProcessRecord) { return }
  try {
    $processId = [int]$ProcessRecord.ProcessId
    if ($ProcessRecord.Process.HasExited) { return }
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = "taskkill.exe"
    $psi.Arguments = "/PID $processId /T /F"
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $killer = [System.Diagnostics.Process]::Start($psi)
    $stdout = $killer.StandardOutput.ReadToEnd()
    $stderr = $killer.StandardError.ReadToEnd()
    $killer.WaitForExit()
    $exitCode = $killer.ExitCode
    $killer.Dispose()
    if ($exitCode -ne 0 -and -not $ProcessRecord.Process.HasExited) {
      throw "Failed to stop verified AI Studio process tree ${processId}: $($stderr.Trim()) $($stdout.Trim())"
    }
    if (-not $ProcessRecord.Process.WaitForExit(5000)) {
      throw "AI Studio process tree $processId did not stop."
    }
  } finally {
    Dispose-ProcessRecord $ProcessRecord
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
$stateDir = Join-Path $repoRoot "tmp\ai_studio"
$pidFile = Join-Path $stateDir "studio_shell_$Port.pid"
$outLog = Join-Path $stateDir "studio_shell_$Port.out.log"
$errLog = Join-Path $stateDir "studio_shell_$Port.err.log"
$url = "http://127.0.0.1:$Port/"

New-Item -ItemType Directory -Force -Path $stateDir | Out-Null

$recordedPid = Read-RecordedPid $pidFile
$studioProcess = Get-RecordedStudioProcess $pidFile $serverPath $Port
if ($null -ne $recordedPid -and $null -eq $studioProcess) {
  Remove-StateFile $pidFile
}

if ($Restart -and $null -ne $studioProcess) {
  Stop-MatchingStudioProcess $studioProcess
  Remove-StateFile $pidFile
  $studioProcess = $null
}

if ($null -ne $studioProcess) {
  $healthy = Test-StudioUrl $url
  if ($healthy -and -not $studioProcess.Process.HasExited) {
    Dispose-ProcessRecord $studioProcess
    if ($Open) { Open-Url $url }
    Write-Host "AI Studio already running: $url"
    exit 0
  }
  if ($studioProcess.Process.HasExited) {
    Dispose-ProcessRecord $studioProcess
    Remove-StateFile $pidFile
    $studioProcess = $null
  } else {
    Dispose-ProcessRecord $studioProcess
    Write-Error "A matching AI Studio process is running but is not healthy at $url. Retry with -Restart."
    exit 1
  }
}

Remove-OrphanRunnerFiles $stateDir $Port

if (Test-PortOpen $Port) {
  Remove-StateFile $pidFile
  Write-Error "Port $Port is occupied by another process; AI Studio was not started."
  exit 1
}

foreach ($path in @($pidFile, $outLog, $errLog)) {
  Remove-StateFile $path
}

$runnerId = [Guid]::NewGuid().ToString("N")
$runner = Join-Path $stateDir "studio_shell_$Port.runner_$runnerId.ps1"
$node = (Get-Command node.exe -ErrorAction Stop).Source
$runnerContent = @(
  '$ErrorActionPreference = "Stop"',
  'try {',
  "  Set-Location -LiteralPath $(Quote-Ps $repoRoot)",
  "  & $(Quote-Ps $node) $(Quote-Ps $serverPath) $Port >> $(Quote-Ps $outLog) 2>> $(Quote-Ps $errLog)",
  '} finally {',
  "  Remove-Item -LiteralPath $(Quote-Ps $runner) -Force -ErrorAction SilentlyContinue",
  '}'
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
    $startedProcess = Get-RecordedStudioProcess $pidFile $serverPath $Port
    if ($null -ne $startedProcess -and -not $startedProcess.Process.HasExited) {
      $startedPid = [int]$startedProcess.ProcessId
      Dispose-ProcessRecord $startedProcess
      if ($Open) { Open-Url $url }
      Write-Host "AI Studio started: $url"
      Write-Host "pid: $startedPid"
      Write-Host "logs: $outLog | $errLog"
      exit 0
    }
    Dispose-ProcessRecord $startedProcess
  }
  Start-Sleep -Milliseconds 250
}

$stderrTail = if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog -Tail 40 | Out-String } else { "(missing)" }
$stdoutTail = if (Test-Path -LiteralPath $outLog) { Get-Content -LiteralPath $outLog -Tail 40 | Out-String } else { "(missing)" }

$failedProcess = Get-RecordedStudioProcess $pidFile $serverPath $Port
if ($null -ne $failedProcess) {
  Stop-MatchingStudioProcess $failedProcess
}
Remove-StateFile $pidFile
Remove-StateFile $runner

Write-Error -ErrorAction Continue @"
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
