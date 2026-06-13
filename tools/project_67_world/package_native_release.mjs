import { createHash } from "node:crypto";
import { copyFileSync, cpSync, existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const releaseDir = join(root, "build", "game_seed", "native-release");
const sourceExe = join(releaseDir, "game_seed.exe");
const sourceAssets = join(releaseDir, "assets");
const sourcePack = join(sourceAssets, "world67_art.ntpack");
const sourceIcon = join(root, "assets", "runtime", "67-world", "67-world.ico");
const sourceAcceptanceKit = join(root, "gamedesign", "meme-evolution", "child_test_acceptance.md");
const sourceResultTemplate = join(root, "gamedesign", "meme-evolution", "child_test_result_template.md");
const sourceParentGuide = join(root, "gamedesign", "meme-evolution", "parent_observer_guide.md");
const packageDir = join(root, "build", "release", "67-world-pc", "67-world");
const packageRoot = join(root, "build", "release");
const packageZip = join(root, "build", "release", "67-world-pc", "67-world-pc.zip");
const packageExe = join(packageDir, "67-world.exe");
const packageAssets = join(packageDir, "assets");
const packagePack = join(packageAssets, "world67_art.ntpack");
const returnInstructionsText = [
  "67 World child-test return instructions",
  "",
  "After a manual child-test:",
  "1. Run START_HERE.bat.",
  "2. Choose [6] Create child-test report and fill the new markdown report.",
  "3. Optional: put chosen screenshots, short videos, or audio-note files in child_test_results\\evidence.",
  "4. Choose [7] Validate filled child-test report. It must say PASS.",
  "5. Choose [8] Export validated child-test results.",
  "",
  "Return this file to the developer/AI agent:",
  "- child_test_results_for_return.zip",
  "",
  "Do not return the whole game folder unless asked.",
  "The return zip contains the validated report, package metadata, this instruction file, and optional evidence files.",
  "",
].join("\r\n");

function fail(message) {
  console.error(`FAIL package native release: ${message}`);
  process.exit(1);
}

function requireFile(path, label) {
  if (!existsSync(path)) {
    fail(`${label} missing: ${path}`);
  }
  const stat = statSync(path);
  if (!stat.isFile() || stat.size <= 0) {
    fail(`${label} is empty or not a file: ${path}`);
  }
  return stat.size;
}

function ensureInside(child, parent) {
  const normalizedChild = resolve(child).toLowerCase();
  const normalizedParent = resolve(parent).toLowerCase();
  if (normalizedChild !== normalizedParent && !normalizedChild.startsWith(`${normalizedParent}\\`)) {
    fail(`refusing to clean outside package root: ${child}`);
  }
}

function sha256(path) {
  return createHash("sha256").update(readFileSync(path)).digest("hex");
}

function packageFile(path, relativePath) {
  const stat = statSync(path);
  return {
    path: relativePath,
    bytes: stat.size,
    sha256: sha256(path),
  };
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (let i = 0; i < buffer.length; i++) {
    crc ^= buffer[i];
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function dosDateTime(date = new Date()) {
  const year = Math.max(1980, date.getFullYear());
  const dosTime = (date.getHours() << 11) | (date.getMinutes() << 5) | Math.floor(date.getSeconds() / 2);
  const dosDate = ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate();
  return { dosDate, dosTime };
}

function listPackageFiles(dir, prefix = "") {
  const files = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const abs = join(dir, entry.name);
    const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isDirectory()) {
      files.push(...listPackageFiles(abs, rel));
    } else if (entry.isFile()) {
      files.push({ abs, rel: `67-world/${rel}` });
    }
  }
  return files.sort((a, b) => a.rel.localeCompare(b.rel));
}

function writeStoreZip(zipPath, entries) {
  const localParts = [];
  const centralParts = [];
  let offset = 0;
  const { dosDate, dosTime } = dosDateTime();

  for (const entry of entries) {
    const name = Buffer.from(entry.rel.replaceAll("\\", "/"), "utf8");
    const data = readFileSync(entry.abs);
    const crc = crc32(data);

    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(0x0800, 6);
    local.writeUInt16LE(0, 8);
    local.writeUInt16LE(dosTime, 10);
    local.writeUInt16LE(dosDate, 12);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(name.length, 26);
    local.writeUInt16LE(0, 28);
    localParts.push(local, name, data);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(0x0800, 8);
    central.writeUInt16LE(0, 10);
    central.writeUInt16LE(dosTime, 12);
    central.writeUInt16LE(dosDate, 14);
    central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt16LE(0, 30);
    central.writeUInt16LE(0, 32);
    central.writeUInt16LE(0, 34);
    central.writeUInt16LE(0, 36);
    central.writeUInt32LE(0, 38);
    central.writeUInt32LE(offset, 42);
    centralParts.push(central, name);

    offset += local.length + name.length + data.length;
  }

  const centralSize = centralParts.reduce((sum, part) => sum + part.length, 0);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralSize, 12);
  end.writeUInt32LE(offset, 16);
  end.writeUInt16LE(0, 20);

  writeFileSync(zipPath, Buffer.concat([...localParts, ...centralParts, end]));
}

const exeSize = requireFile(sourceExe, "native release executable");
const packSize = requireFile(sourcePack, "world67 art pack");
requireFile(sourceIcon, "67 World Windows icon");
requireFile(sourceAcceptanceKit, "child-test acceptance kit");
requireFile(sourceResultTemplate, "child-test result template");
requireFile(sourceParentGuide, "parent observer guide");

ensureInside(packageDir, packageRoot);
rmSync(packageDir, { recursive: true, force: true });
rmSync(packageZip, { force: true });
mkdirSync(packageAssets, { recursive: true });

copyFileSync(sourceExe, packageExe);
cpSync(sourceAssets, packageAssets, { recursive: true });

writeFileSync(
  join(packageDir, "VERIFY_PACKAGE.ps1"),
  [
    "$ErrorActionPreference = 'Stop'",
    "$root = Split-Path -Parent $MyInvocation.MyCommand.Path",
    "Set-Location -LiteralPath $root",
    "",
    "function Fail($message) {",
    "  Write-Host \"FAIL 67 World package self-check: $message\" -ForegroundColor Red",
    "  exit 1",
    "}",
    "",
    "$requiredFiles = @(",
    "  '67-world.exe',",
    "  'assets/world67_art.ntpack',",
    "  'START_HERE.bat',",
    "  'RUN_67_WORLD.bat',",
    "  'START_CHILD_TEST_FRESH.bat',",
    "  'CREATE_CHILD_TEST_REPORT.ps1',",
    "  'CREATE_CHILD_TEST_REPORT.bat',",
    "  'VALIDATE_CHILD_TEST_REPORT.ps1',",
    "  'VALIDATE_CHILD_TEST_REPORT.bat',",
    "  'EXPORT_CHILD_TEST_RESULTS.ps1',",
    "  'EXPORT_CHILD_TEST_RESULTS.bat',",
    "  'README.txt',",
    "  'RETURN_INSTRUCTIONS.txt',",
    "  'PARENT_OBSERVER_GUIDE.md',",
    "  'CHILD_TEST_ACCEPTANCE.md',",
    "  'CHILD_TEST_RESULT_TEMPLATE.md',",
    "  'VERIFY_PACKAGE.ps1',",
    "  'VERIFY_PACKAGE.bat',",
    "  'release_manifest.json',",
    "  'CHECKSUMS.txt'",
    ")",
    "",
    "foreach ($relative in $requiredFiles) {",
    "  $path = Join-Path $root $relative",
    "  if (!(Test-Path -LiteralPath $path -PathType Leaf)) { Fail \"missing required file: $relative\" }",
    "  if ((Get-Item -LiteralPath $path).Length -le 0) { Fail \"empty required file: $relative\" }",
    "}",
    "",
    "$manifest = Get-Content -LiteralPath (Join-Path $root 'release_manifest.json') -Raw | ConvertFrom-Json",
    "if ($manifest.name -ne '67 World') { Fail 'manifest name mismatch' }",
    "if ($manifest.platform -ne 'native-pc') { Fail 'manifest platform mismatch' }",
    "if ($manifest.package.executable -ne '67-world.exe') { Fail 'manifest executable mismatch' }",
    "if ($manifest.package.art_pack -ne 'assets/world67_art.ntpack') { Fail 'manifest art pack mismatch' }",
    "if ($manifest.package.start_here_launcher -ne 'START_HERE.bat') { Fail 'manifest start-here launcher mismatch' }",
    "if ($manifest.package.child_test_launcher -ne 'START_CHILD_TEST_FRESH.bat') { Fail 'manifest child-test launcher mismatch' }",
    "if ($manifest.package.parent_observer_guide -ne 'PARENT_OBSERVER_GUIDE.md') { Fail 'manifest parent observer guide mismatch' }",
    "if ($manifest.package.child_test_acceptance_kit -ne 'CHILD_TEST_ACCEPTANCE.md') { Fail 'manifest child-test kit mismatch' }",
    "if ($manifest.package.child_test_result_template -ne 'CHILD_TEST_RESULT_TEMPLATE.md') { Fail 'manifest child-test result template mismatch' }",
    "if ($manifest.package.child_test_report_script -ne 'CREATE_CHILD_TEST_REPORT.ps1') { Fail 'manifest child-test report script mismatch' }",
    "if ($manifest.package.child_test_report_launcher -ne 'CREATE_CHILD_TEST_REPORT.bat') { Fail 'manifest child-test report launcher mismatch' }",
    "if ($manifest.package.child_test_report_validator_script -ne 'VALIDATE_CHILD_TEST_REPORT.ps1') { Fail 'manifest child-test report validator script mismatch' }",
    "if ($manifest.package.child_test_report_validator_launcher -ne 'VALIDATE_CHILD_TEST_REPORT.bat') { Fail 'manifest child-test report validator launcher mismatch' }",
    "if ($manifest.package.child_test_results_export_script -ne 'EXPORT_CHILD_TEST_RESULTS.ps1') { Fail 'manifest child-test results export script mismatch' }",
    "if ($manifest.package.child_test_results_export_launcher -ne 'EXPORT_CHILD_TEST_RESULTS.bat') { Fail 'manifest child-test results export launcher mismatch' }",
    "if ($manifest.package.self_check_script -ne 'VERIFY_PACKAGE.ps1') { Fail 'manifest self-check script mismatch' }",
    "if ($manifest.package.self_check_launcher -ne 'VERIFY_PACKAGE.bat') { Fail 'manifest self-check launcher mismatch' }",
    "if ($manifest.validation.requires_child_test_acceptance -ne $true) { Fail 'manifest child-test requirement mismatch' }",
    "",
    "$manifestFiles = @{}",
    "foreach ($file in $manifest.package.files) { $manifestFiles[$file.path] = $true }",
    "foreach ($relative in $requiredFiles) {",
    "  if ($relative -eq 'release_manifest.json') { continue }",
    "  if ($relative -eq 'CHECKSUMS.txt') { continue }",
    "  if (!$manifestFiles.ContainsKey($relative)) { Fail \"manifest missing package file: $relative\" }",
    "}",
    "",
    "$checksumPath = Join-Path $root 'CHECKSUMS.txt'",
    "$expectedChecksumFiles = @{}",
    "foreach ($relative in $requiredFiles) {",
    "  if ($relative -eq 'CHECKSUMS.txt') { continue }",
    "  $expectedChecksumFiles[$relative] = $true",
    "}",
    "$seenChecksumFiles = @{}",
    "$checked = 0",
    "foreach ($line in Get-Content -LiteralPath $checksumPath) {",
    "  if ([string]::IsNullOrWhiteSpace($line)) { continue }",
    "  $parts = $line -split '\\s+', 2",
    "  if ($parts.Count -ne 2) { Fail \"bad checksum line: $line\" }",
    "  $expected = $parts[0]",
    "  $relative = $parts[1].Trim()",
    "  if (!$expectedChecksumFiles.ContainsKey($relative)) { Fail \"unexpected checksum entry: $relative\" }",
    "  if ($seenChecksumFiles.ContainsKey($relative)) { Fail \"duplicate checksum entry: $relative\" }",
    "  $seenChecksumFiles[$relative] = $true",
    "  $path = Join-Path $root $relative",
    "  if (!(Test-Path -LiteralPath $path -PathType Leaf)) { Fail \"checksum file missing: $relative\" }",
    "  $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()",
    "  if ($actual -ne $expected.ToLowerInvariant()) { Fail \"checksum mismatch: $relative\" }",
    "  $checked += 1",
    "}",
    "",
    "foreach ($relative in $expectedChecksumFiles.Keys) {",
    "  if (!$seenChecksumFiles.ContainsKey($relative)) { Fail \"missing checksum entry: $relative\" }",
    "}",
    "if ($checked -ne $expectedChecksumFiles.Count) { Fail \"checksum count mismatch: $checked of $($expectedChecksumFiles.Count)\" }",
    "Write-Host \"PASS 67 World package self-check\" -ForegroundColor Green",
    "Write-Host \"Checked files: $checked\"",
    "Write-Host \"Start here: run START_HERE.bat.\"",
    "Write-Host \"Before testing, read PARENT_OBSERVER_GUIDE.md.\"",
    "Write-Host \"For normal play, run RUN_67_WORLD.bat.\"",
    "Write-Host \"For child-test, run START_CHILD_TEST_FRESH.bat and use CHILD_TEST_ACCEPTANCE.md.\"",
    "Write-Host \"After child-test, run CREATE_CHILD_TEST_REPORT.bat, fill it, then run VALIDATE_CHILD_TEST_REPORT.bat.\"",
    "Write-Host \"After validation passes, run EXPORT_CHILD_TEST_RESULTS.bat and return child_test_results_for_return.zip.\"",
    "exit 0",
    "",
  ].join("\r\n"),
);

writeFileSync(
  join(packageDir, "VERIFY_PACKAGE.bat"),
  `@echo off\r\npushd "%~dp0"\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0VERIFY_PACKAGE.ps1"\r\nset RESULT=%ERRORLEVEL%\r\nif not "%RESULT%"=="0" pause\r\npopd\r\nexit /b %RESULT%\r\n`,
);

writeFileSync(
  join(packageDir, "CREATE_CHILD_TEST_REPORT.ps1"),
  [
    "$ErrorActionPreference = 'Stop'",
    "$root = Split-Path -Parent $MyInvocation.MyCommand.Path",
    "Set-Location -LiteralPath $root",
    "$template = Join-Path $root 'CHILD_TEST_RESULT_TEMPLATE.md'",
    "if (!(Test-Path -LiteralPath $template -PathType Leaf)) {",
    "  Write-Host 'FAIL child-test report template is missing.' -ForegroundColor Red",
    "  exit 1",
    "}",
    "$resultsDir = Join-Path $root 'child_test_results'",
    "New-Item -ItemType Directory -Force -Path $resultsDir | Out-Null",
    "$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'",
    "$out = Join-Path $resultsDir \"child_test_result_$stamp.md\"",
    "$content = Get-Content -LiteralPath $template -Raw",
    "$content = $content -replace 'Report created:', \"Report created: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))\"",
    "$content = $content -replace 'Package folder:', \"Package folder: $root\"",
    "Set-Content -LiteralPath $out -Value $content -Encoding UTF8",
    "Write-Host 'PASS child-test result report created' -ForegroundColor Green",
    "Write-Host $out",
    "exit 0",
    "",
  ].join("\r\n"),
);

writeFileSync(
  join(packageDir, "CREATE_CHILD_TEST_REPORT.bat"),
  `@echo off\r\npushd "%~dp0"\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CREATE_CHILD_TEST_REPORT.ps1"\r\nset RESULT=%ERRORLEVEL%\r\npause\r\npopd\r\nexit /b %RESULT%\r\n`,
);

writeFileSync(
  join(packageDir, "VALIDATE_CHILD_TEST_REPORT.ps1"),
  [
    "param([string]$ReportPath = '')",
    "$ErrorActionPreference = 'Stop'",
    "$root = Split-Path -Parent $MyInvocation.MyCommand.Path",
    "Set-Location -LiteralPath $root",
    "",
    "function Fail($messages) {",
    "  Write-Host 'FAIL child-test report validation' -ForegroundColor Red",
    "  foreach ($message in $messages) { Write-Host \"- $message\" }",
    "  exit 1",
    "}",
    "",
    "if ([string]::IsNullOrWhiteSpace($ReportPath)) {",
    "  $resultsDir = Join-Path $root 'child_test_results'",
    "  if (!(Test-Path -LiteralPath $resultsDir -PathType Container)) { Fail @('child_test_results folder is missing') }",
    "  $latest = Get-ChildItem -LiteralPath $resultsDir -Filter 'child_test_result_*.md' -File | Sort-Object LastWriteTime | Select-Object -Last 1",
    "  if ($null -eq $latest) { Fail @('no child_test_result_*.md report found') }",
    "  $ReportPath = $latest.FullName",
    "}",
    "",
    "$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)",
    "if (!(Test-Path -LiteralPath $ReportPath -PathType Leaf)) { Fail @(\"report file missing: $ReportPath\") }",
    "$text = Get-Content -LiteralPath $ReportPath -Raw",
    "$errors = New-Object System.Collections.Generic.List[string]",
    "",
    "function HasExactYes($label) {",
    "  $pattern = '(?m)^- ' + [regex]::Escape($label) + ':[ \\t]*yes[ \\t]*\\r?$'",
    "  return [regex]::IsMatch($text, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)",
    "}",
    "",
    "function FieldValue($label) {",
    "  $pattern = '(?m)^- ' + [regex]::Escape($label) + ':[ \\t]*([^\\r\\n]*?)[ \\t]*\\r?$'",
    "  $match = [regex]::Match($text, $pattern)",
    "  if (!$match.Success) { return '' }",
    "  return $match.Groups[1].Value.Trim()",
    "}",
    "",
    "function FieldValues($label) {",
    "  $pattern = '(?m)^- ' + [regex]::Escape($label) + ':[ \\t]*([^\\r\\n]*?)[ \\t]*\\r?$'",
    "  $matches = [regex]::Matches($text, $pattern)",
    "  $values = New-Object System.Collections.Generic.List[string]",
    "  foreach ($match in $matches) { $values.Add($match.Groups[1].Value.Trim()) }",
    "  return $values",
    "}",
    "",
    "function IsMeaningful($value, $minimumLength) {",
    "  if ([string]::IsNullOrWhiteSpace($value)) { return $false }",
    "  $trimmed = $value.Trim()",
    "  $lower = $trimmed.ToLowerInvariant()",
    "  if ($lower -in @('none', 'n/a', 'na', '-', 'todo', 'placeholder')) { return $false }",
    "  return $trimmed.Length -ge $minimumLength",
    "}",
    "",
    "function RequireExactYes($label) { if (!(HasExactYes $label)) { $errors.Add(\"expected exact yes: $label\") } }",
    "",
    "function RequireFilled($label) {",
    "  $value = FieldValue $label",
    "  if ([string]::IsNullOrWhiteSpace($value) -or $value -eq 'yes / no' -or $value -eq 'pass / fail / needs tuning') {",
    "    $errors.Add(\"missing filled value: $label\")",
    "  }",
    "}",
    "",
    "function RequireMinutes($label, $minimum) {",
    "  $value = FieldValue $label",
    "  $match = [regex]::Match($value, '\\d+')",
    "  if (!$match.Success) { $errors.Add(\"missing minute count: $label\"); return }",
    "  $minutes = [int]$match.Value",
    "  if ($minutes -lt $minimum) { $errors.Add(\"$label must be at least $minimum minutes\") }",
    "}",
    "",
    "function RequireMeaningfulRepeatedField($label, $minimumCount, $minimumLength) {",
    "  $pattern = '(?m)^- ' + [regex]::Escape($label) + ':[ \\t]*([^\\r\\n]*?)[ \\t]*\\r?$'",
    "  $values = [regex]::Matches($text, $pattern)",
    "  $meaningful = 0",
    "  foreach ($match in $values) { if (IsMeaningful $match.Groups[1].Value $minimumLength) { $meaningful += 1 } }",
    "  if ($meaningful -lt $minimumCount) {",
    "    $errors.Add(\"need $minimumCount meaningful entries for $label\")",
    "  }",
    "}",
    "",
    "function RequireMeaningfulLine($label, $minimumLength) {",
    "  $pattern = '(?m)^' + [regex]::Escape($label) + ':[ \\t]*([^\\r\\n]*?)[ \\t]*\\r?$'",
    "  $match = [regex]::Match($text, $pattern)",
    "  $value = if ($match.Success) { $match.Groups[1].Value.Trim() } else { '' }",
    "  if (!(IsMeaningful $value $minimumLength)) {",
    "    $errors.Add(\"missing meaningful line: $label\")",
    "  }",
    "}",
    "",
    "$requiredYes = @(",
    "  '`VERIFY_PACKAGE.bat` passed before the session',",
    "  '`START_CHILD_TEST_FRESH.bat` was used',",
    "  'Game started at first `TAP BOX` FTUE',",
    "  'Real audio output was enabled',",
    "  'Child found `TAP BOX`',",
    "  'Child understood matching pairs',",
    "  'Child completed first merge',",
    "  'Text was readable from normal distance',",
    "  'Audio feedback was audible',",
    "  'First-minute pass',",
    "  'Child kept spawning/merging without confusion',",
    "  'Child noticed new 67 variants',",
    "  'Child understood the upgrade tile when it showed `BUY`',",
    "  'Child recovered from a full board using `FREE SLOT`',",
    "  'Screen stayed readable and not overloaded',",
    "  'Five-minute pass',",
    "  'Child still understood the next goal near the end',",
    "  'One-hour pass',",
    "  'Spawn sound audible',",
    "  'Merge sound audible',",
    "  'Upgrade sound audible',",
    "  'Blocked/full-board sound audible',",
    "  'Free slot/recycle sound audible',",
    "  'Sounds were pleasant for children'",
    ")",
    "foreach ($label in $requiredYes) { RequireExactYes $label }",
    "$requiredFields = @('Observer', 'Child age', 'Device', 'Speaker/headphones', 'Session length in minutes', 'Minutes played', 'Highest 67 reached', 'Collection count')",
    "foreach ($label in $requiredFields) { RequireFilled $label }",
    "RequireMinutes 'Session length in minutes' 55",
    "RequireMinutes 'Minutes played' 55",
    "RequireMeaningfulRepeatedField 'Notes' 4 12",
    "RequireMeaningfulLine 'Observer summary' 20",
    "if (!([regex]::IsMatch($text, '(?m)^Overall result:[ \\t]*pass[ \\t]*\\r?$', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase))) {",
    "  $errors.Add('expected exact final line: Overall result: pass')",
    "}",
    "",
    "if ($errors.Count -gt 0) { Fail $errors }",
    "Write-Host 'PASS child-test report validation' -ForegroundColor Green",
    "Write-Host $ReportPath",
    "exit 0",
    "",
  ].join("\r\n"),
);

writeFileSync(
  join(packageDir, "VALIDATE_CHILD_TEST_REPORT.bat"),
  `@echo off\r\npushd "%~dp0"\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0VALIDATE_CHILD_TEST_REPORT.ps1" %*\r\nset RESULT=%ERRORLEVEL%\r\nif not "%RESULT%"=="0" pause\r\npopd\r\nexit /b %RESULT%\r\n`,
);

writeFileSync(
  join(packageDir, "EXPORT_CHILD_TEST_RESULTS.ps1"),
  [
    "param([string]$ReportPath = '')",
    "$ErrorActionPreference = 'Stop'",
    "$root = Split-Path -Parent $MyInvocation.MyCommand.Path",
    "Set-Location -LiteralPath $root",
    "$validator = Join-Path $root 'VALIDATE_CHILD_TEST_REPORT.ps1'",
    "if (!(Test-Path -LiteralPath $validator -PathType Leaf)) {",
    "  Write-Host 'FAIL child-test report validator is missing.' -ForegroundColor Red",
    "  exit 1",
    "}",
    "",
    "if ([string]::IsNullOrWhiteSpace($ReportPath)) {",
    "  $resultsDir = Join-Path $root 'child_test_results'",
    "  if (!(Test-Path -LiteralPath $resultsDir -PathType Container)) {",
    "    Write-Host 'FAIL child_test_results folder is missing.' -ForegroundColor Red",
    "    exit 1",
    "  }",
    "  $latest = Get-ChildItem -LiteralPath $resultsDir -Filter 'child_test_result_*.md' -File | Sort-Object LastWriteTime | Select-Object -Last 1",
    "  if ($null -eq $latest) {",
    "    Write-Host 'FAIL no child_test_result_*.md report found.' -ForegroundColor Red",
    "    exit 1",
    "  }",
    "  $ReportPath = $latest.FullName",
    "}",
    "",
    "$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)",
    "if (!(Test-Path -LiteralPath $ReportPath -PathType Leaf)) {",
    "  Write-Host \"FAIL report file missing: $ReportPath\" -ForegroundColor Red",
    "  exit 1",
    "}",
    "",
    "& powershell -NoProfile -ExecutionPolicy Bypass -File $validator $ReportPath",
    "if ($LASTEXITCODE -ne 0) {",
    "  Write-Host 'FAIL export blocked because report validation failed.' -ForegroundColor Red",
    "  exit $LASTEXITCODE",
    "}",
    "",
    "$bundle = Join-Path $root 'child_test_results_for_return.zip'",
    "$tmp = Join-Path $root 'child_test_export_tmp'",
    "$evidenceDir = Join-Path $root 'child_test_results\\evidence'",
    "if (Test-Path -LiteralPath $bundle) { Remove-Item -LiteralPath $bundle -Force }",
    "if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }",
    "New-Item -ItemType Directory -Force -Path $tmp | Out-Null",
    "try {",
    "  Copy-Item -LiteralPath $ReportPath -Destination (Join-Path $tmp (Split-Path -Leaf $ReportPath)) -Force",
    "  Copy-Item -LiteralPath (Join-Path $root 'release_manifest.json') -Destination (Join-Path $tmp 'release_manifest.json') -Force",
    "  Copy-Item -LiteralPath (Join-Path $root 'CHECKSUMS.txt') -Destination (Join-Path $tmp 'CHECKSUMS.txt') -Force",
    "  Copy-Item -LiteralPath (Join-Path $root 'CHILD_TEST_ACCEPTANCE.md') -Destination (Join-Path $tmp 'CHILD_TEST_ACCEPTANCE.md') -Force",
    "  Copy-Item -LiteralPath (Join-Path $root 'PARENT_OBSERVER_GUIDE.md') -Destination (Join-Path $tmp 'PARENT_OBSERVER_GUIDE.md') -Force",
    "  if (Test-Path -LiteralPath $evidenceDir -PathType Container) {",
    "    $evidenceOut = Join-Path $tmp 'evidence'",
    "    New-Item -ItemType Directory -Force -Path $evidenceOut | Out-Null",
    "    Copy-Item -Path (Join-Path $evidenceDir '*') -Destination $evidenceOut -Recurse -Force -ErrorAction SilentlyContinue",
    "  }",
    "  Copy-Item -LiteralPath (Join-Path $root 'RETURN_INSTRUCTIONS.txt') -Destination (Join-Path $tmp 'RETURN_INSTRUCTIONS.txt') -Force",
    "  Compress-Archive -Path (Join-Path $tmp '*') -DestinationPath $bundle -Force",
    "} finally {",
    "  if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }",
    "}",
    "",
    "if (!(Test-Path -LiteralPath $bundle -PathType Leaf) -or (Get-Item -LiteralPath $bundle).Length -le 0) {",
    "  Write-Host 'FAIL child-test result export was not created.' -ForegroundColor Red",
    "  exit 1",
    "}",
    "Write-Host 'PASS child-test results export created' -ForegroundColor Green",
    "Write-Host $bundle",
    "exit 0",
    "",
  ].join("\r\n"),
);

writeFileSync(
  join(packageDir, "EXPORT_CHILD_TEST_RESULTS.bat"),
  `@echo off\r\npushd "%~dp0"\r\npowershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0EXPORT_CHILD_TEST_RESULTS.ps1" %*\r\nset RESULT=%ERRORLEVEL%\r\npause\r\npopd\r\nexit /b %RESULT%\r\n`,
);

writeFileSync(
  join(packageDir, "RUN_67_WORLD.bat"),
  `@echo off\r\npushd "%~dp0"\r\nstart "" "%~dp067-world.exe"\r\npopd\r\n`,
);

writeFileSync(
  join(packageDir, "START_CHILD_TEST_FRESH.bat"),
  `@echo off\r\npushd "%~dp0"\r\nstart "" "%~dp067-world.exe" --fresh-state --disable-autosave\r\npopd\r\n`,
);

writeFileSync(
  join(packageDir, "START_HERE.bat"),
  [
    "@echo off",
    "setlocal",
    "pushd \"%~dp0\"",
    "echo 67 World",
    "echo.",
    "echo Recommended child-test order:",
    "echo   [1] Verify package",
    "echo   [2] Read PARENT_OBSERVER_GUIDE.md",
    "echo   [3] Read CHILD_TEST_ACCEPTANCE.md",
    "echo   [4] Start fresh child-test",
    "echo   [6] Create report after the session",
    "echo   [7] Validate the filled report",
    "echo   [8] Export validated child-test results zip",
    "echo.",
    "echo [1] Verify package",
    "echo [2] Read parent observer guide",
    "echo [3] Read child-test acceptance kit",
    "echo [4] Fresh child-test",
    "echo [5] Normal play",
    "echo [6] Create child-test report",
    "echo [7] Validate filled child-test report",
    "echo [8] Export validated child-test results",
    "echo [Q] Quit",
    "choice /C 12345678Q /N /M \"Choose: \"",
    "if errorlevel 9 goto done",
    "if errorlevel 8 call \"%~dp0EXPORT_CHILD_TEST_RESULTS.bat\" & goto done",
    "if errorlevel 7 call \"%~dp0VALIDATE_CHILD_TEST_REPORT.bat\" & goto done",
    "if errorlevel 6 call \"%~dp0CREATE_CHILD_TEST_REPORT.bat\" & goto done",
    "if errorlevel 5 call \"%~dp0RUN_67_WORLD.bat\" & goto done",
    "if errorlevel 4 call \"%~dp0START_CHILD_TEST_FRESH.bat\" & goto done",
    "if errorlevel 3 type \"%~dp0CHILD_TEST_ACCEPTANCE.md\" & pause & goto done",
    "if errorlevel 2 type \"%~dp0PARENT_OBSERVER_GUIDE.md\" & pause & goto done",
    "if errorlevel 1 call \"%~dp0VERIFY_PACKAGE.bat\" & goto done",
    ":done",
    "popd",
    "endlocal",
    "",
  ].join("\r\n"),
);

writeFileSync(
  join(packageDir, "README.txt"),
  [
    "67 World - native PC release package",
    "",
    "How to run",
    "1. Keep this whole 67-world folder together.",
    "2. Double-click START_HERE.bat for the guided menu. The menu can also",
    "   show the parent guide and acceptance kit directly.",
    "3. Or double-click VERIFY_PACKAGE.bat and confirm it says PASS.",
    "",
    "Guided child-test menu path",
    "- [1] Verify package.",
    "- [2] Read PARENT_OBSERVER_GUIDE.md.",
    "- [3] Read CHILD_TEST_ACCEPTANCE.md.",
    "- [4] Start fresh child-test.",
    "- [6] Create report after the session.",
    "- [7] Validate the filled report.",
    "- [8] Export validated child-test results zip.",
    "",
    "Direct launchers",
    "4. Before a child-test, read PARENT_OBSERVER_GUIDE.md.",
    "5. For normal play, double-click RUN_67_WORLD.bat.",
    "6. For a manual child-test, double-click START_CHILD_TEST_FRESH.bat.",
    "   This starts from FTUE and does not overwrite autosave.",
    "7. After the session, double-click CREATE_CHILD_TEST_REPORT.bat and fill",
    "   the new file in child_test_results.",
    "8. Optional: put chosen screenshots, short videos, or audio-note files in",
    "   child_test_results\\evidence before export.",
    "9. Double-click VALIDATE_CHILD_TEST_REPORT.bat. It must say PASS before",
    "   the report can count as release acceptance.",
    "10. Double-click EXPORT_CHILD_TEST_RESULTS.bat to create",
    "   child_test_results_for_return.zip after validation passes.",
    "11. If Windows asks, allow the app to run.",
    "",
    "Controls",
    "- Click TAP BOX to spawn a 67.",
    "- Click matching 67s to merge into a new 67.",
    "- Use the SPEED/BOX upgrade tile when it shows BUY.",
    "- If the board is full, click FREE SLOT.",
    "",
    "Child-test checklist",
    "- First minute: child understands TAP BOX and matching pairs.",
    "- Release acceptance: one-hour section records at least 55 minutes played.",
    "- Audio: spawn, merge, upgrade, blocked, and FREE SLOT feedback are audible.",
    "- Stop if text is hard to read, the child cannot find the next action, or the board feels stuck.",
    "",
    "Troubleshooting",
    "- Keep the assets folder next to 67-world.exe.",
    "- If the game opens without art, re-extract the full zip before running.",
    "",
  ].join("\r\n"),
);
writeFileSync(join(packageDir, "RETURN_INSTRUCTIONS.txt"), returnInstructionsText);

copyFileSync(sourceAcceptanceKit, join(packageDir, "CHILD_TEST_ACCEPTANCE.md"));
copyFileSync(sourceResultTemplate, join(packageDir, "CHILD_TEST_RESULT_TEMPLATE.md"));
copyFileSync(sourceParentGuide, join(packageDir, "PARENT_OBSERVER_GUIDE.md"));

const packageFiles = [
  packageFile(packageExe, "67-world.exe"),
  packageFile(packagePack, "assets/world67_art.ntpack"),
  packageFile(join(packageDir, "START_HERE.bat"), "START_HERE.bat"),
  packageFile(join(packageDir, "RUN_67_WORLD.bat"), "RUN_67_WORLD.bat"),
  packageFile(join(packageDir, "START_CHILD_TEST_FRESH.bat"), "START_CHILD_TEST_FRESH.bat"),
  packageFile(join(packageDir, "CREATE_CHILD_TEST_REPORT.ps1"), "CREATE_CHILD_TEST_REPORT.ps1"),
  packageFile(join(packageDir, "CREATE_CHILD_TEST_REPORT.bat"), "CREATE_CHILD_TEST_REPORT.bat"),
  packageFile(join(packageDir, "VALIDATE_CHILD_TEST_REPORT.ps1"), "VALIDATE_CHILD_TEST_REPORT.ps1"),
  packageFile(join(packageDir, "VALIDATE_CHILD_TEST_REPORT.bat"), "VALIDATE_CHILD_TEST_REPORT.bat"),
  packageFile(join(packageDir, "EXPORT_CHILD_TEST_RESULTS.ps1"), "EXPORT_CHILD_TEST_RESULTS.ps1"),
  packageFile(join(packageDir, "EXPORT_CHILD_TEST_RESULTS.bat"), "EXPORT_CHILD_TEST_RESULTS.bat"),
  packageFile(join(packageDir, "VERIFY_PACKAGE.ps1"), "VERIFY_PACKAGE.ps1"),
  packageFile(join(packageDir, "VERIFY_PACKAGE.bat"), "VERIFY_PACKAGE.bat"),
  packageFile(join(packageDir, "README.txt"), "README.txt"),
  packageFile(join(packageDir, "RETURN_INSTRUCTIONS.txt"), "RETURN_INSTRUCTIONS.txt"),
  packageFile(join(packageDir, "PARENT_OBSERVER_GUIDE.md"), "PARENT_OBSERVER_GUIDE.md"),
  packageFile(join(packageDir, "CHILD_TEST_ACCEPTANCE.md"), "CHILD_TEST_ACCEPTANCE.md"),
  packageFile(join(packageDir, "CHILD_TEST_RESULT_TEMPLATE.md"), "CHILD_TEST_RESULT_TEMPLATE.md"),
];

const manifest = {
  schema_version: 1,
  name: "67 World",
  platform: "native-pc",
  created_at: new Date().toISOString(),
  source: {
    executable: "build/game_seed/native-release/game_seed.exe",
    executable_bytes: exeSize,
    art_pack: "build/game_seed/native-release/assets/world67_art.ntpack",
    art_pack_bytes: packSize,
  },
  package: {
    directory: "build/release/67-world-pc/67-world",
    archive: "build/release/67-world-pc/67-world-pc.zip",
    executable: "67-world.exe",
    art_pack: "assets/world67_art.ntpack",
    start_here_launcher: "START_HERE.bat",
    launcher: "RUN_67_WORLD.bat",
    child_test_launcher: "START_CHILD_TEST_FRESH.bat",
    child_test_report_script: "CREATE_CHILD_TEST_REPORT.ps1",
    child_test_report_launcher: "CREATE_CHILD_TEST_REPORT.bat",
    child_test_report_validator_script: "VALIDATE_CHILD_TEST_REPORT.ps1",
    child_test_report_validator_launcher: "VALIDATE_CHILD_TEST_REPORT.bat",
    child_test_results_export_script: "EXPORT_CHILD_TEST_RESULTS.ps1",
    child_test_results_export_launcher: "EXPORT_CHILD_TEST_RESULTS.bat",
    self_check_script: "VERIFY_PACKAGE.ps1",
    self_check_launcher: "VERIFY_PACKAGE.bat",
    parent_observer_guide: "PARENT_OBSERVER_GUIDE.md",
    child_test_acceptance_kit: "CHILD_TEST_ACCEPTANCE.md",
    child_test_result_template: "CHILD_TEST_RESULT_TEMPLATE.md",
    files: packageFiles,
  },
  validation: {
    requires_child_test_acceptance: true,
    parent_observer_guide_source: "gamedesign/meme-evolution/parent_observer_guide.md",
    child_test_acceptance_kit_source: "gamedesign/meme-evolution/child_test_acceptance.md",
    latest_automated_readiness_evidence: "tasks/STATUS.md",
  },
  branding: {
    icon_source: "assets/runtime/67-world/67-world.ico",
    resource_file: "src/windows/67_world.rc",
    product_name: "67 World",
    file_description: "67 World native PC game",
    file_version: "1.0.0.0",
    product_version: "1.0.0.0",
  },
};
writeFileSync(join(packageDir, "release_manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);

const checksumFiles = [
  ...packageFiles,
  packageFile(join(packageDir, "release_manifest.json"), "release_manifest.json"),
];
writeFileSync(
  join(packageDir, "CHECKSUMS.txt"),
  checksumFiles.map((file) => `${file.sha256}  ${file.path}`).join("\r\n") + "\r\n",
);

writeStoreZip(packageZip, listPackageFiles(packageDir));

const packagedExeSize = requireFile(packageExe, "packaged executable");
const packagedPackSize = requireFile(packagePack, "packaged world67 art pack");
const packageZipSize = requireFile(packageZip, "packaged zip archive");
console.log("PASS native release package");
console.log(`package: ${packageDir}`);
console.log(`zip: ${packageZip}`);
console.log(`exe bytes: ${packagedExeSize}`);
console.log(`art pack bytes: ${packagedPackSize}`);
console.log(`zip bytes: ${packageZipSize}`);
