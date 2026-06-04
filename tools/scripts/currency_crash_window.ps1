<#
.SYNOPSIS
    Currency-crash-window verification driver. Runs the 4 cases (A/B/C/D)
    described in docs/notes/currency-crash-window-verification.md and writes
    the results table to the same document's "执行结果" appendix.

.DESCRIPTION
    Each case is two robot legs separated by either kill -9 + restart (A/B/C)
    or a normal LeaveGame (D):

      Leg 1 (pre):   robot logs in → reads pre balance → GmAddCurrency 10000
                     → reads post balance → exits (no LeaveGame for A/B/C
                     so the variant lives only in scene memory; D uses the
                     normal logout path inside the robot scenario).
      <kill / wait>
      Leg 2 (post):  robot logs in → reads pre balance (= "what survived")
                     → exits.

    Per case we record:
      - leg1_pre, leg1_post, leg1_delta
      - wait_seconds (actual time between leg1 exit and kill)
      - leg2_pre  (== leg2_post since add_amount=0)
      - lost      = leg1_post - leg2_pre  (gold that disappeared due to kill)
      - expected  = "lost" or "preserved" depending on case
      - pass/fail

    This script does NOT launch gate, login, db, or other go services — those
    are assumed to already be running via tools/scripts/go_services.ps1 +
    tools/scripts/cpp_nodes.ps1. It DOES launch/restart the scene node since
    that's the unit under test.

.PARAMETER Cases
    Comma-separated list of cases to run: A, B, C, D. Default: all four.

.PARAMETER Account
    Account name to use for the robot. Default: robot_0001. Each case uses a
    different account so a failed case doesn't leak state into the next.

.PARAMETER NoReset
    By default the script wipes redis/mysql/kafka offsets before each case
    (per CLAUDE.md §9.6). Pass -NoReset to skip the wipe — only useful when
    iterating the script itself; never set for a real run.

.PARAMETER OutDir
    Where to write per-leg snapshot JSONs and the summary table.
    Default: robot/logs/currency-crash-window-<yyyyMMdd-HHmmss>

.PARAMETER ScenePort
    Scene metrics port to wait on after restart (default 9150). Used to know
    when the new scene process is actually ready to accept the post-leg robot.

.EXAMPLE
    pwsh tools/scripts/currency_crash_window.ps1
    Runs all four cases with default settings.

.EXAMPLE
    pwsh tools/scripts/currency_crash_window.ps1 -Cases C
    Run only case C (accelerated cross-period via SCENE_PLAYER_SAVE_INTERVAL_SECONDS=10).
#>
param(
    [string]$Cases = "A,B,C,D",
    [string]$Account = "robot_0001",
    [switch]$NoReset,
    [string]$OutDir,
    [int]$ScenePort = 9150
)

$ResetData = -not $NoReset

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Resolve-Path (Join-Path $ScriptDir "..\..")
$RobotDir  = Join-Path $RepoRoot "robot"
$RobotExe  = Join-Path $RobotDir "robot.exe"
$RobotCfg  = Join-Path $RobotDir "etc\robot.currency-crash.yaml"

if (-not $OutDir) {
    $ts     = (Get-Date).ToString("yyyyMMdd-HHmmss")
    $OutDir = Join-Path $RepoRoot "robot\logs\currency-crash-window-$ts"
}
$null = New-Item -ItemType Directory -Force -Path $OutDir

Write-Host "Out dir: $OutDir" -ForegroundColor Cyan

# ── Helpers ────────────────────���─────────────────────────────────────────

function Invoke-DataReset {
    Write-Host "[reset] flushing redis / mysql / kafka offsets..." -ForegroundColor Yellow

    # Per CLAUDE.md §9.6: every kill-test login round must wipe redis/mysql/etcd
    # before the next round. We trim the wipe set to what currency persistence
    # actually depends on so this script stays usable outside stress contexts.

    # Redis full flush — we want a clean PlayerAllData parent-key state.
    & redis-cli FLUSHALL | Out-Null

    # MySQL: truncate the player tables. Keep table definitions (DROP would
    # force a re-init dance). We touch only the tables this verification reads.
    $mysqlPwd = $env:MYSQL_PASSWORD
    if (-not $mysqlPwd) { $mysqlPwd = "root" }
    $sql = @'
TRUNCATE TABLE player_database;
TRUNCATE TABLE player_database_1;
TRUNCATE TABLE player_centre_database;
TRUNCATE TABLE user_accounts;
TRUNCATE TABLE account_share_database;
'@
    $sql | & mysql -uroot "-p$mysqlPwd" mmo 2>&1 | Out-Null

    # Kafka offset reset — reuse the project helper.
    $kafkaReset = Join-Path $RepoRoot "tools\scripts\kafka_offset_reset.ps1"
    if (Test-Path $kafkaReset) {
        & pwsh -NoProfile -File $kafkaReset 2>&1 | Out-Null
    }
}

function Stop-SceneProcess {
    # Kill ALL scene.exe processes (case A/B/C want the abrupt-death path).
    # cpp_nodes.ps1 also has Stop, but it does graceful cleanup of the pid
    # file — we want kill -9 semantics, not graceful, so we Stop-Process -Force.
    $procs = Get-Process scene -ErrorAction SilentlyContinue
    if (-not $procs) {
        Write-Host "[kill] no scene process running" -ForegroundColor DarkYellow
        return
    }
    foreach ($p in $procs) {
        Write-Host "[kill] scene.exe PID=$($p.Id)" -ForegroundColor Magenta
        Stop-Process -Id $p.Id -Force
    }
    # Clean up the pid file so cpp_nodes.ps1 -Command status doesn't lie.
    $pidFile = Join-Path $RepoRoot "run\pids\cpp_nodes.pid.json"
    if (Test-Path $pidFile) {
        Remove-Item $pidFile -Force -ErrorAction SilentlyContinue
    }
}

function Start-Scene {
    param([int]$SaveIntervalSec = 0)
    if ($SaveIntervalSec -gt 0) {
        $env:SCENE_PLAYER_SAVE_INTERVAL_SECONDS = "$SaveIntervalSec"
        Write-Host "[scene] starting with SCENE_PLAYER_SAVE_INTERVAL_SECONDS=$SaveIntervalSec" -ForegroundColor DarkCyan
    } else {
        # Clear so the next start uses the default 300s.
        Remove-Item env:SCENE_PLAYER_SAVE_INTERVAL_SECONDS -ErrorAction SilentlyContinue
        Write-Host "[scene] starting with default save interval (300s)" -ForegroundColor DarkCyan
    }
    $cppNodes = Join-Path $RepoRoot "tools\scripts\cpp_nodes.ps1"
    & pwsh -NoProfile -File $cppNodes -Command start -Nodes scene -SceneCount 1 | Out-Host

    # Wait for scene metrics port to come up so the next robot leg doesn't
    # race the bind. 30s budget is plenty on a dev box.
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $tcp = Test-NetConnection -ComputerName 127.0.0.1 -Port $ScenePort `
            -InformationLevel Quiet -WarningAction SilentlyContinue
        if ($tcp) { return }
        Start-Sleep -Milliseconds 500
    }
    throw "scene did not bind metrics port $ScenePort within 30s"
}

function Invoke-RobotLeg {
    param(
        [string]$Account,
        [int64]$AddAmount,
        [string]$OutputPath
    )

    if (-not (Test-Path $RobotExe)) {
        Write-Host "[robot] building robot.exe..." -ForegroundColor DarkGray
        Push-Location $RobotDir
        try { go build -o robot.exe . | Out-Host } finally { Pop-Location }
    }

    # Override the snapshot config via env so we can re-use one yaml for
    # both legs of every case. The robot reads `currency_crash:` from the
    # yaml; we patch the values per leg via a dedicated env-aware override
    # by writing a small per-leg yaml on disk.
    $legCfgPath = "$OutputPath.cfg.yaml"
    $cfgTxt = Get-Content $RobotCfg -Raw
    $cfgTxt = $cfgTxt -replace 'account: "robot_\d+"', ('account: "{0}"' -f $Account)
    $cfgTxt = $cfgTxt -replace 'add_amount: \d+',      ('add_amount: {0}' -f $AddAmount)
    # Force-LF the output path so embedded backslashes don't break yaml.
    $escapedPath = $OutputPath.Replace('\','/')
    $cfgTxt = $cfgTxt -replace 'output_path:.*',       ('output_path: "{0}"' -f $escapedPath)
    Set-Content -Path $legCfgPath -Value $cfgTxt -Encoding utf8

    Push-Location $RobotDir
    try {
        & $RobotExe -c $legCfgPath | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "robot leg exited with code $LASTEXITCODE (account=$Account add=$AddAmount)"
        }
    } finally { Pop-Location }

    if (-not (Test-Path $OutputPath)) {
        throw "robot leg did not write snapshot at $OutputPath"
    }
    return Get-Content $OutputPath -Raw | ConvertFrom-Json
}

function Invoke-CrashCase {
    param(
        [string]$Name,
        [string]$Account,
        [int]$WaitSeconds,
        [int]$SaveIntervalSec,
        [bool]$Kill,
        [string]$Expected   # "lost" or "preserved"
    )

    Write-Host "`n========== CASE $Name ($Account, wait=$WaitSeconds`s, kill=$Kill, expect $Expected) ==========" `
        -ForegroundColor Cyan

    if ($ResetData) { Invoke-DataReset }

    # Make sure scene is fresh with the right save-interval setting.
    Stop-SceneProcess
    Start-Sleep -Milliseconds 500
    Start-Scene -SaveIntervalSec $SaveIntervalSec

    $legPre  = Join-Path $OutDir ("case-$Name`_leg_pre.json")
    $legPost = Join-Path $OutDir ("case-$Name`_leg_post.json")

    # Leg 1: add 10000 gold, exit (without LeaveGame for A/B/C).
    $snap1 = Invoke-RobotLeg -Account $Account -AddAmount 10000 -OutputPath $legPre

    # Wait for the configured window to open the kill opportunity.
    if ($WaitSeconds -gt 0) {
        Write-Host "[wait] sleeping $WaitSeconds`s before kill..." -ForegroundColor DarkGray
        Start-Sleep -Seconds $WaitSeconds
    }

    if ($Kill) {
        Stop-SceneProcess
        Start-Sleep -Seconds 1
        Start-Scene -SaveIntervalSec $SaveIntervalSec
    }

    # Leg 2: read-only re-snapshot.
    $snap2 = Invoke-RobotLeg -Account $Account -AddAmount 0 -OutputPath $legPost

    $lost     = [int64]$snap1.post_balance - [int64]$snap2.pre_balance
    $passed   = $false
    if ($Expected -eq "lost") {
        # We expect the +10000 to NOT survive, so leg2_pre should equal leg1_pre
        # (i.e. nothing got persisted). Allow any positive lost up to the full
        # delta — partial save is still a "lost" outcome.
        $passed = $lost -gt 0
    } else {
        # "preserved": leg2_pre should equal leg1_post (= leg1_pre + 10000).
        $passed = ($lost -eq 0) -and ([int64]$snap2.pre_balance -eq [int64]$snap1.post_balance)
    }

    return [pscustomobject]@{
        Case        = $Name
        Account     = $Account
        Leg1Pre     = [int64]$snap1.pre_balance
        Leg1Post    = [int64]$snap1.post_balance
        WaitSeconds = $WaitSeconds
        Kill        = $Kill
        Leg2Pre     = [int64]$snap2.pre_balance
        Lost        = $lost
        Expected    = $Expected
        Passed      = $passed
        PlayerID    = [uint64]$snap1.player_id
    }
}

# ── Cases ────────────────────────────────────────────────────────────────

# A: kill 5s after add → expect loss (within default 300s save period).
# B: wait 350s with default 300s period → expect preserved (one save tick fired).
# C: same as B but with SCENE_PLAYER_SAVE_INTERVAL_SECONDS=10 + wait 15s.
# D: no kill, normal scenario in-process — separate path; reuse case A
#    accounts but Kill=$false so leg2 reads back the freshly-added gold.
$caseList = $Cases.Split(",") | ForEach-Object { $_.Trim().ToUpper() }
$results  = @()

foreach ($c in $caseList) {
    switch ($c) {
        "A" {
            $results += Invoke-CrashCase -Name "A" -Account "robot_0001" `
                -WaitSeconds 5 -SaveIntervalSec 0 -Kill $true -Expected "lost"
        }
        "B" {
            $results += Invoke-CrashCase -Name "B" -Account "robot_0002" `
                -WaitSeconds 350 -SaveIntervalSec 0 -Kill $true -Expected "preserved"
        }
        "C" {
            $results += Invoke-CrashCase -Name "C" -Account "robot_0003" `
                -WaitSeconds 15 -SaveIntervalSec 10 -Kill $true -Expected "preserved"
        }
        "D" {
            $results += Invoke-CrashCase -Name "D" -Account "robot_0004" `
                -WaitSeconds 1 -SaveIntervalSec 0 -Kill $false -Expected "preserved"
        }
        default {
            Write-Host "Unknown case: $c (expected A/B/C/D)" -ForegroundColor Red
        }
    }
}

# ── Summary ──────────────────────────────────────────────────────────────

Write-Host "`n========== SUMMARY ==========" -ForegroundColor Green
$results | Format-Table -AutoSize Case, Account, Leg1Pre, Leg1Post, WaitSeconds, Kill, Leg2Pre, Lost, Expected, Passed

$summaryPath = Join-Path $OutDir "summary.json"
$results | ConvertTo-Json -Depth 4 | Set-Content -Path $summaryPath -Encoding utf8
Write-Host "`nSummary JSON: $summaryPath" -ForegroundColor Cyan

# Markdown table fragment ready to paste into the verification doc.
$md = New-Object System.Text.StringBuilder
[void]$md.AppendLine("| Case | Account | Leg1 Pre | Leg1 Post | Wait (s) | Kill | Leg2 Pre | Lost | Expected | Pass |")
[void]$md.AppendLine("|------|---------|----------|-----------|----------|------|----------|------|----------|------|")
foreach ($r in $results) {
    $passMark = if ($r.Passed) { "✅" } else { "❌" }
    [void]$md.AppendLine("| $($r.Case) | $($r.Account) | $($r.Leg1Pre) | $($r.Leg1Post) | $($r.WaitSeconds) | $($r.Kill) | $($r.Leg2Pre) | $($r.Lost) | $($r.Expected) | $passMark |")
}
$mdPath = Join-Path $OutDir "summary.md"
Set-Content -Path $mdPath -Value $md.ToString() -Encoding utf8
Write-Host "Summary MD : $mdPath (paste into docs/notes/currency-crash-window-verification.md)" -ForegroundColor Cyan

# Exit non-zero on any failure so CI / babysitting scripts notice.
$failCount = ($results | Where-Object { -not $_.Passed }).Count
if ($failCount -gt 0) {
    Write-Host "`n$failCount case(s) failed." -ForegroundColor Red
    exit 1
}
Write-Host "`nAll cases passed." -ForegroundColor Green
