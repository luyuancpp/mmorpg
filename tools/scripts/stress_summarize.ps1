#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Summarise a stress-run into a single 2-D table for fast AI / human review.

.DESCRIPTION
    Walks a `robot/logs/stress-<name>-<ts>/` directory + the corresponding
    `prom-snapshots/` and produces the compact view we used in the
    2026-05-27/28 stress series:

      [robot stats per minute]
          time | conn | login_ok | enter_ok | enter_fail | recon_fb | max_login | msg/s

      [entergame_total snapshot at each prom-snapshot point]
          when | success | preload_failed | apply_failed | fail %
                 | preload{ok} avg | preload{fail} avg | apply avg | pool_saturated
          (pool_saturated = PreloadPool 满、入场链没启动的次数;不计入 fail %;缺序列打印 n/a)

      [dataloader per-stage avg at each prom-snapshot point]
          when | cache_check{hit} | sub_cache_check | kafka_send | callback_wait{ok} | callback_wait{fail}

      [CreatePlayer stages:每个快照点的建角分阶段均值 + 名字孤儿计数;缺指标打印 n/a,
       name_orphan 在「有 stage 序列但无孤儿序列」时打印 0(规则见该段注释)]
          when | mint | name | register | seed | account_write | name_orphan

      [DB write path(Round 20+,*_db 快照):db 新 stage 均值、子 shard 批大小、排序守卫 / 失租 / 释放计数、
       go-redis 连接池等待与超时;缺序列打印 n/a]
          when | w_sql w_task w_queue w_e2e | r_sql r_task | batch | no_cursor lease_lost rel_notown | rpool_wait rpool_tmo

      [DB Redis steps(Round 20+ / 围栏 v0.4,*_db 快照):12 个固定列只读写任务均值与合计;缺序列打印 n/a]
          when | lock_acq cur_get ep_get owner_ep plc_lkp exp_get plc_rck verify publish cache_nx advance lock_rel | sum

      [DB retry routing(*_db 快照):重试路由模式、四个重试列表长度、路由动作计数;缺序列打印 n/a]
          when | mode | q_ready q_part q_proc q_dead | exec nonowner fwd revoked misrouted rt_dead

      [optional: Kafka consumer lag (final)]
          partition | current | log_end | lag

    All numbers are derived, no raw histogram buckets — exactly what the
    AI needs to understand a run in ≤2 KB output instead of 30+ KB of
    prom dumps.

    Goal is "stop spending tokens parsing prom output by hand". After
    every stress run, call this once. The text it prints is the only
    artefact AI needs to read.

.PARAMETER RunDir
    The robot/logs/stress-* directory. Auto-discovers prom-snapshots/
    inside it.

.PARAMETER ProbePromUrl
    If passed, additionally fetches LIVE prometheus from this URL right
    now and treats it as a synthetic 'live' snapshot (named `t<duration>m_live`).
    Useful when the run is still going.

.PARAMETER KafkaContainer
    Docker container name (default 'kafka'). Set to '' to skip Kafka lag
    section.

.PARAMETER Zone
    本机启动 go/db 时用的 -Zone(dev_tools.ps1 dev-start-zones / go_services.ps1 -Zone N)。默认 0 = 单 zone 默认启动
    (start_game / dev-start / go-svc-start 不带 -Zone):topic 与组名用下面两个参数的缺省值。
    > 0 时,未显式传 -KafkaTopic / -KafkaConsumerGroup 的那一项按 zone 推导:topic 为 db_task_zone_<Zone>,
    组名为 db_rpc_consumer_group_z<Zone>(go_services.ps1 只在 -Zone > 0 时给 GroupID 追加 _z<Zone>)。
    显式传的值一律优先。

.PARAMETER KafkaTopic
    Topic to inspect for consumer lag. Default 'db_task_zone_1' (db.yaml ZoneId=1);
    derived as 'db_task_zone_<Zone>' when -Zone > 0 and this parameter is not passed.

.PARAMETER KafkaConsumerGroup
    Consumer group to inspect. Default 'db_rpc_consumer_group' (single-zone default start);
    derived as 'db_rpc_consumer_group_z<Zone>' when -Zone > 0 and this parameter is not passed.

.EXAMPLE
    # Standard post-run summary
    pwsh tools/scripts/stress_summarize.ps1 -RunDir robot/logs/stress-1zone-25k-mysqlpool-20260528-095929

.EXAMPLE
    # Mid-run probe + Kafka lag
    pwsh tools/scripts/stress_summarize.ps1 `
        -RunDir robot/logs/stress-1zone-25k-mysqlpool-20260528-095929 `
        -ProbePromUrl http://127.0.0.1:9101/metrics
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir,

    [string]$ProbePromUrl = "",

    [string]$KafkaContainer = "kafka",
    [string]$KafkaTopic = "db_task_zone_1",
    [string]$KafkaConsumerGroup = "db_rpc_consumer_group",
    # 见 .PARAMETER Zone:0 = 单 zone 默认启动(裸名组);> 0 时推导未显式传入的 topic / 组名。
    [int]$Zone = 0
)

$ErrorActionPreference = "Stop"

# 本机组名规则以 go_services.ps1 为准:只有 -Zone N(N > 0)启动时 GroupID 才追加 _zN,单 zone 默认启动是裸名。
# 只推导调用方没有显式传的那一项,显式值优先。
if ($Zone -gt 0) {
    if (-not $PSBoundParameters.ContainsKey('KafkaTopic')) { $KafkaTopic = "db_task_zone_$Zone" }
    if (-not $PSBoundParameters.ContainsKey('KafkaConsumerGroup')) { $KafkaConsumerGroup = "db_rpc_consumer_group_z$Zone" }
}

# ----- helpers ---------------------------------------------------------

function Format-Number {
    param([double]$N, [int]$Width = 6)
    if ([double]::IsNaN($N) -or [double]::IsInfinity($N)) { return "-" }
    return "{0,$Width}" -f [math]::Round($N).ToString("N0")
}

function Format-Avg {
    param([double]$Sum, [double]$Count, [string]$Suffix = "ms")
    if ($Count -le 0) { return "-" }
    $avg = ($Sum / $Count) * 1000.0  # convert sec → ms
    if ($avg -ge 10000) { return "{0,7:N1}s" -f ($avg / 1000.0) }
    return "{0,5:N1}$Suffix" -f $avg
}

# Parse one prometheus snapshot file → hashtable of metric -> value.
# Histograms: keep the `_count`, `_sum` raw; let caller compute avg.
function Read-PromSnapshot {
    param([string]$Path)

    $m = @{}
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line.StartsWith("#")) { continue }
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        # metric_name{label1="...",label2="..."} value [timestamp]
        if ($line -match '^([a-zA-Z_:][a-zA-Z0-9_:]*)(\{[^}]*\})?\s+([0-9.eE+-]+)') {
            $name = $matches[1]
            $labels = if ($matches[2]) { $matches[2] } else { "" }
            $value = [double]$matches[3]
            $key = "$name$labels"
            $m[$key] = $value
        }
    }
    return $m
}

# Pull labelled value or 0 if missing.
function Get-Metric {
    param([hashtable]$M, [string]$Key)
    if ($M.ContainsKey($Key)) { return [double]$M[$Key] }
    return 0.0
}

# 按正则把一组序列求和(跨全部 label 组合)。一条都没匹配到返回 $null ——
# 调用方据此区分「指标不存在」(打印 n/a)与「存在但为 0」。Get-Metric 把两者
# 都折成 0,对后加的指标不够用:旧版本服务的快照里没有这些序列,显示成 0 会被
# 读成「测过了,是 0」。
function Get-MetricSumOrNull {
    param([hashtable]$M, [string]$Pattern)
    $sum = 0.0
    $found = $false
    foreach ($k in $M.Keys) {
        if ($k -match $Pattern) {
            $sum += [double]$M[$k]
            $found = $true
        }
    }
    if ($found) { return $sum }
    return $null
}

# ----- 1. robot stats ---------------------------------------------------

$RunDirResolved = (Resolve-Path $RunDir).Path
Write-Host ""
Write-Host "Run directory: $RunDirResolved" -ForegroundColor Cyan

$stderrPath = Join-Path $RunDirResolved "robot.stderr"
if (-not (Test-Path $stderrPath)) {
    throw "robot.stderr not found in $RunDirResolved"
}

Write-Host ""
Write-Host "=== Robot per-minute stats =============================================" -ForegroundColor Yellow
Write-Host ""
"  time   conn        login_ok   enter_ok   enter_fail  recon_fb  max_login  msg/s" | Write-Host
"  ----   ----        --------   --------   ----------  --------  ---------  -----" | Write-Host

# Each line we want has form:
#   [stats 5m0s] conn=25184 login_ok=25119 ... enter_ok=25119 enter_fail=0 ...
#   recon_fb=0 ... max_login=151ms ... msg_sent=...(199/s)
$statsPattern = '\[stats (?<t>\d+m0s)\] conn=(?<conn>\d+) login_ok=(?<login_ok>\d+) login_fail=\d+ login_stuck=\d+ enter_ok=(?<enter_ok>\d+) enter_fail=(?<enter_fail>\d+) msg_sent=\d+\((?<mps>\d+)/s\) .*?max_login=(?<max_login>\d+)ms .*?recon_fb=(?<recon_fb>\d+)'

$lastStat = $null
$rowCount = 0
foreach ($line in [System.IO.File]::ReadLines($stderrPath)) {
    if ($line -match $statsPattern) {
        $r = $matches
        $line = "  {0,-6} {1,-11} {2,-10} {3,-10} {4,-11} {5,-9} {6,-10} {7}" -f `
            $r['t'], `
            (Format-Number $r['conn'] 11), `
            (Format-Number $r['login_ok']  10), `
            (Format-Number $r['enter_ok'] 10), `
            (Format-Number $r['enter_fail'] 11), `
            (Format-Number $r['recon_fb'] 9), `
            ($r['max_login'] + "ms"), `
            ($r['mps'] + "/s")
        Write-Host $line
        $lastStat = $r
        $rowCount++
    }
}
if ($rowCount -eq 0) {
    Write-Host "  (no [stats Xm0s] markers found — was the run shorter than 1 minute?)" -ForegroundColor Red
}

# ----- 2. snapshot summaries -------------------------------------------

$snapDir = Join-Path $RunDirResolved "prom-snapshots"
$snapshots = @()
$smSnapshots = @()
$dbSnapshots = @()

if (Test-Path $snapDir) {
    $allTxt = Get-ChildItem $snapDir -Filter "*.txt" | Sort-Object Name
    # New multi-endpoint layout (Round 16+) uses suffixes: t{N}m_login.txt,
    # t{N}m_sm.txt, t{N}m_db.txt. Old single-endpoint layout used bare
    # t{N}m.txt and only ever contained login metrics.
    $loginTxt = $allTxt | Where-Object { $_.BaseName -match '_login$' }
    $smTxt    = $allTxt | Where-Object { $_.BaseName -match '_sm$' }
    $dbTxt    = $allTxt | Where-Object { $_.BaseName -match '_db$' }
    if ($loginTxt.Count -gt 0) {
        $snapshots   += $loginTxt
        $smSnapshots += $smTxt
        $dbSnapshots += $dbTxt
    } else {
        # Legacy: bare t{N}m.txt files all hold login metrics.
        $snapshots += $allTxt
    }
}

if ($ProbePromUrl) {
    Write-Host ""
    Write-Host "Probing live prometheus: $ProbePromUrl" -ForegroundColor DarkCyan
    $tmp = New-TemporaryFile
    try {
        Invoke-WebRequest -Uri $ProbePromUrl -OutFile $tmp -TimeoutSec 5 -UseBasicParsing
        $snapshots += Get-Item $tmp
    } catch {
        Write-Host "  live probe failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

if ($snapshots.Count -eq 0) {
    Write-Host ""
    Write-Host "(no prom-snapshots/ directory and no -ProbePromUrl; skipping prom analysis)" -ForegroundColor DarkGray
} else {

    Write-Host ""
    Write-Host "=== EnterGame end-to-end (entergame_*) =================================" -ForegroundColor Yellow
    Write-Host ""
    "  snapshot                       success  preload_failed  apply_failed  fail%   preload{ok}avg  preload{fail}avg  apply avg  pool_saturated" | Write-Host
    "  --------                       -------  --------------  ------------  -----   --------------  ----------------  ---------  --------------" | Write-Host

    foreach ($snap in $snapshots) {
        $m = Read-PromSnapshot $snap.FullName
        $success      = Get-Metric $m 'entergame_total{result="success"}'
        $preloadFail  = Get-Metric $m 'entergame_total{result="preload_failed"}'
        $applyFail    = Get-Metric $m 'entergame_total{result="apply_failed"}'
        $lockLost     = Get-Metric $m 'entergame_total{result="lock_lost"}'
        $total = $success + $preloadFail + $applyFail + $lockLost
        $failPct = if ($total -gt 0) { [math]::Round(($preloadFail + $applyFail + $lockLost) / $total * 100, 1) } else { 0.0 }

        # pool_saturated = PreloadPool 满、入场链根本没启动(EnterGame 当场回 kLoginInProgress)的次数。
        # 它**不是**链的终态:只在 entergame_total 里计数,不进任何耗时直方图,所以不计入上面的
        # $total 与 fail%,单列一列放在最后(原有各列的位置不变,新旧 summary 仍能逐列对比)。
        # 开 NewPlayerSeed 后新号的入场链整段跑在 PreloadPool 的 worker 上,这一列有数就该调大
        # login 的 PreloadPool.Size(docs/design/new-player-first-enter-no-cold-read.md §6 第 21 行)。
        # 缺序列打印 n/a 而不是 0:login 起服时由 PrimeEnterGameMetrics(clientplayerlogin/metrics.go)
        # 把这条序列预置成 0,所以带这个取值的 login 的快照里它恒存在,健康压测读到的就是 0;
        # 读到 n/a = 这个 login 不导出该取值(2026-10-13 之前的版本 / 预置没跑 / 采集没开),不是「测到 0」。
        $poolSatV = Get-MetricSumOrNull $m '^entergame_total\{.*result="pool_saturated"'
        $poolSatCell = if ($null -ne $poolSatV) { "{0:N0}" -f $poolSatV } else { "n/a" }

        $preloadOkAvg   = Format-Avg (Get-Metric $m 'entergame_preload_seconds_sum{result="success"}')        (Get-Metric $m 'entergame_preload_seconds_count{result="success"}')
        $preloadFailAvg = Format-Avg (Get-Metric $m 'entergame_preload_seconds_sum{result="preload_failed"}') (Get-Metric $m 'entergame_preload_seconds_count{result="preload_failed"}')
        $applyAvg       = Format-Avg (Get-Metric $m 'entergame_apply_seconds_sum{result="success"}')          (Get-Metric $m 'entergame_apply_seconds_count{result="success"}')

        $name = $snap.BaseName
        $line = "  {0,-30} {1,-8} {2,-15} {3,-13} {4,-7} {5,-15} {6,-17} {7,-10} {8}" -f `
            $name, `
            (Format-Number $success 8), `
            (Format-Number $preloadFail 15), `
            (Format-Number $applyFail 13), `
            ("{0:N1}%" -f $failPct), `
            $preloadOkAvg, `
            $preloadFailAvg, `
            $applyAvg, `
            $poolSatCell
        Write-Host $line
    }

    Write-Host ""
    Write-Host "=== Dataloader per-stage avg (dataloader_preload_*) ====================" -ForegroundColor Yellow
    Write-Host ""
    "  snapshot                       cache_check  sub_cache  dispatcher  kafka_send  cb_wait{ok}  cb_wait{fail}" | Write-Host
    "  --------                       -----------  ---------  ----------  ----------  -----------  -------------" | Write-Host

    foreach ($snap in $snapshots) {
        $m = Read-PromSnapshot $snap.FullName

        # cache_check has hit/miss labels; combine for an overall avg.
        $ccSumHit    = Get-Metric $m 'dataloader_preload_cache_check_seconds_sum{hit="hit"}'
        $ccCountHit  = Get-Metric $m 'dataloader_preload_cache_check_seconds_count{hit="hit"}'
        $ccSumMiss   = Get-Metric $m 'dataloader_preload_cache_check_seconds_sum{hit="miss"}'
        $ccCountMiss = Get-Metric $m 'dataloader_preload_cache_check_seconds_count{hit="miss"}'
        $cacheAvg = Format-Avg ($ccSumHit + $ccSumMiss) ($ccCountHit + $ccCountMiss)

        # sub_cache_check has labels by `subs=N`; we collapse by walking all keys.
        $subSum = 0.0; $subCount = 0.0
        foreach ($k in $m.Keys) {
            if ($k -match '^dataloader_preload_sub_cache_check_seconds_sum\{subs="') { $subSum += $m[$k] }
            elseif ($k -match '^dataloader_preload_sub_cache_check_seconds_count\{subs="') { $subCount += $m[$k] }
        }
        $subAvg = Format-Avg $subSum $subCount

        $dispatcherAvg = Format-Avg `
            (Get-Metric $m 'dataloader_preload_dispatcher_register_seconds_sum') `
            (Get-Metric $m 'dataloader_preload_dispatcher_register_seconds_count')

        $kafkaAvg = Format-Avg `
            (Get-Metric $m 'dataloader_preload_kafka_send_seconds_sum{result="success"}') `
            (Get-Metric $m 'dataloader_preload_kafka_send_seconds_count{result="success"}')

        $cbOkAvg   = Format-Avg `
            (Get-Metric $m 'dataloader_preload_callback_wait_seconds_sum{result="success"}') `
            (Get-Metric $m 'dataloader_preload_callback_wait_seconds_count{result="success"}')
        $cbFailAvg = Format-Avg `
            (Get-Metric $m 'dataloader_preload_callback_wait_seconds_sum{result="failed"}') `
            (Get-Metric $m 'dataloader_preload_callback_wait_seconds_count{result="failed"}')

        $name = $snap.BaseName
        $line = "  {0,-30} {1,-12} {2,-10} {3,-11} {4,-11} {5,-12} {6}" -f `
            $name, $cacheAvg, $subAvg, $dispatcherAvg, $kafkaAvg, $cbOkAvg, $cbFailAvg
        Write-Host $line
    }

    # 建角分阶段耗时(帮会二期 B3a-2,docs/design/guild-phase2/03-names.md §3.11 / §3.20)。
    # 指标在 login(:9101)上,所以与上面两段共用 login 快照。stage 的五个取值落码在
    # go/login/internal/logic/clientplayerlogin/metrics.go(mint / name / register / account_write
    # 由设计 §3.11 钉死,seed 由 docs/design/new-player-first-enter-no-cold-read.md §4.5 加入)
    # —— 那边改名或加取值这里必须跟着改,否则整列静默变成 n/a,或者新取值根本没有列:
    #   mint = 发 player_id,name = 名字占用(ReservePlayerName,含重试),
    #   register = RegisterPlayerZone,account_write = 围栏写账号 blob,
    #   seed = 新号父键种子(账号锁内多出的一次 Redis,排在 register 与 account_write 之间)。
    #          只在 login 的 NewPlayerSeed.Enabled 打开时有序列,关着时按下面的既有规则打印 n/a
    #          —— 所以这一列同时能核对本轮压测的开关状态。
    # 验收口径(§3.20)看 name 段均值与 name_orphan(建角失败后名字仍被占住的次数,应为 0)。
    # 各 stage 列缺指标一律打印 n/a,不折成 0:B3a-2 之前的 login 没有这些序列;histogram 的
    # 某个 stage 在第一次观测前也不会出现 —— 两种都不是「测到 0」。
    #
    # name_orphan 列:正常情况下直接读序列。login 起服时由 PrimeCreatePlayerMetrics
    # (metrics.go;login.go startServer 在 zrpc.MustNewServer 之后调用)把该计数器预置成 0,
    # 所以新版 login 且采集已开的快照里这条序列恒存在,健康压测读到的就是 0。
    #
    # 下面的 $stageSeen 分支只是兜底,覆盖「预置被移除 / 调用时机被挪到 MustNewServer 之前
    # 而被 prometheus.Enabled() 开关丢弃」的情形:无 label 的 CounterVec 在第一次写入前
    # 连 HELP/TYPE 都不输出,那时照抄上面的规则会让验收口径「orphan=0」永远读成 n/a。
    # 规则:序列缺失、但同一快照里存在任一 login_create_player_stage_seconds_count 序列
    # → 打印 0;两者都缺 → 打印 n/a。依据(都在 login 的 metrics.go / createplayerlogic.go):
    #   1. 直方图与该计数器在同一个 var 块注册、同受 prometheus.Enabled() 开关控制,
    #      直方图有序列即证明「新版 login + 采集已开」;
    #   2. 每条孤儿路径都在 mint 段观测之后才可达,不存在「出过孤儿却没有直方图序列」。
    # 该例外已记入设计 03-names.md §3.18 的 B3a-2 工具清单(stress_summarize.ps1 那一条)。
    Write-Host ""
    Write-Host "=== CreatePlayer stages (login_create_player_stage_seconds) ============" -ForegroundColor Yellow
    Write-Host ""
    "  snapshot                       mint       name       register   seed       account_write  name_orphan" | Write-Host
    "  --------                       ----       ----       --------   ----       -------------  -----------" | Write-Host

    $createStages = @('mint','name','register','seed','account_write')
    foreach ($snap in $snapshots) {
        $m = Read-PromSnapshot $snap.FullName
        $cells = @()
        foreach ($st in $createStages) {
            $sumV = Get-MetricSumOrNull $m "^login_create_player_stage_seconds_sum\{.*stage=`"$st`""
            $cntV = Get-MetricSumOrNull $m "^login_create_player_stage_seconds_count\{.*stage=`"$st`""
            $cell = if ($null -eq $sumV -or $null -eq $cntV) { "n/a" } else { Format-Avg $sumV $cntV }
            $cells += $cell
        }

        $orphanV = Get-MetricSumOrNull $m '^login_create_player_name_orphan_total(\{|$)'
        $stageSeen = $null -ne (Get-MetricSumOrNull $m '^login_create_player_stage_seconds_count\{')
        $orphanCell = if ($null -ne $orphanV) { "{0:N0}" -f $orphanV } elseif ($stageSeen) { "0" } else { "n/a" }

        $name = $snap.BaseName
        $line = "  {0,-30} {1,-10} {2,-10} {3,-10} {4,-10} {5,-14} {6}" -f `
            $name, $cells[0], $cells[1], $cells[2], $cells[3], $cells[4], $orphanCell
        Write-Host $line
    }
}

# ----- 2b. SceneManager EnterScene stage breakdown (Round 16+) ----------

if ($smSnapshots.Count -gt 0) {
    Write-Host ""
    Write-Host "=== SceneManager EnterScene stages (scene_manager_enter_scene_stage_seconds) ===" -ForegroundColor Yellow
    Write-Host ""
    "  snapshot                       dedup     scene_res reserve  upd_loc  route_gt rel_disp cross_zn" | Write-Host
    "  --------                       --------- --------- -------- -------- -------- -------- --------" | Write-Host

    $stages = @('dedup','scene_resolve','reserve','update_loc','route_gate','release_dispatch','cross_zone')
    foreach ($snap in $smSnapshots) {
        $m = Read-PromSnapshot $snap.FullName
        $cells = @()
        foreach ($st in $stages) {
            # Sum across all zone_id labels.
            $sumK = "scene_manager_enter_scene_stage_seconds_sum"
            $cntK = "scene_manager_enter_scene_stage_seconds_count"
            $sumV = 0.0; $cntV = 0.0
            foreach ($k in $m.Keys) {
                if ($k -match "^$sumK\{.*stage=`"$st`"") { $sumV += [double]$m[$k] }
                if ($k -match "^$cntK\{.*stage=`"$st`"") { $cntV += [double]$m[$k] }
            }
            $cells += (Format-Avg $sumV $cntV)
        }
        $name = $snap.BaseName
        $line = "  {0,-30} {1,-9} {2,-9} {3,-8} {4,-8} {5,-8} {6,-8} {7}" -f `
            $name, $cells[0], $cells[1], $cells[2], $cells[3], $cells[4], $cells[5], $cells[6]
        Write-Host $line
    }
}

# ----- 2bb. Dirty-save skip rate (cpp scene log, parsed not scraped) ----
#
# Why log-parsing instead of a Prometheus scrape: cpp scene processes don't
# expose a metrics HTTP endpoint today (only go SceneManager :9150 and go
# db :9160 do). The two atomic counters live in dirty_save_stats.h and emit
# one LOG_INFO line every 30 s from the RedisSystem snapshot timer. We pull
# the LATEST line from the most recent scene_*.log and surface the
# total / skipped / skip_pct triple.
#
# Stays silent when no [DirtySave] line is found — keeps legacy stress
# runs (pre-dirty-save-metric) clean and avoids a noisy "0 saves" line in
# very short smoke tests.

# Resolve repo root once from the script's own location ($PSScriptRoot is
# tools/scripts/, so two levels up is the repo root) — stress_summarize.ps1
# doesn't otherwise need it, but the dirty-save log scan below does.
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$cppLogDir = Join-Path $RepoRoot "run\logs\cpp_nodes"

# Only scan scene*.log files (gate logs don't run SavePlayerToRedis).
# The glob deliberately omits the underscore so it matches both legacy
# single-instance "scene.stdout.log" and multi-instance "scene_1.stdout.log".
$sceneLogs = @()
if (Test-Path $cppLogDir) {
    $sceneLogs = Get-ChildItem -Path $cppLogDir -Filter 'scene*.log' `
                 -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
}

$dirtySaveLines = @()
foreach ($log in $sceneLogs) {
    # Tail-style scan: read the file once and keep all matches; counters
    # are monotonic so the LAST match per file is the cumulative total.
    #
    # Use FileShare.ReadWrite — the scene process holds an open write
    # handle on its own stdout log while it's running, and the default
    # File::ReadLines opens with FileShare.None which would throw
    # "being used by another process". Round 17 (2026-06-03) hit this
    # the first time we ran the parser against a still-running scene.
    $fs = $null
    $sr = $null
    try {
        $fs = [System.IO.FileStream]::new(
            $log.FullName,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite)
        $sr = [System.IO.StreamReader]::new($fs)
        while (-not $sr.EndOfStream) {
            $line = $sr.ReadLine()
            if ($line -match '\[DirtySave\] total=(?<total>\d+) skipped=(?<skipped>\d+) skip_pct=(?<pct>[0-9.]+)%') {
                $dirtySaveLines += [pscustomobject]@{
                    Log     = $log.Name
                    Total   = [int64]$matches['total']
                    Skipped = [int64]$matches['skipped']
                    Pct     = [double]$matches['pct']
                }
            }
        }
    } finally {
        if ($sr) { $sr.Dispose() }
        if ($fs) { $fs.Dispose() }
    }
}

if ($dirtySaveLines.Count -gt 0) {
    Write-Host ""
    Write-Host "=== Dirty-save skip rate (cpp scene LOG_INFO [DirtySave]) ============" -ForegroundColor Yellow
    Write-Host ""
    "  scene_log                      total       skipped     skip_pct" | Write-Host
    "  ---------                      -----       -------     --------" | Write-Host

    # Group by log file and take the last (== highest, monotonic) sample
    # per scene instance. With one scene_1.log this is just one row; with
    # multi-scene runs each instance gets its own row.
    $byLog = $dirtySaveLines | Group-Object -Property Log
    foreach ($g in $byLog) {
        $latest = $g.Group | Select-Object -Last 1
        $line = "  {0,-30} {1,-11} {2,-11} {3,5:N1}%" -f `
            $g.Name, `
            (Format-Number $latest.Total 11), `
            (Format-Number $latest.Skipped 11), `
            $latest.Pct
        Write-Host $line
    }
}

# ----- 2bc. 节点间 RPC0 调用结局(cpp gate / scene 日志的 [RpcChannel] summary) ----
#
# C++ 节点没有 Prometheus 出口,RpcChannelStats(cpp/libs/engine/core/network/rpc_channel_stats.h)每 30s、
# 计数有变化时打一行 `[RpcChannel] summary k=v ...`,计数是进程级、单调累计的,所以每个日志文件取最后一行。
# 字段名是与 FormatRpcChannelStats 之间的契约(docs/design/muduo-rpc-channel.md §12),改名要两边一起改。
# 节点日志实际落在 bin\logs\cpp_nodes(start_game.ps1),旧路径 run\logs\cpp_nodes 也一并扫。
# 一行都没有时保持沉默(老版本二进制没有这行)。

$rpcChannelFields = @('started', 'ok', 'not_sent', 'remote_rejected', 'remote_failed', 'reply_dropped', 'timeout',
                      'disconnected', 'channel_closed', 'invalid_response', 'late_reply', 'legacy_id0_reply',
                      'max_in_flight', 'handler_failed', 'reply_abandoned')
$rpcChannelLatest = [ordered]@{}
foreach ($dir in @((Join-Path $RepoRoot "bin\logs\cpp_nodes"), $cppLogDir)) {
    if (-not (Test-Path $dir)) { continue }
    $nodeLogs = @(Get-ChildItem -Path $dir -Filter 'gate*.log' -ErrorAction SilentlyContinue) +
                @(Get-ChildItem -Path $dir -Filter 'scene*.log' -ErrorAction SilentlyContinue)
    foreach ($log in $nodeLogs) {
        # 与上面 DirtySave 同理:节点进程运行中持有写句柄,必须以 FileShare.ReadWrite 打开。
        $fs = $null
        $sr = $null
        $last = $null
        try {
            $fs = [System.IO.FileStream]::new(
                $log.FullName,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::ReadWrite)
            $sr = [System.IO.StreamReader]::new($fs)
            while (-not $sr.EndOfStream) {
                $line = $sr.ReadLine()
                $idx = $line.IndexOf('[RpcChannel] summary ')
                if ($idx -ge 0) { $last = $line.Substring($idx + '[RpcChannel] summary '.Length) }
            }
        } finally {
            if ($sr) { $sr.Dispose() }
            if ($fs) { $fs.Dispose() }
        }
        if ($null -ne $last) {
            $values = @{}
            foreach ($pair in ($last -split '\s+')) {
                if ($pair -match '^(?<k>[a-z0-9_]+)=(?<v>\d+)$') { $values[$matches['k']] = [int64]$matches['v'] }
            }
            $rpcChannelLatest["$($log.Directory.Name)/$($log.Name)"] = $values
        }
    }
}

if ($rpcChannelLatest.Count -gt 0) {
    Write-Host ""
    Write-Host "=== RPC0 call outcomes (cpp gate/scene LOG_INFO [RpcChannel] summary, cumulative) ===" -ForegroundColor Yellow
    Write-Host ""
    foreach ($entry in $rpcChannelLatest.GetEnumerator()) {
        $cells = foreach ($field in $rpcChannelFields) {
            $v = if ($entry.Value.ContainsKey($field)) { $entry.Value[$field] } else { 'n/a' }
            "$field=$v"
        }
        Write-Host ("  {0}" -f $entry.Key)
        Write-Host ("    {0}" -f ($cells -join ' '))
    }
    Write-Host "  (after a same-zone cutover timeout / late_reply / legacy_id0_reply should stay 0)"
}

# ----- 2c. DB per-task stage breakdown (Round 16+) ----------------------

if ($dbSnapshots.Count -gt 0) {
    Write-Host ""
    Write-Host "=== DB task stages (db_task_stage_seconds, db_task_result_total) ===" -ForegroundColor Yellow
    Write-Host ""
    "  snapshot                       read_total read_hdlr cache_wr publish  | write_total write_hdlr | ok      err" | Write-Host
    "  --------                       ---------- --------- -------- -------- | ----------- ---------- | ------- ----" | Write-Host

    foreach ($snap in $dbSnapshots) {
        $m = Read-PromSnapshot $snap.FullName

        # Get-Metric 按 key 精确查找,label 必须写成 /metrics 实际输出的顺序。
        # client_golang 输出时按 label 名字排序(op < stage),写成 {stage=..,op=..} 永远查不到,
        # 历次 summary(如 prev-summary-round18.txt、prev-summary.txt)这几列全是 "-" 就是这个原因
        # (go/db/internal/metrics/metrics.go 里声明的顺序是 stage, op,输出时会被重排)。
        $readTotalAvg = Format-Avg `
            (Get-Metric $m 'db_task_stage_seconds_sum{op="read",stage="op_total"}') `
            (Get-Metric $m 'db_task_stage_seconds_count{op="read",stage="op_total"}')
        $readHdlrAvg = Format-Avg `
            (Get-Metric $m 'db_task_stage_seconds_sum{op="read",stage="op_handler"}') `
            (Get-Metric $m 'db_task_stage_seconds_count{op="read",stage="op_handler"}')
        $readCacheAvg = Format-Avg `
            (Get-Metric $m 'db_task_stage_seconds_sum{op="read",stage="cache_write"}') `
            (Get-Metric $m 'db_task_stage_seconds_count{op="read",stage="cache_write"}')
        $publishAvg = Format-Avg `
            (Get-Metric $m 'db_task_stage_seconds_sum{op="read",stage="result_publish"}') `
            (Get-Metric $m 'db_task_stage_seconds_count{op="read",stage="result_publish"}')

        $writeTotalAvg = Format-Avg `
            (Get-Metric $m 'db_task_stage_seconds_sum{op="write",stage="op_total"}') `
            (Get-Metric $m 'db_task_stage_seconds_count{op="write",stage="op_total"}')
        $writeHdlrAvg = Format-Avg `
            (Get-Metric $m 'db_task_stage_seconds_sum{op="write",stage="op_handler"}') `
            (Get-Metric $m 'db_task_stage_seconds_count{op="write",stage="op_handler"}')

        $okCnt  = (Get-Metric $m 'db_task_result_total{op="read",result="ok"}') + (Get-Metric $m 'db_task_result_total{op="write",result="ok"}')
        $errCnt = (Get-Metric $m 'db_task_result_total{op="read",result="error"}') + (Get-Metric $m 'db_task_result_total{op="write",result="error"}')

        $name = $snap.BaseName
        $line = "  {0,-30} {1,-10} {2,-9} {3,-8} {4,-8} | {5,-11} {6,-10} | {7,-7} {8}" -f `
            $name, $readTotalAvg, $readHdlrAvg, $readCacheAvg, $publishAvg, $writeTotalAvg, $writeHdlrAvg, `
            (Format-Number $okCnt 7), (Format-Number $errCnt 4)
        Write-Host $line
    }
}

# ----- 2d / 2e / 2f. DB 写路径打点、Redis 分步、重试路由(Round 20+) ----
#
# 指标来源(那边改名或加取值,这里与 test_stress_summarize.ps1 必须同改,否则整列静默变成 n/a):
#   - db_task_stage_seconds 的 stage=sql / task_total / queue_wait / kafka_e2e:go/db/internal/metrics/metrics.go;
#   - db_task_redis_step_seconds、db_ordering_guard_total、db_subshard_batch_size、db_ordering_lease_lost_total、
#     db_ordering_lock_release_total、db_redis_pool_*:go/db/internal/metrics/pipeline.go;
#   - db_retry_routing_mode、db_retry_route_total、db_retry_queue_length(含 queue="partitioned"):
#     go/db/internal/metrics/metrics.go(重试只交给分区属主,docs/design/db-retry-owner-routing.md)。
# 缺序列一律打印 n/a,不折成 0:Round 20 之前的 db 没有这些序列,显示成 0 会被读成「测过了,是 0」。
# histogram 的 sum / count 都有序列但 count=0 时打印 "-"(同 Format-Avg)。
# 键按 client_golang 输出时的 label 字母序写(op < stage < step;action < mode),原因见 2c 段注释。

# 一条序列的精确匹配正则;$Labels 为空表示无 label 的序列。
function Get-SeriesPattern {
    param([string]$Name, [string]$Labels = "")
    if ($Labels) { return '^' + [regex]::Escape($Name) + '\{' + [regex]::Escape($Labels) + '\}$' }
    return '^' + [regex]::Escape($Name) + '$'
}

# histogram 均值(秒)。sum / count 任一序列缺失返回 $null(→ n/a);count 为 0 返回 NaN(→ "-")。
function Get-HistogramAvgOrNull {
    param([hashtable]$M, [string]$Name, [string]$Labels = "")
    $sumV = Get-MetricSumOrNull $M (Get-SeriesPattern "${Name}_sum" $Labels)
    $cntV = Get-MetricSumOrNull $M (Get-SeriesPattern "${Name}_count" $Labels)
    if ($null -eq $sumV -or $null -eq $cntV) { return $null }
    if ($cntV -le 0) { return [double]::NaN }
    return $sumV / $cntV
}

# 秒 → 毫秒文本;≥10s 改用秒(同 Format-Avg)。
function Format-AvgCell {
    param($AvgSeconds, [int]$Decimals = 1)
    if ($null -eq $AvgSeconds) { return "n/a" }
    if ([double]::IsNaN([double]$AvgSeconds)) { return "-" }
    $ms = [double]$AvgSeconds * 1000.0
    if ($ms -ge 10000) { return "{0:N1}s" -f ($ms / 1000.0) }
    return "{0:N$Decimals}ms" -f $ms
}

# 计数文本;$null(缺序列)→ n/a。
function Format-CountCell {
    param($Value)
    if ($null -eq $Value) { return "n/a" }
    return "{0:N0}" -f [double]$Value
}

if ($dbSnapshots.Count -gt 0) {

    # ----- 2d. DB write path ---------------------------------------------
    # 列含义:
    #   w_sql / r_sql      runDBOp 里 handler 的耗时(写含大字段闸 + Save,读是 SELECT)
    #   w_task / r_task    handleTask 整段
    #   w_queue            入 worker 通道到 handleTask 开始(排队)
    #   w_e2e              生产者 CreateTime 到 handleTask 结束,只统计 Kafka 来源;有跨机时钟偏差,只看趋势
    #                      (超过 5s 落 +Inf 桶,但 sum/count 的均值仍准)
    #   batch              子 shard 每批合并的任务数均值(db_subshard_batch_size 的 sum/count)
    #   no_cursor          db_ordering_guard_total{outcome="apply_no_cursor"}:没有 applied 游标、按首写放行的次数
    #   lease_lost         db_ordering_lease_lost_total 各 reason 之和(排序锁失租,应接近 0)
    #   rel_notown         db_ordering_lock_release_total{outcome="not_owner"}(释放时锁已不归本实例,应为 0)
    #   rpool_wait / rpool_tmo
    #                      go-redis 连接池等待次数 / 取连接超时次数(db_redis_pool_wait_total / timeouts_total),
    #                      跨 redis_client 求和;placement 与 main 是同一个 client 时只导出 main 一份,不会重复计数。
    #                      注意 wait_total 与 wait_seconds_total 是两条指标,正则锚到 "_wait_total{" 不会串。
    $wpFmt = "  {0,-30} {1,-8} {2,-8} {3,-8} {4,-8} | {5,-8} {6,-8} | {7,-6} | {8,-9} {9,-10} {10,-10} | {11,-10} {12}"
    Write-Host ""
    Write-Host "=== DB write path (db_task_stage_seconds sql/task_total/queue_wait/kafka_e2e, Round 20+) ===" -ForegroundColor Yellow
    Write-Host ""
    Write-Host ($wpFmt -f 'snapshot', 'w_sql', 'w_task', 'w_queue', 'w_e2e', 'r_sql', 'r_task', 'batch',
        'no_cursor', 'lease_lost', 'rel_notown', 'rpool_wait', 'rpool_tmo')
    Write-Host ($wpFmt -f '--------', '-----', '------', '-------', '-----', '-----', '------', '-----',
        '---------', '----------', '----------', '----------', '---------')

    foreach ($snap in $dbSnapshots) {
        $m = Read-PromSnapshot $snap.FullName
        $stageName = 'db_task_stage_seconds'

        $wSql   = Format-AvgCell (Get-HistogramAvgOrNull $m $stageName 'op="write",stage="sql"')
        $wTask  = Format-AvgCell (Get-HistogramAvgOrNull $m $stageName 'op="write",stage="task_total"')
        $wQueue = Format-AvgCell (Get-HistogramAvgOrNull $m $stageName 'op="write",stage="queue_wait"')
        $wE2e   = Format-AvgCell (Get-HistogramAvgOrNull $m $stageName 'op="write",stage="kafka_e2e"')
        $rSql   = Format-AvgCell (Get-HistogramAvgOrNull $m $stageName 'op="read",stage="sql"')
        $rTask  = Format-AvgCell (Get-HistogramAvgOrNull $m $stageName 'op="read",stage="task_total"')

        $batchAvg = Get-HistogramAvgOrNull $m 'db_subshard_batch_size'
        $batchCell = if ($null -eq $batchAvg) { "n/a" } elseif ([double]::IsNaN($batchAvg)) { "-" } else { "{0:N1}" -f $batchAvg }

        $noCursor  = Format-CountCell (Get-MetricSumOrNull $m (Get-SeriesPattern 'db_ordering_guard_total' 'outcome="apply_no_cursor"'))
        $leaseLost = Format-CountCell (Get-MetricSumOrNull $m '^db_ordering_lease_lost_total\{')
        $relNotOwn = Format-CountCell (Get-MetricSumOrNull $m (Get-SeriesPattern 'db_ordering_lock_release_total' 'outcome="not_owner"'))
        $rpoolWait = Format-CountCell (Get-MetricSumOrNull $m '^db_redis_pool_wait_total\{')
        $rpoolTmo  = Format-CountCell (Get-MetricSumOrNull $m '^db_redis_pool_timeouts_total\{')

        Write-Host ($wpFmt -f $snap.BaseName, $wSql, $wTask, $wQueue, $wE2e, $rSql, $rTask, $batchCell,
            $noCursor, $leaseLost, $relNotOwn, $rpoolWait, $rpoolTmo)
    }

    # ----- 2e. DB Redis steps --------------------------------------------
    # 写任务每个 Redis 往返的均值。12 个 step 的名字与顺序同 go/db/internal/metrics/pipeline.go 的 RedisSteps
    # (按写路径顺序),一一对应;那边改名 / 合并 step 时,这里、test_stress_summarize.ps1 与 TestRedisStepNamesPinned 同改。
    # 只读 op="write";读任务只有 placement_lookup / expand_get / placement_recheck / cache_set 四步,不进本表。
    # 围栏 v0.4 起 cache_set 只剩读路径 SET NX,写任务这列应为 n/a;lease_verify 只剩 older_seq 条件路径。
    # cache_publish 合并写路径核锁与缓存发布,applied_advance 合并 cursor_set / epoch_mark。
    # R20 的 cache_set / lease_verify 含义已变、cursor_set / epoch_mark 已移除,新旧列不能直接相减。
    # 均值保留两位小数(Redis 往返常在 0.1ms 量级,一位小数分不开)。sum = 有序列的各步均值之和,
    # 近似一条写任务花在 Redis 往返上的时间;某步没有序列时不计入,全部没有打印 n/a。
    $redisSteps = @('lock_acquire', 'cursor_get', 'epoch_get', 'owner_epoch_probe', 'placement_lookup', 'expand_get', 'placement_recheck',
                    'lease_verify', 'cache_publish', 'cache_set', 'applied_advance', 'lock_release')
    $rsFmt = "  {0,-30} {1,-8} {2,-8} {3,-8} {4,-8} {5,-8} {6,-8} {7,-8} {8,-8} {9,-8} {10,-8} {11,-8} {12,-8} {13}"
    Write-Host ""
    Write-Host "=== DB Redis steps (db_task_redis_step_seconds{op=`"write`"} avg, fence v0.4) ===" -ForegroundColor Yellow
    Write-Host '  口径: verify 仅 older_seq;cache_nx 仅读路径(本表通常 n/a);publish / advance 已合并旧步骤,勿直接减 R20 对应列。'
    Write-Host ""
    Write-Host ($rsFmt -f 'snapshot', 'lock_acq', 'cur_get', 'ep_get', 'owner_ep', 'plc_lkp', 'exp_get', 'plc_rck', 'verify',
        'publish', 'cache_nx', 'advance', 'lock_rel', 'sum')
    Write-Host ($rsFmt -f '--------', '--------', '-------', '------', '--------', '-------', '-------', '-------', '------',
        '-------', '--------', '-------', '--------', '---')

    foreach ($snap in $dbSnapshots) {
        $m = Read-PromSnapshot $snap.FullName
        $cells = @()
        $sumSeconds = 0.0
        $anySeries = $false
        $anyValue = $false
        foreach ($step in $redisSteps) {
            $avg = Get-HistogramAvgOrNull $m 'db_task_redis_step_seconds' "op=`"write`",step=`"$step`""
            $cells += (Format-AvgCell $avg 2)
            if ($null -eq $avg) { continue }
            $anySeries = $true
            if (-not [double]::IsNaN($avg)) { $sumSeconds += $avg; $anyValue = $true }
        }
        $sumCell = if (-not $anySeries) { "n/a" } elseif (-not $anyValue) { "-" } else { Format-AvgCell $sumSeconds 2 }
        $rowArgs = @($snap.BaseName) + $cells + @($sumCell)
        Write-Host ($rsFmt -f $rowArgs)
    }

    # ----- 2f. DB retry routing ------------------------------------------
    # 重试只交给分区属主(docs/design/db-retry-owner-routing.md)的观测面。列:
    #   mode       db_retry_routing_mode 中值为 1 的那个(shared / owner);旧版 db 没有该序列 → n/a
    #   q_ready / q_part / q_proc / q_dead
    #              db_retry_queue_length{queue=ready|partitioned|processing|dead}:共享收件箱、按分区的 p* 之和、
    #              本实例 processing、死信。ready / partitioned / dead 是 topic 级共享列表,每个实例报的是同一个数;
    #              q_part 在 shared 模式下应为 0;旧版 db 没有 partitioned → n/a,其余三列照常有数
    #   exec / nonowner / fwd / revoked / misrouted / rt_dead
    #              db_retry_route_total{action=executed|executed_nonowner|forwarded|requeued_revoked|misrouted|dead}
    #              跨 mode 求和,是本实例的累计;owner 模式的 zone 里 nonowner 应为 0
    # 快照是单个 db 实例(:9160)的抓取;多实例时各实例的计数不在这里合并。
    $rtFmt = "  {0,-30} {1,-7} | {2,-7} {3,-7} {4,-7} {5,-7} | {6,-7} {7,-8} {8,-7} {9,-7} {10,-9} {11}"
    Write-Host ""
    Write-Host "=== DB retry routing (db_retry_routing_mode, db_retry_queue_length, db_retry_route_total) ===" -ForegroundColor Yellow
    Write-Host ""
    Write-Host ($rtFmt -f 'snapshot', 'mode', 'q_ready', 'q_part', 'q_proc', 'q_dead',
        'exec', 'nonowner', 'fwd', 'revoked', 'misrouted', 'rt_dead')
    Write-Host ($rtFmt -f '--------', '----', '-------', '------', '------', '------',
        '----', '--------', '---', '-------', '---------', '-------')

    $retryQueues = @('ready', 'partitioned', 'processing', 'dead')
    $retryActions = @('executed', 'executed_nonowner', 'forwarded', 'requeued_revoked', 'misrouted', 'dead')
    foreach ($snap in $dbSnapshots) {
        $m = Read-PromSnapshot $snap.FullName

        $ownerV  = Get-MetricSumOrNull $m (Get-SeriesPattern 'db_retry_routing_mode' 'mode="owner"')
        $sharedV = Get-MetricSumOrNull $m (Get-SeriesPattern 'db_retry_routing_mode' 'mode="shared"')
        $modeCell = if ($null -eq $ownerV -and $null -eq $sharedV) { "n/a" }
                    elseif ($ownerV -gt 0 -and $sharedV -gt 0) { "both?" }
                    elseif ($ownerV -gt 0) { "owner" }
                    elseif ($sharedV -gt 0) { "shared" }
                    else { "none" }

        $queueCells = foreach ($q in $retryQueues) {
            Format-CountCell (Get-MetricSumOrNull $m (Get-SeriesPattern 'db_retry_queue_length' "queue=`"$q`""))
        }
        # action 在 mode 之前(字母序);action="executed" 带上右引号,不会把 executed_nonowner 算进来。
        $actionCells = foreach ($a in $retryActions) {
            Format-CountCell (Get-MetricSumOrNull $m ('^db_retry_route_total\{action="' + [regex]::Escape($a) + '",'))
        }

        $rowArgs = @($snap.BaseName, $modeCell) + @($queueCells) + @($actionCells)
        Write-Host ($rtFmt -f $rowArgs)
    }
}

# ----- 3. Kafka lag -----------------------------------------------------

if ($KafkaContainer) {
    Write-Host ""
    Write-Host "=== Kafka consumer lag (live) ==========================================" -ForegroundColor Yellow
    Write-Host ""

    $cmd = "/opt/kafka/bin/kafka-consumer-groups.sh"
    $args = @(
        "exec", $KafkaContainer, $cmd,
        "--bootstrap-server", "localhost:9092",
        "--describe", "--group", $KafkaConsumerGroup
    )

    $raw = ""
    try {
        $raw = & docker @args 2>&1 | Out-String
    } catch {
        Write-Host "  docker exec failed: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host ""
        return
    }

    $totalLag = 0
    $partRows = @()
    foreach ($line in ($raw -split "`r?`n")) {
        # GROUP TOPIC PARTITION CURRENT-OFFSET LOG-END-OFFSET LAG ...
        if ($line -match "^\S+\s+$KafkaTopic\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)") {
            $p = [int]$matches[1]
            $cur = [int]$matches[2]
            $end = [int]$matches[3]
            $lag = [int]$matches[4]
            $totalRows += 1
            $totalLag += $lag
            $partRows += [pscustomobject]@{ Part = $p; Cur = $cur; End = $end; Lag = $lag }
        }
    }

    if ($partRows.Count -eq 0) {
        Write-Host "  (no lag rows parsed — group $KafkaConsumerGroup empty? or container=$KafkaContainer down?)" -ForegroundColor DarkGray
    } else {
        "  partition    current      log_end       lag" | Write-Host
        "  ---------    -------      -------       ---" | Write-Host
        foreach ($r in ($partRows | Sort-Object Part)) {
            $line = "  {0,-12} {1,-12} {2,-13} {3}" -f $r.Part, $r.Cur.ToString("N0"), $r.End.ToString("N0"), $r.Lag.ToString("N0")
            Write-Host $line
        }
        Write-Host ""
        Write-Host ("  TOTAL LAG: {0}" -f $totalLag.ToString("N0")) -ForegroundColor Yellow
    }
}

Write-Host ""
