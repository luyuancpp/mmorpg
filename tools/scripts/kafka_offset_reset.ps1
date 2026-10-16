#requires -Version 7
<#
.SYNOPSIS
    Reset a Kafka consumer group's offsets — used by zone rollback / disaster recovery —
    and finalize a db_task consumer-group handoff.

.DESCRIPTION
    Wraps `kafka-consumer-groups.sh` (must be on PATH or pointed at via -KafkaBin).
    Modes (exactly one):
      • -ToDatetime: reset to a specific UTC timestamp (PITR-style)
      • -ToEarliest / -ToLatest: reset to topic boundaries
      • -DeleteAndRecreateTopic: nuke the topic and recreate it (most aggressive — used
        when the rollback target time is older than topic retention)
      • -FinalizeHandoff: 交接收尾(docs/design/db-consumer-group-per-zone.md §5)。go/db 的消费组按 zone 拆分后,
        每次正向(db_rpc_consumer_group → db_rpc_consumer_group_z<id>)或反向交接完成,都要删除旧组在本 topic 上的位点:
        先逐分区确认新权威组(-Group)已接手(有数值位点且不小于旧组;没有位点的分区只在 earliest == latest 即空分区时放行),
        再对旧组(-RetireGroup)执行 --delete-offsets,并 describe 复核旧组已没有本 topic 的数值位点。
        不收尾时两组都有位点,合服门禁拒绝、go/db 交接闸的 R7 会拦住反向交接。

    重置模式(-ToDatetime / -ToEarliest / -ToLatest)的复核:
      • 输出里的 GROUP/TOPIC/PARTITION/NEW-OFFSET 计划表必须解析得出;给了 -Partitions N(> 0)时计划必须覆盖 0..N-1。
        dry-run 与 -Apply 都检查。
      • -Apply 时 --execute 之后再 describe 一次,逐分区断言 CURRENT-OFFSET 等于计划的 NEW-OFFSET;不符即失败。
      • 给了 -RetireGroup 时,只有复核通过才删除 RetireGroup 在本 topic 上的位点(回档是有意回退,不做「新组 ≥ 旧组」检查)。
        回档 Step 5 用它把 zone 落到 zone 组:重置 zone 组到 TargetTime,再退役 legacy 组在本 topic 上的位点。

    Always defaults to dry-run (uses `--dry-run` flag of the kafka tool, which prints
    the plan without writing). Pass -Apply to actually execute.

    See docs/design/zone_data_rollback.md §3 step 4 for the rollback context.

.EXAMPLE
    # Preview offsets reset to a timestamp
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Group db_rpc_consumer_group_z101 `
        -Topic db_task_zone_101 `
        -ToDatetime "2026-05-15T03:17:00Z" `
        -Partitions 10

.EXAMPLE
    # Apply (write) the reset, verify it, then retire the legacy group's offsets on this topic (rollback Step 5)
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Group db_rpc_consumer_group_z101 `
        -RetireGroup db_rpc_consumer_group `
        -Topic db_task_zone_101 `
        -ToDatetime "2026-05-15T03:17:00Z" `
        -Partitions 10 `
        -Apply

.EXAMPLE
    # Reset to earliest
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Group db_rpc_consumer_group_z101 `
        -Topic db_task_zone_101 `
        -ToEarliest `
        -Apply

.EXAMPLE
    # Finalize a forward handoff (legacy → zone group): dry-run first, then add -Apply
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Topic db_task_zone_101 `
        -Group db_rpc_consumer_group_z101 `
        -RetireGroup db_rpc_consumer_group `
        -Partitions 10 `
        -FinalizeHandoff

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
    # 要退役的旧组:删除它在 -Topic 上的已提交位点。只能配 -ToDatetime / -ToEarliest / -ToLatest(复核通过后删)
    # 或 -FinalizeHandoff;必须同时给 -Partitions > 0,不能等于 -Group。
    [string]$RetireGroup = "",
    [string]$Topic = "",

    # Reset modes — exactly one of:
    [string]$ToDatetime = "",   # ISO 8601 UTC, e.g. "2026-05-15T03:17:00Z"
    [switch]$ToEarliest,
    [switch]$ToLatest,
    [switch]$DeleteAndRecreateTopic,
    [switch]$FinalizeHandoff,

    # Required when -DeleteAndRecreateTopic / -FinalizeHandoff / -RetireGroup.
    # 重置模式下给了(> 0)就要求重置计划覆盖 0..N-1;取 go/db/etc/db.yaml 的 ServerConfig.Kafka.PartitionCnt。
    [int]$Partitions = 0,
    [int]$ReplicationFactor = 1,

    # Path to the Kafka CLI (default: assume on PATH).
    # On a typical Confluent / Apache Kafka install:
    #   /opt/kafka/bin/kafka-consumer-groups.sh
    #   /opt/kafka/bin/kafka-topics.sh
    #   /opt/kafka/bin/kafka-get-offsets.sh
    [string]$KafkaBin = "",

    [switch]$Apply
)

$ErrorActionPreference = "Stop"

# describe / 重置计划 / get-offsets 的解析与收尾判定只在这一处(k8s_zone_rollback.ps1、k8s_deploy.ps1 共用)。
. (Join-Path $PSScriptRoot "lib" "kafka_consumer_groups.ps1")

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
# 这里两种都按失败处理)。K8s 切成每 zone 一组(db_rpc_consumer_group_z<id>)之后,回档重置的是本 zone 组,zone-down 后它
# 是 Empty;但 legacy 共享组 db_rpc_consumer_group 只要别的 zone 还在线就不是 Empty,对它重置一定被拒 —— 以前脚本照样打印 OK,
# 回档后的 zone 带着旧位点继续消费。所以退出码之外还要看输出(fail-closed)。describe / get-offsets / delete-offsets 同样过这道判定。
function Assert-KafkaToolOutputClean {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][int]$ExitCode,
        [Parameter(Mandatory = $true)][string]$Tool,
        # 失败时错误文本里说明的后果;缺省是重置场景的说法。
        [string]$Consequence = '位点没有重置'
    )
    if ($ExitCode -ne 0) {
        throw "$Tool failed with exit code $ExitCode"
    }
    $errorLines = @($Lines | Where-Object { $_ -match '^\s*Error:' })
    if ($errorLines.Count -gt 0) {
        throw "$Tool 退出码为 0 但输出了错误,${Consequence}: $($errorLines -join ' | ')"
    }
}

<#
.SYNOPSIS
    执行一次 Kafka CLI,合并收集 stdout / stderr 为字符串行,返回 @{ Lines; ExitCode }。不判成败。
.DESCRIPTION
    局部放宽 ErrorActionPreference:成败只看退出码与输出,不让 stderr 行本身中断脚本("Error:" 行可能走 stderr)。
    先清零 LASTEXITCODE,避免把更早一次调用的退出码当成本次的。
#>
function Invoke-KafkaToolCaptured {
    param(
        [Parameter(Mandatory = $true)][string]$Tool,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    $prevErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $global:LASTEXITCODE = 0
        $lines = @(& $Tool @Arguments 2>&1 | ForEach-Object { "$_" })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prevErrorAction
    }
    return [pscustomobject]@{ Lines = $lines; ExitCode = $code }
}

<#
.SYNOPSIS
    describe 一个组,按 lib 的 Resolve-KafkaGroupTopicOffsets 分类:missing / no-offsets / offsets;判不出就 throw。
#>
function Invoke-KafkaConsumerGroupDescribe {
    param(
        [Parameter(Mandatory = $true)][string]$CgTool,
        [Parameter(Mandatory = $true)][string]$BootstrapServer,
        [Parameter(Mandatory = $true)][string]$Group,
        [Parameter(Mandatory = $true)][string]$Topic
    )
    $describeArgs = @("--bootstrap-server", $BootstrapServer, "--describe", "--group", $Group)
    Write-Host "  kafka-consumer-groups.sh $($describeArgs -join ' ')"
    $result = Invoke-KafkaToolCaptured -Tool $CgTool -Arguments $describeArgs
    $result.Lines | ForEach-Object { Write-Host "    $_" }
    $state = Resolve-KafkaGroupTopicOffsets -Lines $result.Lines -ExitCode $result.ExitCode -Topic $Topic
    Write-Host "  => group $Group on ${Topic}: $($state.State) ($($state.Committed.Count) partition(s) with committed offsets)"
    return $state
}

<#
.SYNOPSIS
    kafka-get-offsets.sh --time -2(earliest)/ -1(latest),返回 @{ [int]分区 = [long]位点 }。
#>
function Invoke-KafkaGetOffsets {
    param(
        [Parameter(Mandatory = $true)][string]$Tool,
        [Parameter(Mandatory = $true)][string]$BootstrapServer,
        [Parameter(Mandatory = $true)][string]$Topic,
        [Parameter(Mandatory = $true)][ValidateSet("-2", "-1")][string]$Time
    )
    $offsetArgs = @("--bootstrap-server", $BootstrapServer, "--topic", $Topic, "--time", $Time)
    Write-Host "  kafka-get-offsets.sh $($offsetArgs -join ' ')"
    $result = Invoke-KafkaToolCaptured -Tool $Tool -Arguments $offsetArgs
    $result.Lines | ForEach-Object { Write-Host "    $_" }
    Assert-KafkaToolOutputClean -Lines $result.Lines -ExitCode $result.ExitCode -Tool "kafka-get-offsets" -Consequence '读不到日志边界'
    return ConvertFrom-KafkaGetOffsets -Lines $result.Lines -Topic $Topic
}

<#
.SYNOPSIS
    重置计划的覆盖检查(纯函数):-Partitions N > 0 时,计划必须恰好覆盖 0..N-1。返回问题列表。
#>
function Get-ResetPlanProblems {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Plan,
        [Parameter(Mandatory = $true)][int]$Partitions
    )
    $problems = [System.Collections.Generic.List[string]]::new()
    if ($Partitions -le 0) { return , $problems.ToArray() }
    for ($p = 0; $p -lt $Partitions; $p++) {
        if (-not $Plan.ContainsKey($p)) { $problems.Add("重置计划缺少分区 $p") }
    }
    foreach ($key in @($Plan.Keys | Sort-Object)) {
        if ([int]$key -ge $Partitions) {
            $problems.Add("重置计划里有分区 $key,超出 -Partitions $Partitions(topic 的分区数与 go/db/etc/db.yaml PartitionCnt 不一致?)")
        }
    }
    return , $problems.ToArray()
}

<#
.SYNOPSIS
    重置 -Apply 之后的复核(纯函数):describe 结果里逐分区的 CURRENT-OFFSET 必须等于计划的 NEW-OFFSET。返回问题列表。
.PARAMETER Described
    Resolve-KafkaGroupTopicOffsets 的返回值。组仍不存在(--execute 没有建出位点)即问题。
.PARAMETER Partitions
    > 0 时逐个核对 0..N-1;否则核对计划里的全部分区。
#>
function Get-ResetVerificationProblems {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Plan,
        [Parameter(Mandatory = $true)]$Described,
        [Parameter(Mandatory = $true)][int]$Partitions
    )
    $problems = [System.Collections.Generic.List[string]]::new()
    if ($Described.State -eq 'missing') {
        $problems.Add("重置之后组仍不存在,--execute 没有建出任何位点")
        return , $problems.ToArray()
    }
    $committed = $Described.Committed
    $expected = if ($Partitions -gt 0) { @(0..($Partitions - 1)) } else { @($Plan.Keys | ForEach-Object { [int]$_ } | Sort-Object) }
    foreach ($p in $expected) {
        if (-not $Plan.ContainsKey($p)) {
            $problems.Add("分区 ${p}:不在重置计划里")
            continue
        }
        if (-not $committed.ContainsKey($p)) {
            $problems.Add("分区 ${p}:重置之后没有已提交位点(计划 $($Plan[$p]))")
            continue
        }
        if ([long]$committed[$p] -ne [long]$Plan[$p]) {
            $problems.Add("分区 ${p}:已提交位点 $($committed[$p]) 不等于计划 $($Plan[$p])")
        }
    }
    return , $problems.ToArray()
}

<#
.SYNOPSIS
    删除旧组(-RetireGroup)在 -Topic 上的已提交位点,删后 describe 复核;不带 -Apply 只打印将要执行的命令。

.DESCRIPTION
    旧组不存在或在本 topic 上没有数值位点时打印 nothing to retire 后返回(幂等,可重跑)。
    --delete-offsets 只要还有成员订阅该 topic 就会被 broker 拒绝(GROUP_SUBSCRIBED_TO_TOPIC);
    legacy 组即使还有别的 zone 的成员,只要没有成员订阅本 topic 就能删。删完必须复核:仍有数值位点即失败。
#>
function Remove-RetiredGroupTopicOffsets {
    param(
        [Parameter(Mandatory = $true)][string]$CgTool,
        [Parameter(Mandatory = $true)][string]$BootstrapServer,
        [Parameter(Mandatory = $true)][string]$RetireGroup,
        [Parameter(Mandatory = $true)][string]$Topic,
        [switch]$Apply
    )
    $before = Invoke-KafkaConsumerGroupDescribe -CgTool $CgTool -BootstrapServer $BootstrapServer -Group $RetireGroup -Topic $Topic
    if ($before.State -ne 'offsets') {
        Write-Host "  nothing to retire: group $RetireGroup has no committed offsets on $Topic ($($before.State))" -ForegroundColor Green
        return
    }
    $deleteArgs = @("--bootstrap-server", $BootstrapServer, "--delete-offsets", "--group", $RetireGroup, "--topic", $Topic)
    if (-not $Apply) {
        Write-Host "[DRY-RUN] would run kafka-consumer-groups.sh $($deleteArgs -join ' ')" -ForegroundColor Yellow
        return
    }
    Write-Host "  kafka-consumer-groups.sh $($deleteArgs -join ' ')"
    $result = Invoke-KafkaToolCaptured -Tool $CgTool -Arguments $deleteArgs
    $result.Lines | ForEach-Object { Write-Host "    $_" }
    Assert-KafkaToolOutputClean -Lines $result.Lines -ExitCode $result.ExitCode -Tool "kafka-consumer-groups --delete-offsets" -Consequence "旧组 $RetireGroup 的位点没有删除"
    $subscribed = @($result.Lines | Where-Object { $_ -match 'GROUP_SUBSCRIBED_TO_TOPIC|actively subscribed' })
    if ($subscribed.Count -gt 0) {
        throw "删除旧组 $RetireGroup 在 $Topic 上的位点被 broker 拒绝:仍有成员订阅该 topic(GROUP_SUBSCRIBED_TO_TOPIC)。先停掉仍在消费本 topic 的旧组实例再重跑:$($subscribed -join ' | ')"
    }
    $after = Invoke-KafkaConsumerGroupDescribe -CgTool $CgTool -BootstrapServer $BootstrapServer -Group $RetireGroup -Topic $Topic
    if ($after.State -eq 'offsets') {
        throw ("旧组 $RetireGroup 在 $Topic 上的位点没有删干净(复核仍有 $($after.Committed.Count) 个分区有数值位点),多半仍有成员订阅该 topic" +
            "(GROUP_SUBSCRIBED_TO_TOPIC)。先停掉仍在消费本 topic 的旧组实例,再重跑本命令。")
    }
    Write-Host "  OK: group $RetireGroup has no committed offsets on $Topic anymore" -ForegroundColor Green
}

# ── Validate exactly one mode ─────────────────────────────────────
$modes = @()
if ($ToDatetime -ne "")        { $modes += "ToDatetime" }
if ($ToEarliest)               { $modes += "ToEarliest" }
if ($ToLatest)                 { $modes += "ToLatest" }
if ($DeleteAndRecreateTopic)   { $modes += "DeleteAndRecreateTopic" }
if ($FinalizeHandoff)          { $modes += "FinalizeHandoff" }
if ($modes.Count -ne 1) {
    throw "Pass exactly one of -ToDatetime / -ToEarliest / -ToLatest / -DeleteAndRecreateTopic / -FinalizeHandoff. Got: $($modes -join ',')"
}

# ── Required-arg sanity ───────────────────────────────────────────
if ($Topic -eq "") { throw "-Topic is required" }
if ($modes[0] -ne "DeleteAndRecreateTopic" -and $Group -eq "") {
    throw "-Group is required for offset reset (only -DeleteAndRecreateTopic skips group)"
}
if ($DeleteAndRecreateTopic -and $Partitions -le 0) {
    throw "-DeleteAndRecreateTopic requires -Partitions > 0"
}
if ($FinalizeHandoff -and ($RetireGroup -eq "" -or $Partitions -le 0)) {
    throw "-FinalizeHandoff 必须同时给 -Group <新权威组>、-RetireGroup <旧组>、-Topic 与 -Partitions <分区数 > 0>(分区数取 go/db/etc/db.yaml 的 ServerConfig.Kafka.PartitionCnt)"
}
if ($RetireGroup -ne "") {
    if ($DeleteAndRecreateTopic) {
        throw "-RetireGroup 不能与 -DeleteAndRecreateTopic 同用:删 topic 会让所有组在该 topic 上的位点一起作废,没有「旧组位点」可退役"
    }
    if ($Partitions -le 0) {
        throw "-RetireGroup 必须同时给 -Partitions <分区数 > 0>:删除旧组位点之前要逐分区复核权威组的位点,分区数取 go/db/etc/db.yaml 的 ServerConfig.Kafka.PartitionCnt"
    }
    if ($RetireGroup -ceq $Group) {
        throw "-RetireGroup 不能等于 -Group($Group):那会删掉权威组自己在 $Topic 上的位点"
    }
}

$verb = if ($Apply) { "APPLY" } else { "DRY-RUN" }
Write-Host "=== Kafka offset reset ($verb) ===" -ForegroundColor Cyan
Write-Host "  bootstrap : $BootstrapServer"
Write-Host "  topic     : $Topic"
if ($Group -ne "") { Write-Host "  group     : $Group" }
if ($RetireGroup -ne "") { Write-Host "  retire    : $RetireGroup (its committed offsets on $Topic are deleted after the checks pass)" }
if ($Partitions -gt 0) { Write-Host "  partitions: $Partitions" }
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

# ── FinalizeHandoff ───────────────────────────────────────────────
# 先确认新权威组已接手全部分区,再删旧组位点;任一检查不过就在删除之前拒绝,并一次列全问题。
if ($FinalizeHandoff) {
    $cgTool = Resolve-KafkaTool "kafka-consumer-groups.sh"
    $offsetsTool = Resolve-KafkaTool "kafka-get-offsets.sh"

    Write-Host "Step 1: describe new authoritative group $Group" -ForegroundColor Yellow
    $newState = Invoke-KafkaConsumerGroupDescribe -CgTool $cgTool -BootstrapServer $BootstrapServer -Group $Group -Topic $Topic
    Write-Host "Step 2: describe retiring group $RetireGroup" -ForegroundColor Yellow
    $oldState = Invoke-KafkaConsumerGroupDescribe -CgTool $cgTool -BootstrapServer $BootstrapServer -Group $RetireGroup -Topic $Topic
    Write-Host "Step 3: log boundaries of $Topic (earliest / latest)" -ForegroundColor Yellow
    $earliest = Invoke-KafkaGetOffsets -Tool $offsetsTool -BootstrapServer $BootstrapServer -Topic $Topic -Time "-2"
    $latest = Invoke-KafkaGetOffsets -Tool $offsetsTool -BootstrapServer $BootstrapServer -Topic $Topic -Time "-1"

    $problems = Test-HandoffFinalizeReady -NewOffsets $newState.Committed -OldOffsets $oldState.Committed `
        -Earliest $earliest -Latest $latest -Partitions $Partitions
    if ($problems.Count -gt 0) {
        throw ("交接收尾拒绝执行,没有删除 $RetireGroup 的任何位点:新权威组 $Group 在 $Topic 上还没有接手全部 $Partitions 个分区" +
            "($($problems.Count) 项问题):`n  - " + ($problems -join "`n  - ") +
            "`n确认新组的 db 实例已在消费、且旧组实例已全部停掉后重跑;方向弄反时交换 -Group 与 -RetireGroup。")
    }
    Write-Host "  new group $Group covers all $Partitions partitions of $Topic (empty partitions excepted) and is not behind $RetireGroup" -ForegroundColor Green

    Write-Host "Step 4: retire committed offsets of $RetireGroup on $Topic" -ForegroundColor Yellow
    Remove-RetiredGroupTopicOffsets -CgTool $cgTool -BootstrapServer $BootstrapServer -RetireGroup $RetireGroup -Topic $Topic -Apply:$Apply
    if ($Apply) {
        Write-Host "OK: handoff finalized; $Group is the only group with committed offsets on $Topic" -ForegroundColor Green
        Write-Host "Next: re-run k8s_deploy.ps1 -Command zone-up for this zone with the default -DbConsumerGroup keep;"
        Write-Host "      the db ConfigMap drops GroupHandoffFrom (renders GroupID: `"$Group`") and db rolls once."
    } else {
        Write-Host "[DRY-RUN] Checks passed. Re-run with -Apply to delete the retiring group's offsets." -ForegroundColor Yellow
    }
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

# dry-run 与 --execute 都打印 GROUP/TOPIC/PARTITION/NEW-OFFSET 计划表;解析不出、或不覆盖 0..N-1 都按失败处理:
# 没有计划就没法在 -Apply 之后逐分区复核,而复核是删除 -RetireGroup 位点的前提。
$plan = ConvertFrom-KafkaResetPlan -Lines $cgOutput -Topic $Topic
$planProblems = Get-ResetPlanProblems -Plan $plan -Partitions $Partitions
if ($planProblems.Count -gt 0) {
    $applied = if ($Apply) { "--execute 已执行、计划内的分区可能已写入;没有删除任何旧组位点。" } else { "" }
    throw ("重置计划不完整($($planProblems.Count) 项):`n  - " + ($planProblems -join "`n  - ") + "`n$applied")
}

if ($Apply) {
    # 复核必须先于删除 -RetireGroup 的位点:重置静默无效时再删掉旧组位点,两组都没有位点,
    # go/db 会从 Oldest 重放整个保留期(回档 Step 4 已 FLUSHDB 清掉游标,写不会被游标吸收)。
    Write-Host "Step: verify committed offsets of $Group on $Topic equal the plan" -ForegroundColor Yellow
    $afterReset = Invoke-KafkaConsumerGroupDescribe -CgTool $cgTool -BootstrapServer $BootstrapServer -Group $Group -Topic $Topic
    $verifyProblems = Get-ResetVerificationProblems -Plan $plan -Described $afterReset -Partitions $Partitions
    if ($verifyProblems.Count -gt 0) {
        $notRetired = if ($RetireGroup -ne "") { ",没有删除 $RetireGroup 的位点" } else { "" }
        throw ("重置后复核失败$notRetired($($verifyProblems.Count) 项):`n  - " + ($verifyProblems -join "`n  - ") +
            "`n先核对 $Group 的位点(kafka-consumer-groups.sh --describe --group $Group),再重跑本命令。")
    }
    Write-Host "OK: offset reset applied and verified ($($plan.Count) partition(s))" -ForegroundColor Green
    if ($RetireGroup -ne "") {
        # 回档是有意回退:不做「新组 ≥ 旧组」检查(那是 -FinalizeHandoff 的前置条件)。
        Write-Host "Step: retire committed offsets of $RetireGroup on $Topic" -ForegroundColor Yellow
        Remove-RetiredGroupTopicOffsets -CgTool $cgTool -BootstrapServer $BootstrapServer -RetireGroup $RetireGroup -Topic $Topic -Apply
    }
} else {
    Write-Host "[DRY-RUN] Plan printed above ($($plan.Count) partition(s)). Re-run with -Apply to execute." -ForegroundColor Yellow
    if ($RetireGroup -ne "") {
        Write-Host "[DRY-RUN] With -Apply, once the reset is verified, committed offsets of $RetireGroup on $Topic are deleted (kafka-consumer-groups.sh --delete-offsets)." -ForegroundColor Yellow
    }
}
