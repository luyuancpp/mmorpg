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
                 | preload{ok} avg | preload{fail} avg | apply avg

      [dataloader per-stage avg at each prom-snapshot point]
          when | cache_check{hit} | sub_cache_check | kafka_send | callback_wait{ok} | callback_wait{fail}

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

.PARAMETER KafkaTopic
    Topic to inspect for consumer lag. Default 'db_task_zone_1'.

.PARAMETER KafkaConsumerGroup
    Consumer group to inspect. Default 'db_rpc_consumer_group'.

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
    [string]$KafkaConsumerGroup = "db_rpc_consumer_group"
)

$ErrorActionPreference = "Stop"

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

if (Test-Path $snapDir) {
    $snapshots += Get-ChildItem $snapDir -Filter "*.txt" | Sort-Object Name
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
    "  snapshot                       success  preload_failed  apply_failed  fail%   preload{ok}avg  preload{fail}avg  apply avg" | Write-Host
    "  --------                       -------  --------------  ------------  -----   --------------  ----------------  ---------" | Write-Host

    foreach ($snap in $snapshots) {
        $m = Read-PromSnapshot $snap.FullName
        $success      = Get-Metric $m 'entergame_total{result="success"}'
        $preloadFail  = Get-Metric $m 'entergame_total{result="preload_failed"}'
        $applyFail    = Get-Metric $m 'entergame_total{result="apply_failed"}'
        $lockLost     = Get-Metric $m 'entergame_total{result="lock_lost"}'
        $total = $success + $preloadFail + $applyFail + $lockLost
        $failPct = if ($total -gt 0) { [math]::Round(($preloadFail + $applyFail + $lockLost) / $total * 100, 1) } else { 0.0 }

        $preloadOkAvg   = Format-Avg (Get-Metric $m 'entergame_preload_seconds_sum{result="success"}')        (Get-Metric $m 'entergame_preload_seconds_count{result="success"}')
        $preloadFailAvg = Format-Avg (Get-Metric $m 'entergame_preload_seconds_sum{result="preload_failed"}') (Get-Metric $m 'entergame_preload_seconds_count{result="preload_failed"}')
        $applyAvg       = Format-Avg (Get-Metric $m 'entergame_apply_seconds_sum{result="success"}')          (Get-Metric $m 'entergame_apply_seconds_count{result="success"}')

        $name = $snap.BaseName
        $line = "  {0,-30} {1,-8} {2,-15} {3,-13} {4,-7} {5,-15} {6,-17} {7}" -f `
            $name, `
            (Format-Number $success 8), `
            (Format-Number $preloadFail 15), `
            (Format-Number $applyFail 13), `
            ("{0:N1}%" -f $failPct), `
            $preloadOkAvg, `
            $preloadFailAvg, `
            $applyAvg
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
