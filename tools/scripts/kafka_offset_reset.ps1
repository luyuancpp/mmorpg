#requires -Version 7
<#
.SYNOPSIS
    Reset a Kafka consumer group's offsets — used by zone rollback / disaster recovery.

.DESCRIPTION
    Wraps `kafka-consumer-groups.sh` (must be on PATH or pointed at via -KafkaBin).
    Two modes:
      • -ToDatetime: reset to a specific UTC timestamp (PITR-style)
      • -ToEarliest / -ToLatest: reset to topic boundaries
      • -DeleteAndRecreateTopic: nuke the topic and recreate it (most aggressive — used
        when the rollback target time is older than topic retention)

    Always defaults to dry-run (uses `--dry-run` flag of the kafka tool, which prints
    the plan without writing). Pass -Apply to actually execute.

    See docs/design/zone_data_rollback.md §3 step 4 for the rollback context.

.EXAMPLE
    # Preview offsets reset to a timestamp
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Group db_rpc_consumer_group `
        -Topic db_task_zone_101 `
        -ToDatetime "2026-05-15T03:17:00Z"

.EXAMPLE
    # Apply (write) the reset
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Group db_rpc_consumer_group `
        -Topic db_task_zone_101 `
        -ToDatetime "2026-05-15T03:17:00Z" `
        -Apply

.EXAMPLE
    # Reset to earliest
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Group db_rpc_consumer_group `
        -Topic db_task_zone_101 `
        -ToEarliest `
        -Apply

.EXAMPLE
    # Nuclear: delete topic and recreate (zone rollback fallback when binlog spans > retention)
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Topic db_task_zone_101 `
        -DeleteAndRecreateTopic `
        -Partitions 5 `
        -ReplicationFactor 1 `
        -Apply
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$BootstrapServer,

    [string]$Group = "",
    [string]$Topic = "",

    # Reset modes — exactly one of:
    [string]$ToDatetime = "",   # ISO 8601 UTC, e.g. "2026-05-15T03:17:00Z"
    [switch]$ToEarliest,
    [switch]$ToLatest,
    [switch]$DeleteAndRecreateTopic,

    # Required when -DeleteAndRecreateTopic
    [int]$Partitions = 0,
    [int]$ReplicationFactor = 1,

    # Path to the Kafka CLI (default: assume on PATH).
    # On a typical Confluent / Apache Kafka install:
    #   /opt/kafka/bin/kafka-consumer-groups.sh
    #   /opt/kafka/bin/kafka-topics.sh
    [string]$KafkaBin = "",

    [switch]$Apply
)

$ErrorActionPreference = "Stop"

function Resolve-KafkaTool {
    param([string]$Name)
    if ($KafkaBin -ne "") {
        $candidate = Join-Path $KafkaBin $Name
        if (Test-Path $candidate) { return $candidate }
        throw "Kafka tool not found at $candidate"
    }
    # Try PATH lookup (Linux: kafka-consumer-groups.sh; Confluent: kafka-consumer-groups)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    # Try the .sh suffix variant
    if ($Name -notmatch '\.sh$') {
        $sh = Get-Command "$Name.sh" -ErrorAction SilentlyContinue
        if ($sh) { return $sh.Source }
    }
    throw "Kafka tool '$Name' not found on PATH; pass -KafkaBin <dir> or install bin/ to PATH."
}

# kafka-consumer-groups.sh 有一类失败只打印 "Error: ..." 而退出码仍为 0。典型是对仍有活跃成员
# (状态不是 Empty / Dead)的组执行 --reset-offsets:
#   Error: Assignments can only be reset if the group 'g' is inactive, but the current state is Stable.
# 位点根本没动(Kafka 3.x 的 ConsumerGroupCommand 打完这行就正常返回;在钉死的镜像版本上的退出码待实测,
# 这里两种都按失败处理)。K8s 上所有 zone 共用 db_rpc_consumer_group,只回档一个 zone 时组里永远有别的 zone
# 的成员,这就是常态;以前脚本照样打印 OK,回档后的 zone 带着旧位点继续消费。所以退出码之外还要看输出(fail-closed)。
function Assert-KafkaToolOutputClean {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][int]$ExitCode,
        [Parameter(Mandatory = $true)][string]$Tool
    )
    if ($ExitCode -ne 0) {
        throw "$Tool failed with exit code $ExitCode"
    }
    $errorLines = @($Lines | Where-Object { $_ -match '^\s*Error:' })
    if ($errorLines.Count -gt 0) {
        throw "$Tool 退出码为 0 但输出了错误,位点没有重置: $($errorLines -join ' | ')"
    }
}

# ── Validate exactly one mode ─────────────────────────────────────
$modes = @()
if ($ToDatetime -ne "")        { $modes += "ToDatetime" }
if ($ToEarliest)               { $modes += "ToEarliest" }
if ($ToLatest)                 { $modes += "ToLatest" }
if ($DeleteAndRecreateTopic)   { $modes += "DeleteAndRecreateTopic" }
if ($modes.Count -ne 1) {
    throw "Pass exactly one of -ToDatetime / -ToEarliest / -ToLatest / -DeleteAndRecreateTopic. Got: $($modes -join ',')"
}

# ── Required-arg sanity ───────────────────────────────────────────
if ($Topic -eq "") { throw "-Topic is required" }
if ($modes[0] -ne "DeleteAndRecreateTopic" -and $Group -eq "") {
    throw "-Group is required for offset reset (only -DeleteAndRecreateTopic skips group)"
}
if ($DeleteAndRecreateTopic -and $Partitions -le 0) {
    throw "-DeleteAndRecreateTopic requires -Partitions > 0"
}

$verb = if ($Apply) { "APPLY" } else { "DRY-RUN" }
Write-Host "=== Kafka offset reset ($verb) ===" -ForegroundColor Cyan
Write-Host "  bootstrap : $BootstrapServer"
Write-Host "  topic     : $Topic"
if ($Group -ne "") { Write-Host "  group     : $Group" }
Write-Host "  mode      : $($modes[0])"
if ($ToDatetime -ne "") { Write-Host "  datetime  : $ToDatetime" }
Write-Host ""

# ── DeleteAndRecreateTopic ────────────────────────────────────────
if ($DeleteAndRecreateTopic) {
    $topicTool = Resolve-KafkaTool "kafka-topics.sh"

    Write-Host "Step 1: describe (pre-delete)" -ForegroundColor Yellow
    & $topicTool --bootstrap-server $BootstrapServer --describe --topic $Topic
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  (topic may not exist yet — continuing)"
    }

    if (-not $Apply) {
        Write-Host "[DRY-RUN] Would delete topic '$Topic' and recreate with partitions=$Partitions rf=$ReplicationFactor" -ForegroundColor Yellow
        exit 0
    }

    Write-Host "Step 2: delete" -ForegroundColor Yellow
    & $topicTool --bootstrap-server $BootstrapServer --delete --topic $Topic
    if ($LASTEXITCODE -ne 0) { throw "kafka-topics --delete failed" }

    # Kafka delete is async; wait for it to disappear before recreating.
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $exists = (& $topicTool --bootstrap-server $BootstrapServer --list 2>$null) -match "^$([regex]::Escape($Topic))$"
        if (-not $exists) { break }
        Start-Sleep -Seconds 1
    }

    Write-Host "Step 3: create" -ForegroundColor Yellow
    & $topicTool --bootstrap-server $BootstrapServer --create `
        --topic $Topic --partitions $Partitions --replication-factor $ReplicationFactor
    if ($LASTEXITCODE -ne 0) { throw "kafka-topics --create failed" }

    Write-Host "OK: topic '$Topic' recreated with $Partitions partitions" -ForegroundColor Green
    exit 0
}

# ── Offset reset (group + topic mode) ─────────────────────────────
$cgTool = Resolve-KafkaTool "kafka-consumer-groups.sh"

# kafka-consumer-groups.sh accepts --dry-run (default behaviour without --execute);
# we mirror that with the -Apply switch: -Apply ⇒ --execute, otherwise --dry-run.
$cmdArgs = @(
    "--bootstrap-server", $BootstrapServer,
    "--group", $Group,
    "--topic", $Topic,
    "--reset-offsets"
)
switch ($modes[0]) {
    "ToDatetime" { $cmdArgs += @("--to-datetime", $ToDatetime) }
    "ToEarliest" { $cmdArgs += "--to-earliest" }
    "ToLatest"   { $cmdArgs += "--to-latest" }
}
if ($Apply) {
    $cmdArgs += "--execute"
} else {
    $cmdArgs += "--dry-run"
}

Write-Host "Step: kafka-consumer-groups.sh $($cmdArgs -join ' ')" -ForegroundColor Yellow
# 输出要留下来交给 Assert-KafkaToolOutputClean;stderr 一并收进来("Error:" 行可能走 stderr)。
# 局部放宽 ErrorActionPreference:成败只看退出码与输出,不让 stderr 行本身中断脚本。
$prevErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    $cgOutput = @(& $cgTool @cmdArgs 2>&1 | ForEach-Object { "$_" })
    $exitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $prevErrorAction
}
$cgOutput | ForEach-Object { Write-Host $_ }
Assert-KafkaToolOutputClean -Lines $cgOutput -ExitCode $exitCode -Tool "kafka-consumer-groups"

if ($Apply) {
    Write-Host "OK: offset reset applied" -ForegroundColor Green
    Write-Host "Verify with:"
    Write-Host "  $cgTool --bootstrap-server $BootstrapServer --describe --group $Group"
} else {
    Write-Host "[DRY-RUN] Plan printed above. Re-run with -Apply to execute." -ForegroundColor Yellow
}
