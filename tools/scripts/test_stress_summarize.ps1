<#
.SYNOPSIS
    Smoke test for tools/scripts/stress_summarize.ps1 — focuses on the
    new section 2bb (Dirty-save skip rate) added 2026-06-03 but also
    exercises the legacy login-stats parser so a regression there can't
    sneak through unnoticed.

.DESCRIPTION
    The real stress_summarize parses three sources:
      * RunDir/robot.stderr           → "[stats Xm0s]" lines
      * RunDir/prom-snapshots/*.txt   → SceneManager + DB Prom snapshots
      * run/logs/cpp_nodes/scene_*.log → "[DirtySave]" lines (new)

    This script fabricates minimal versions of all three under a temp
    directory, runs stress_summarize.ps1 against them, and asserts the
    section 2bb table contains the correct numbers parsed from the
    fake [DirtySave] lines.

    The Kafka block of stress_summarize is skipped by passing
    -KafkaContainer "" — we don't have a kafka container in the test
    sandbox and the flag explicitly silences that section.

.NOTES
    Test failures print a red FAIL line and exit 1 so this can wire
    into CI later. Success exits 0 with a green PASS line.
#>
param(
    [switch]$KeepArtifacts # leave the temp dir intact for debugging
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Resolve-Path (Join-Path $ScriptDir "..\..")
$Summarize = Join-Path $ScriptDir "stress_summarize.ps1"

if (-not (Test-Path $Summarize)) {
    throw "stress_summarize.ps1 not found at $Summarize"
}

# ── Fabricate inputs ────────────────────────────────────────────────────

# 1. A throwaway RunDir with the bare minimum the parser expects.
$tmpRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("stress-summarize-test-" + [guid]::NewGuid().ToString("N"))
$runDir  = Join-Path $tmpRoot "run"
$null = New-Item -ItemType Directory -Force -Path $runDir
$null = New-Item -ItemType Directory -Force -Path (Join-Path $runDir "prom-snapshots")

# robot.stderr with one [stats Xm0s] line so the legacy parser has
# something to chew on.
$fakeStderr = @"
[stats 5m0s] conn=1000 login_ok=995 login_fail=0 login_stuck=0 enter_ok=995 enter_fail=0 msg_sent=12345(40/s) entergame_total=ok=995 fail=0 max_login=82ms recon_fb=0
"@
Set-Content -Path (Join-Path $runDir "robot.stderr") -Value $fakeStderr -Encoding utf8

# 2. The dirty-save scan walks run/logs/cpp_nodes under the *real*
#    repo root (not RunDir) — that's the production layout. We back up
#    any existing scene_*.log under that path, drop our fake in place,
#    and restore at the end.
$realCppLogDir = Join-Path $RepoRoot "run\logs\cpp_nodes"
$null = New-Item -ItemType Directory -Force -Path $realCppLogDir
$backupDir = Join-Path $tmpRoot "real_cpp_logs_backup"
$null = New-Item -ItemType Directory -Force -Path $backupDir

# Backup ALL existing scene*.log so the test directory only contains
# our fabricated fixture. The glob deliberately matches both the legacy
# single-instance "scene.stdout.log" and the multi-instance form
# "scene_1.log", because real and fake both need to be cleared.
$preExistingLogs = Get-ChildItem -Path $realCppLogDir -Filter 'scene*.log' -ErrorAction SilentlyContinue
foreach ($log in $preExistingLogs) {
    Move-Item -Path $log.FullName -Destination $backupDir
}

# Three monotonic [DirtySave] samples — what 90 s of scene runtime
# would look like with two 30 s snapshot ticks. The parser keeps the
# LAST sample per log file (highest, monotonic) so the assertion below
# is against the third row.
$fakeSceneLog = @"
2026-06-03 09:00:00 INFO  [SavePlayerToRedis] Player 1001 saved to Redis
2026-06-03 09:00:30 INFO  [DirtySave] total=120 skipped=80 skip_pct=66.6%
2026-06-03 09:01:00 INFO  [DirtySave] total=240 skipped=170 skip_pct=70.8%
2026-06-03 09:01:30 INFO  [DirtySave] total=360 skipped=270 skip_pct=75.0%
2026-06-03 09:02:00 INFO  [SavePlayerToRedis] Player 1042 saved to Redis
"@
$fakeLogPath = Join-Path $realCppLogDir "scene_1.log"
Set-Content -Path $fakeLogPath -Value $fakeSceneLog -Encoding utf8

# ── Run stress_summarize and capture output ─────────────────────────────

Write-Host "Running stress_summarize against fabricated inputs..." -ForegroundColor Cyan
$outputLines = & pwsh -NoProfile -File $Summarize -RunDir $runDir -KafkaContainer "" 2>&1 |
                  ForEach-Object { $_.ToString() }

# ── Restore real logs before any assertion can throw ────────────────────

Remove-Item $fakeLogPath -Force -ErrorAction SilentlyContinue
foreach ($log in (Get-ChildItem -Path $backupDir -ErrorAction SilentlyContinue)) {
    Move-Item -Path $log.FullName -Destination $realCppLogDir
}

# ── Assertions ──────────────────────────────────────────────────────────

function Assert-Contains {
    param(
        [string[]]$Haystack,
        [string]$Needle,
        [string]$Reason
    )
    if (-not ($Haystack -match [regex]::Escape($Needle))) {
        Write-Host "FAIL: expected to find $Needle ($Reason)" -ForegroundColor Red
        Write-Host "── stress_summarize output ──" -ForegroundColor DarkGray
        $Haystack | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
        if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue }
        exit 1
    }
    Write-Host "  ok: $Needle" -ForegroundColor DarkGreen
}

Write-Host ""
Write-Host "Assertions:" -ForegroundColor Cyan

# Section 2bb header must show up.
Assert-Contains -Haystack $outputLines `
    -Needle "Dirty-save skip rate" `
    -Reason "section 2bb header missing — was the parser introduced?"

# The LAST monotonic sample (total=360 skipped=270 → 75.0%) must appear.
# Format-Number renders 360 / 270 with comma thousands separators in
# fields padded to width 11 ("        360" / "        270"). Match the
# numbers loosely so a future formatter tweak doesn't break the test.
Assert-Contains -Haystack $outputLines `
    -Needle "scene_1.log" `
    -Reason "scene_1.log row missing in section 2bb"

Assert-Contains -Haystack $outputLines `
    -Needle "360" `
    -Reason "expected total=360 (latest sample) — parser may be reading the first instead of last sample"

Assert-Contains -Haystack $outputLines `
    -Needle "270" `
    -Reason "expected skipped=270 (latest sample)"

Assert-Contains -Haystack $outputLines `
    -Needle "75.0" `
    -Reason "expected skip_pct=75.0 (latest sample)"

# Earlier samples must NOT appear as the displayed row — that would
# mean the parser kept the FIRST match instead of the LAST.
$rendered = ($outputLines -join "`n")
if ($rendered -match "scene_1\.log\s+120\s+80") {
    Write-Host "FAIL: parser surfaced the FIRST sample (120/80) instead of the LAST (360/270)" -ForegroundColor Red
    if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue }
    exit 1
}
Write-Host "  ok: parser kept last (monotonic) sample" -ForegroundColor DarkGreen

# Legacy section sanity: the fake [stats 5m0s] line must show in
# section 1 so we know we didn't accidentally break the existing path.
Assert-Contains -Haystack $outputLines `
    -Needle "5m0s" `
    -Reason "robot stats parser regressed — fake [stats 5m0s] line did not produce a row"

# ── Cleanup ─────────────────────────────────────────────────────────────

if (-not $KeepArtifacts) {
    Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue
} else {
    Write-Host "`nArtifacts kept under $tmpRoot" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "PASS" -ForegroundColor Green
