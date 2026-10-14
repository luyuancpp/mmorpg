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

$snapDirForTest = Join-Path $runDir "prom-snapshots"

# 1a. login 快照(2026-10-13,G3 评审修复):两列后加的指标各造「有序列」与「缺序列」两种快照。
#     - entergame_total{result="pool_saturated"}:PreloadPool 满、入场链没启动的次数。它不是链的终态,
#       不计入 fail% 的分母;序列缺失(不导出该取值的旧 login)必须打印 n/a,不能折成 0。
#     - login_create_player_stage_seconds{stage="seed"}:建角锁内的新号种子那一步,只在
#       NewPlayerSeed.Enabled 打开时有序列;关着时打印 n/a。
#     t18m = 新版 login、开着种子;t5m = 不导出 pool_saturated 的旧 login、关着种子。
#     数值都取小于 1000 的整数:Format-Number 用 N0,千分位分隔符随区域设置变。
$fakeLoginSeedOn = @"
# HELP entergame_total EnterGame async chain terminal-state count.
# TYPE entergame_total counter
entergame_total{result="success"} 900
entergame_total{result="preload_failed"} 60
entergame_total{result="apply_failed"} 30
entergame_total{result="lock_lost"} 10
entergame_total{result="pool_saturated"} 7
# TYPE login_create_player_stage_seconds histogram
login_create_player_stage_seconds_sum{stage="mint"} 0.1
login_create_player_stage_seconds_count{stage="mint"} 100
login_create_player_stage_seconds_sum{stage="name"} 0.2
login_create_player_stage_seconds_count{stage="name"} 100
login_create_player_stage_seconds_sum{stage="register"} 0.3
login_create_player_stage_seconds_count{stage="register"} 100
login_create_player_stage_seconds_sum{stage="seed"} 0.06
login_create_player_stage_seconds_count{stage="seed"} 100
login_create_player_stage_seconds_sum{stage="account_write"} 0.5
login_create_player_stage_seconds_count{stage="account_write"} 100
# TYPE login_create_player_name_orphan_total counter
login_create_player_name_orphan_total 0
"@
Set-Content -Path (Join-Path $snapDirForTest "t18m_login.txt") -Value $fakeLoginSeedOn -Encoding utf8
$fakeLoginSeedOff = @"
# TYPE entergame_total counter
entergame_total{result="success"} 500
# TYPE login_create_player_stage_seconds histogram
login_create_player_stage_seconds_sum{stage="mint"} 0.05
login_create_player_stage_seconds_count{stage="mint"} 50
login_create_player_stage_seconds_sum{stage="name"} 0.1
login_create_player_stage_seconds_count{stage="name"} 50
login_create_player_stage_seconds_sum{stage="register"} 0.15
login_create_player_stage_seconds_count{stage="register"} 50
login_create_player_stage_seconds_sum{stage="account_write"} 0.25
login_create_player_stage_seconds_count{stage="account_write"} 50
"@
Set-Content -Path (Join-Path $snapDirForTest "t5m_login.txt") -Value $fakeLoginSeedOff -Encoding utf8

# 1b. DB 分段快照(2026-10-12 回归):client_golang 输出时按 label 名排序,db_task_stage_seconds
#     的实际序列是 {op=..,stage=..}。旧脚本按 {stage=..,op=..} 精确查找,这张表永远是 "-"。
#     这里按真实的输出顺序造快照;db 快照只在存在 *_login.txt 时才被识别(多端点布局),
#     上面 1a 的 login 快照同时满足这一条(原先这里补的是一个空的 t18m_login.txt)。
$fakeDbSnapshot = @"
# HELP db_task_stage_seconds Per-task sub-stage latency.
# TYPE db_task_stage_seconds histogram
db_task_stage_seconds_sum{op="read",stage="op_total"} 0.2
db_task_stage_seconds_count{op="read",stage="op_total"} 50
db_task_stage_seconds_sum{op="read",stage="op_handler"} 0.15
db_task_stage_seconds_count{op="read",stage="op_handler"} 50
db_task_stage_seconds_sum{op="read",stage="cache_write"} 0.025
db_task_stage_seconds_count{op="read",stage="cache_write"} 50
db_task_stage_seconds_sum{op="read",stage="result_publish"} 0.035
db_task_stage_seconds_count{op="read",stage="result_publish"} 50
db_task_stage_seconds_sum{op="write",stage="op_total"} 1.25
db_task_stage_seconds_count{op="write",stage="op_total"} 100
db_task_stage_seconds_sum{op="write",stage="op_handler"} 0.9
db_task_stage_seconds_count{op="write",stage="op_handler"} 100
db_task_result_total{op="read",result="ok"} 50
db_task_result_total{op="write",result="ok"} 100
"@
Set-Content -Path (Join-Path $snapDirForTest "t18m_db.txt") -Value $fakeDbSnapshot -Encoding utf8

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

# DB 分段:按真实 label 顺序造的快照必须算出平均值,而不是 "-"。
# read: op_total 4.0 / handler 3.0 / cache_write 0.5 / publish 0.7;write: op_total 12.5 / handler 9.0(ms)。
$dbRow = @($outputLines | Where-Object { $_ -match '^\s*t18m_db\s' })
if ($dbRow.Count -ne 1) {
    Write-Host "FAIL: expected exactly one t18m_db row in the DB task stages section, got $($dbRow.Count)" -ForegroundColor Red
    $outputLines | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
    if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue }
    exit 1
}
foreach ($expected in @('4.0ms', '3.0ms', '0.5ms', '0.7ms', '12.5ms', '9.0ms')) {
    Assert-Contains -Haystack $dbRow -Needle $expected `
        -Reason "DB task stages row must show $expected — label order {op,stage} not matched?"
}

# login 三个段(EnterGame / Dataloader / CreatePlayer)的行都以同一个快照名开头,只按行首匹配分不清是哪一段,
# 所以先按段标题定位,再取该段里指定快照的那一行。段标题只做子串匹配,不锚行首。
function Get-SectionRow {
    param(
        [string[]]$Lines,
        [string]$SectionMarker,
        [string]$SnapshotName
    )
    $inSection = $false
    foreach ($l in $Lines) {
        if ($l.Contains('=== ')) {
            $inSection = $l.Contains($SectionMarker)
            continue
        }
        if ($inSection -and $l -match ('^\s*' + [regex]::Escape($SnapshotName) + '\s')) {
            return $l
        }
    }
    return $null
}

function Assert-SectionRow {
    param(
        [string[]]$Lines,
        [string]$SectionMarker,
        [string]$SnapshotName,
        [string]$Pattern,
        [string]$Reason
    )
    $row = Get-SectionRow -Lines $Lines -SectionMarker $SectionMarker -SnapshotName $SnapshotName
    if (-not $row -or $row -notmatch $Pattern) {
        Write-Host "FAIL: $SectionMarker row for $SnapshotName does not match $Pattern ($Reason)" -ForegroundColor Red
        Write-Host "  got: $row" -ForegroundColor Red
        Write-Host "── stress_summarize output ──" -ForegroundColor DarkGray
        $Lines | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
        if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue }
        exit 1
    }
    Write-Host "  ok: $SectionMarker / $SnapshotName" -ForegroundColor DarkGreen
}

function Assert-HeaderMatches {
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$Reason
    )
    if (-not ($Lines -match $Pattern)) {
        Write-Host "FAIL: no header line matches $Pattern ($Reason)" -ForegroundColor Red
        Write-Host "── stress_summarize output ──" -ForegroundColor DarkGray
        $Lines | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
        if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue }
        exit 1
    }
    Write-Host "  ok: header $Pattern" -ForegroundColor DarkGreen
}

# EnterGame 表:pool_saturated 是最后一列。原有各列的位置不变。
Assert-HeaderMatches -Lines $outputLines -Pattern 'apply avg\s+pool_saturated\s*$' `
    -Reason "EnterGame table must end with the pool_saturated column"
# t18m:success 900 / preload_failed 60 / apply_failed 30,另有 lock_lost 10 → fail% = 100/1000 = 10.0%。
# pool_saturated 的 7 不进分母:进了分母就不是 10.0%(100/1007 = 9.9%,算成失败则是 107/1007 = 10.6%)。
Assert-SectionRow -Lines $outputLines -SectionMarker 'EnterGame end-to-end' -SnapshotName 't18m_login' `
    -Pattern '^\s*t18m_login\s+900\s+60\s+30\s+10\.0%\s.*\s7\s*$' `
    -Reason "pool_saturated must print the counter value as the last column and stay out of fail%"
# t5m:不导出 pool_saturated 的 login → n/a,不能折成 0。
Assert-SectionRow -Lines $outputLines -SectionMarker 'EnterGame end-to-end' -SnapshotName 't5m_login' `
    -Pattern '^\s*t5m_login\s+500\s+0\s+0\s+0\.0%\s.*\sn/a\s*$' `
    -Reason "a login snapshot without the pool_saturated series must print n/a, not 0"

# CreatePlayer 表:seed 列在 register 与 account_write 之间。
Assert-HeaderMatches -Lines $outputLines -Pattern 'mint\s+name\s+register\s+seed\s+account_write\s+name_orphan\s*$' `
    -Reason "CreatePlayer stages table must list the five stages in lock order"
# t18m(开着种子):五个阶段均值依次 1.0 / 2.0 / 3.0 / 0.6 / 5.0 ms,孤儿 0。五个数互不相同,列串位即红。
Assert-SectionRow -Lines $outputLines -SectionMarker 'CreatePlayer stages' -SnapshotName 't18m_login' `
    -Pattern '^\s*t18m_login\s+1\.0ms\s+2\.0ms\s+3\.0ms\s+0\.6ms\s+5\.0ms\s+0\s*$' `
    -Reason "seed stage avg must sit between register and account_write"
# t5m(关着种子):seed 没有序列 → n/a;其余四列照常;孤儿序列缺失但有 stage 序列 → 0(既有规则)。
Assert-SectionRow -Lines $outputLines -SectionMarker 'CreatePlayer stages' -SnapshotName 't5m_login' `
    -Pattern '^\s*t5m_login\s+1\.0ms\s+2\.0ms\s+3\.0ms\s+n/a\s+5\.0ms\s+0\s*$' `
    -Reason "with NewPlayerSeed off the seed column must print n/a and the other columns must not shift"

# ── Cleanup ─────────────────────────────────────────────────────────────

if (-not $KeepArtifacts) {
    Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue
} else {
    Write-Host "`nArtifacts kept under $tmpRoot" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "PASS" -ForegroundColor Green
