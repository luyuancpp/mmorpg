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
# TYPE db_retry_queue_length gauge
db_retry_queue_length{queue="dead"} 1
db_retry_queue_length{queue="processing"} 0
db_retry_queue_length{queue="ready"} 2
"@
Set-Content -Path (Join-Path $snapDirForTest "t18m_db.txt") -Value $fakeDbSnapshot -Encoding utf8

# 1c. Round 20+ 的 db 快照(S4 测量打点 + S1 重试路由)。t18m_db 扮演旧版 db:没有这些新序列,
#     新段里除 snapshot 外必须全是 n/a(旧快照不能折成 0);它只有 L-04 的三个重试列表长度,没有 partitioned。
#     取值约定(列串位、求和口径写错都会变红):
#     - stage 与 step 的 count 都取 2,打印 sum 而不是均值会变红;各 step 均值互不相同;
#     - op="read" 的 step 序列不能混进 Redis steps 表(该表只读 op="write");
#     - db_redis_pool_wait_seconds_total 不能被当成 wait_total 加进 rpool_wait;只有 redis_client="main"
#       一份(placement 与 main 共用 client 时 go/db 只导出 main),rpool_wait 必须是 5 而不是翻倍;
#     - db_retry_route_total 跨 mode 求和,action="executed" 不能把 executed_nonowner 算进去。
$fakeDbSnapshotR20 = @"
# TYPE db_task_stage_seconds histogram
db_task_stage_seconds_bucket{op="write",stage="sql",le="0.005"} 2
db_task_stage_seconds_sum{op="write",stage="sql"} 0.004
db_task_stage_seconds_count{op="write",stage="sql"} 2
db_task_stage_seconds_sum{op="write",stage="task_total"} 0.04
db_task_stage_seconds_count{op="write",stage="task_total"} 2
db_task_stage_seconds_sum{op="write",stage="queue_wait"} 0.002
db_task_stage_seconds_count{op="write",stage="queue_wait"} 2
db_task_stage_seconds_sum{op="write",stage="kafka_e2e"} 0.1
db_task_stage_seconds_count{op="write",stage="kafka_e2e"} 2
db_task_stage_seconds_sum{op="read",stage="sql"} 0.006
db_task_stage_seconds_count{op="read",stage="sql"} 2
db_task_stage_seconds_sum{op="read",stage="task_total"} 0.012
db_task_stage_seconds_count{op="read",stage="task_total"} 2
# TYPE db_task_redis_step_seconds histogram
db_task_redis_step_seconds_sum{op="read",step="cache_set"} 0.009
db_task_redis_step_seconds_count{op="read",step="cache_set"} 1
db_task_redis_step_seconds_sum{op="write",step="cache_set"} 0.00096
db_task_redis_step_seconds_count{op="write",step="cache_set"} 2
db_task_redis_step_seconds_sum{op="write",step="cursor_get"} 0.00024
db_task_redis_step_seconds_count{op="write",step="cursor_get"} 2
db_task_redis_step_seconds_sum{op="write",step="cursor_set"} 0.00038
db_task_redis_step_seconds_count{op="write",step="cursor_set"} 2
db_task_redis_step_seconds_sum{op="write",step="epoch_get"} 0.00026
db_task_redis_step_seconds_count{op="write",step="epoch_get"} 2
db_task_redis_step_seconds_sum{op="write",step="epoch_mark"} 0.0005
db_task_redis_step_seconds_count{op="write",step="epoch_mark"} 2
db_task_redis_step_seconds_sum{op="write",step="expand_get"} 0.0003
db_task_redis_step_seconds_count{op="write",step="expand_get"} 2
db_task_redis_step_seconds_sum{op="write",step="lease_verify"} 0.00054
db_task_redis_step_seconds_count{op="write",step="lease_verify"} 2
db_task_redis_step_seconds_sum{op="write",step="lock_acquire"} 0.00042
db_task_redis_step_seconds_count{op="write",step="lock_acquire"} 2
db_task_redis_step_seconds_sum{op="write",step="lock_release"} 0.00022
db_task_redis_step_seconds_count{op="write",step="lock_release"} 2
db_task_redis_step_seconds_sum{op="write",step="placement_lookup"} 0.00068
db_task_redis_step_seconds_count{op="write",step="placement_lookup"} 2
db_task_redis_step_seconds_sum{op="write",step="placement_recheck"} 0.00072
db_task_redis_step_seconds_count{op="write",step="placement_recheck"} 2
# TYPE db_subshard_batch_size histogram
db_subshard_batch_size_bucket{le="1"} 2
db_subshard_batch_size_bucket{le="+Inf"} 10
db_subshard_batch_size_sum 30
db_subshard_batch_size_count 10
# TYPE db_ordering_guard_total counter
db_ordering_guard_total{outcome="apply"} 90
db_ordering_guard_total{outcome="apply_no_cursor"} 4
db_ordering_guard_total{outcome="error"} 0
db_ordering_guard_total{outcome="incomparable"} 0
db_ordering_guard_total{outcome="stale"} 6
db_ordering_lease_lost_total{reason="owner_changed"} 1
db_ordering_lease_lost_total{reason="renew_deadline"} 2
db_ordering_lease_lost_total{reason="renew_exhausted"} 0
db_ordering_lease_lost_total{reason="verify_error"} 0
db_ordering_lease_renew_retry_total 7
db_ordering_lock_release_total{outcome="error"} 0
db_ordering_lock_release_total{outcome="not_owner"} 1
db_ordering_lock_release_total{outcome="released"} 95
db_ordering_lock_release_total{outcome="skipped_sql_abandoned"} 0
db_redis_pool_hits_total{redis_client="main"} 900
db_redis_pool_wait_total{redis_client="main"} 5
db_redis_pool_wait_seconds_total{redis_client="main"} 0.5
db_redis_pool_timeouts_total{redis_client="main"} 0
db_redis_pool_conns{redis_client="main"} 12
# TYPE db_retry_routing_mode gauge
db_retry_routing_mode{mode="owner"} 1
db_retry_routing_mode{mode="shared"} 0
db_retry_queue_length{queue="dead"} 0
db_retry_queue_length{queue="partitioned"} 7
db_retry_queue_length{queue="processing"} 1
db_retry_queue_length{queue="ready"} 3
db_retry_route_total{action="dead",mode="owner"} 9
db_retry_route_total{action="dead",mode="shared"} 0
db_retry_route_total{action="executed",mode="owner"} 40
db_retry_route_total{action="executed",mode="shared"} 2
db_retry_route_total{action="executed_nonowner",mode="owner"} 0
db_retry_route_total{action="executed_nonowner",mode="shared"} 5
db_retry_route_total{action="forwarded",mode="owner"} 11
db_retry_route_total{action="misrouted",mode="owner"} 8
db_retry_route_total{action="requeued_revoked",mode="owner"} 6
"@
Set-Content -Path (Join-Path $snapDirForTest "t20m_db.txt") -Value $fakeDbSnapshotR20 -Encoding utf8

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

# DB 各段的行同样都以 t18m_db / t20m_db 开头(2c / 2d / 2e / 2f 各一行),一律按段标题定位,
# 不能再用「全量输出里恰好一行 t18m_db」。
# 2c DB task stages:按真实 label 顺序造的快照必须算出平均值,而不是 "-"。
# read: op_total 4.0 / handler 3.0 / cache_write 0.5 / publish 0.7;write: op_total 12.5 / handler 9.0(ms)。
$dbStagesRow = Get-SectionRow -Lines $outputLines -SectionMarker 'DB task stages' -SnapshotName 't18m_db'
if (-not $dbStagesRow) {
    Write-Host "FAIL: no t18m_db row in the DB task stages section" -ForegroundColor Red
    $outputLines | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }
    if (-not $KeepArtifacts) { Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue }
    exit 1
}
foreach ($expected in @('4.0ms', '3.0ms', '0.5ms', '0.7ms', '12.5ms', '9.0ms')) {
    Assert-Contains -Haystack @($dbStagesRow) -Needle $expected `
        -Reason "DB task stages row must show $expected — label order {op,stage} not matched?"
}

# 2d DB write path:列顺序 w_sql w_task w_queue w_e2e | r_sql r_task | batch | no_cursor lease_lost rel_notown | rpool_wait rpool_tmo。
Assert-HeaderMatches -Lines $outputLines `
    -Pattern 'w_sql\s+w_task\s+w_queue\s+w_e2e\s+\|\s+r_sql\s+r_task\s+\|\s+batch\s+\|\s+no_cursor\s+lease_lost\s+rel_notown\s+\|\s+rpool_wait\s+rpool_tmo\s*$' `
    -Reason "DB write path table must keep its column order"
# t20m:stage 均值 2.0 / 20.0 / 1.0 / 50.0、读 3.0 / 6.0 ms;batch 30/10 = 3.0;no_cursor 4;lease_lost = 1 + 2;
# rel_notown 1;rpool_wait 只有 main 一份 = 5(不翻倍,也没把 wait_seconds_total 的 0.5 加进来);rpool_tmo 0。
Assert-SectionRow -Lines $outputLines -SectionMarker 'DB write path' -SnapshotName 't20m_db' `
    -Pattern '^\s*t20m_db\s+2\.0ms\s+20\.0ms\s+1\.0ms\s+50\.0ms\s+\|\s+3\.0ms\s+6\.0ms\s+\|\s+3\.0\s+\|\s+4\s+3\s+1\s+\|\s+5\s+0\s*$' `
    -Reason "new stage averages / batch / guard counters / redis pool sums must land in their own columns"
# t18m(旧版 db):没有任何新序列 → 除 snapshot 外全是 n/a。
Assert-SectionRow -Lines $outputLines -SectionMarker 'DB write path' -SnapshotName 't18m_db' `
    -Pattern '^\s*t18m_db\s+n/a\s+n/a\s+n/a\s+n/a\s+\|\s+n/a\s+n/a\s+\|\s+n/a\s+\|\s+n/a\s+n/a\s+n/a\s+\|\s+n/a\s+n/a\s*$' `
    -Reason "a pre-Round-20 db snapshot must print n/a, not 0"

# 2e DB Redis steps:11 个 step 按 go/db/internal/metrics/pipeline.go RedisSteps 的顺序,最后一列是均值之和。
Assert-HeaderMatches -Lines $outputLines `
    -Pattern 'lock_acq\s+cur_get\s+ep_get\s+plc_lkp\s+exp_get\s+plc_rck\s+verify\s+cache\s+cur_set\s+ep_mark\s+lock_rel\s+sum\s*$' `
    -Reason "DB Redis steps table must list the 11 steps in write-path order plus sum"
# t20m:0.21 0.12 0.13 0.34 0.15 0.36 0.27 0.48 0.19 0.25 0.11,合计 2.61(ms)。cache 列是 0.48 而不是混进读任务的 9ms。
Assert-SectionRow -Lines $outputLines -SectionMarker 'DB Redis steps' -SnapshotName 't20m_db' `
    -Pattern '^\s*t20m_db\s+0\.21ms\s+0\.12ms\s+0\.13ms\s+0\.34ms\s+0\.15ms\s+0\.36ms\s+0\.27ms\s+0\.48ms\s+0\.19ms\s+0\.25ms\s+0\.11ms\s+2\.61ms\s*$' `
    -Reason "each write-path Redis step avg must sit in its RedisSteps column and sum must add them up"
Assert-SectionRow -Lines $outputLines -SectionMarker 'DB Redis steps' -SnapshotName 't18m_db' `
    -Pattern '^\s*t18m_db(\s+n/a){12}\s*$' `
    -Reason "a pre-Round-20 db snapshot must print n/a for all 11 steps and the sum"

# 2f DB retry routing(S1 的 db_retry_* 列由 S4 测量批输出)。
Assert-HeaderMatches -Lines $outputLines `
    -Pattern 'mode\s+\|\s+q_ready\s+q_part\s+q_proc\s+q_dead\s+\|\s+exec\s+nonowner\s+fwd\s+revoked\s+misrouted\s+rt_dead\s*$' `
    -Reason "DB retry routing table must keep its column order"
# t20m:owner 模式;队列 3 / 7 / 1 / 0;executed 跨 mode 40 + 2 = 42,executed_nonowner 5 单独一列,其余 11 / 6 / 8 / 9。
Assert-SectionRow -Lines $outputLines -SectionMarker 'DB retry routing' -SnapshotName 't20m_db' `
    -Pattern '^\s*t20m_db\s+owner\s+\|\s+3\s+7\s+1\s+0\s+\|\s+42\s+5\s+11\s+6\s+8\s+9\s*$' `
    -Reason "routing mode, queue lengths and route actions must land in their own columns"
# t18m(L-04 版 db):有 ready / processing / dead,没有 partitioned、路由计数与模式 → 这些打印 n/a,已有的照常出数。
Assert-SectionRow -Lines $outputLines -SectionMarker 'DB retry routing' -SnapshotName 't18m_db' `
    -Pattern '^\s*t18m_db\s+n/a\s+\|\s+2\s+n/a\s+0\s+1\s+\|\s+n/a\s+n/a\s+n/a\s+n/a\s+n/a\s+n/a\s*$' `
    -Reason "an older db without retry routing must print n/a for the new series and real values for the old ones"

# ── Cleanup ─────────────────────────────────────────────────────────────

if (-not $KeepArtifacts) {
    Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue
} else {
    Write-Host "`nArtifacts kept under $tmpRoot" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "PASS" -ForegroundColor Green
