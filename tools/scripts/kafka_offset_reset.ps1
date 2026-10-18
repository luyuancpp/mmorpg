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
      • -DeleteAndRecreateTopic: nuke the topic and recreate it (most aggressive).
        db_task topic(db_task_zone_<id>[_g<gen>])一律拒绝(2026-10-17,R7 复核 [1]):go/db 的已落库游标
        consumer:applied:<topic>:<key>:<type> 只按 topic 名区分、永不过期,且在共享 DB0 里(回档 Step 4 的防护恰好不清它)。
        同名重建后各分区 offset 从 0 重新开始,新存盘的 seq 都不大于旧游标,被判为 orderingStale:跳过落库、照常提交位点,
        存盘静默丢失,直到新 offset 追上旧值。要换一个空 topic,走 ServerConfig.Kafka.TopicGeneration 换代(新 topic 名就是新的游标命名空间;
        go/db、login、merge_zone、k8s_deploy 都按它推导 topic 名)。
        其它 topic:必须显式给 -Partitions 与 -ReplicationFactor,且都要等于现有 topic 的分区数与副本数(先 describe 核对,读不到就拒绝);
        删除后等 topic 消失(--list 失败或 30s 超时即失败,不 create),重建后再 describe 复核。
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
    # Nuclear: delete a (non db_task) topic and recreate it with the same partition count / replication factor.
    # db_task_zone_<id>[_g<gen>] is refused: bump go/db ServerConfig.Kafka.TopicGeneration instead (see .DESCRIPTION).
    pwsh -File tools/scripts/kafka_offset_reset.ps1 `
        -BootstrapServer kafka:9092 `
        -Topic <topic> `
        -DeleteAndRecreateTopic `
        -Partitions <现有分区数> `
        -ReplicationFactor <现有副本数> `
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
    # -DeleteAndRecreateTopic 必填(> 0),且必须等于现有 topic 的副本数。0 = 没给。
    # 以前缺省为 1:3 broker 档(min.insync.replicas=2)上重建出 RF=1 的 topic,acks=all 的写全部 NotEnoughReplicas,
    # go/shared/kafkautil 的启动校验也会判 replication contract mismatch。
    [int]$ReplicationFactor = 0,

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
        # 退出码只经返回值报告。全局 LASTEXITCODE 复位为 0:被判为可接受的非 0(如「组不存在」的 describe)不能漏到调用方 ——
        # k8s_zone_rollback.ps1 Step 5 与 dev_tools.ps1 按 $LASTEXITCODE 判定本脚本成败(R7 复核 [5])。
        $global:LASTEXITCODE = 0
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

<#
.SYNOPSIS
    解析 kafka-topics.sh --describe --topic <t> 的摘要行,返回 @{ PartitionCount [int]; ReplicationFactor [int] };找不到就 throw(纯函数)。
.DESCRIPTION
    摘要行形如 "Topic: t<TAB>TopicId: x<TAB>PartitionCount: 5<TAB>ReplicationFactor: 3<TAB>Configs: …"(旧版本冒号后没有空格),
    只认 topic 名与 -Topic 完全相同的那一行;同一 topic 出现两行摘要也 throw(不猜)。
#>
function ConvertFrom-KafkaTopicDescribe {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$Topic
    )
    $found = @()
    foreach ($raw in @($Lines | ForEach-Object { "$_" })) {
        $match = [regex]::Match($raw, '^\s*Topic:\s*(?<t>\S+)\s.*?PartitionCount:\s*(?<p>\d+)\s+ReplicationFactor:\s*(?<r>\d+)')
        if ($match.Success -and $match.Groups['t'].Value -ceq $Topic) {
            $found += [pscustomobject]@{ PartitionCount = [int]$match.Groups['p'].Value; ReplicationFactor = [int]$match.Groups['r'].Value }
        }
    }
    if ($found.Count -ne 1) {
        throw "kafka-topics --describe 的输出里 topic $Topic 的 PartitionCount / ReplicationFactor 摘要行有 $($found.Count) 行(期望恰好 1 行),读不出现有 topic 的分区数与副本数"
    }
    return $found[0]
}

<#
.SYNOPSIS
    删除 topic 之后等它从 --list 里消失;--list 失败或超时都 throw(不进入 create)。
.DESCRIPTION
    Kafka 的删除是异步的。以前 --list 失败时 2>$null 让判断结果为空、被当成「已消失」,30s 超时也照样 create。
    超时用单调时钟(Stopwatch)计,-TimeoutSec 0 = 只看一次。throw 时 topic 可能已删、尚未重建:broker 开着 auto.create.topics.enable 时
    producer 会把它自动建成 num.partitions(=1)个分区,错误文本要求先停掉该 topic 的生产者再手工重建。
#>
function Wait-KafkaTopicDeleted {
    param(
        [Parameter(Mandatory = $true)][string]$TopicTool,
        [Parameter(Mandatory = $true)][string]$BootstrapServer,
        [Parameter(Mandatory = $true)][string]$Topic,
        [Parameter(Mandatory = $true)][int]$TimeoutSec,
        [int]$PollMilliseconds = 1000,
        [Parameter(Mandatory = $true)][string]$RecreateHint
    )
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $listed = Invoke-KafkaToolCaptured -Tool $TopicTool -Arguments @("--bootstrap-server", $BootstrapServer, "--list")
        try {
            Assert-KafkaToolOutputClean -Lines $listed.Lines -ExitCode $listed.ExitCode -Tool "kafka-topics --list" -Consequence '判不出 topic 是否已删除'
        }
        catch {
            throw "删除之后读不到 topic 列表,没有重建 ${Topic}:$($_.Exception.Message)。$RecreateHint"
        }
        if (@($listed.Lines | Where-Object { "$_".Trim() -ceq $Topic }).Count -eq 0) { return }
        if ($stopwatch.Elapsed.TotalSeconds -ge $TimeoutSec) {
            throw "topic $Topic 在删除之后 ${TimeoutSec}s 内仍在 --list 里(删除仍在进行),没有重建。$RecreateHint"
        }
        Start-Sleep -Milliseconds $PollMilliseconds
    }
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
# -DeleteAndRecreateTopic 的两条拒绝放在参数组合校验之后(参数组合本身错了先报组合错误),都早于解析任何 Kafka 工具。
if ($DeleteAndRecreateTopic -and (Test-DbTaskTopicName -Topic $Topic)) {
    throw ("-DeleteAndRecreateTopic 拒绝 db_task topic $Topic(尚未做任何改动):go/db 的已落库游标 consumer:applied:${Topic}:* 只按 topic 名区分、永不过期," +
        "且在共享 DB0 里(回档 Step 4 不清它)。同名重建后 offset 从 0 重新开始,新存盘的 seq 都不大于旧游标,被判为 orderingStale 静默跳过,存盘丢失。" +
        "要换一个空 topic,把 go/db / go/login 的 Kafka.TopicGeneration 加一(新 topic 名即新的游标命名空间,go/db、login、merge_zone、k8s_deploy 都按它推导)," +
        "按 docs/design/db-task-kafka-partition-contract.md「离线扩容流程」换代;只想回到某个时间点,用 -ToDatetime / -ToEarliest / -ToLatest。")
}
if ($DeleteAndRecreateTopic -and $ReplicationFactor -le 0) {
    throw "-DeleteAndRecreateTopic 必须显式给 -ReplicationFactor <副本数 > 0>,且等于现有 topic 的副本数(以前缺省 1:3 broker 档 min.insync.replicas=2 下重建出的 topic 写入全部 NotEnoughReplicas)"
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
    $createArgs = @("--bootstrap-server", $BootstrapServer, "--create", "--topic", $Topic, "--partitions", "$Partitions", "--replication-factor", "$ReplicationFactor")
    $recreateHint = ("topic 此时可能已删除、尚未重建:broker 开着 auto.create.topics.enable 时 producer 会把它自动建成默认分区数(num.partitions)。" +
        "立即停掉该 topic 的全部生产者,手工执行 kafka-topics.sh $($createArgs -join ' '),describe 核对 PartitionCount=$Partitions ReplicationFactor=$ReplicationFactor 后再恢复。")

    # 删除之前先核对现有 topic 的分区数与副本数:读不到就不删(fail-closed)。重建必须保持原契约 ——
    # 分区数变了等于换了按 key 的分区函数(要走 TopicGeneration 换代),副本数变了会撞上 min.insync.replicas 与启动校验。
    Write-Host "Step 1: describe (pre-delete)" -ForegroundColor Yellow
    $before = Invoke-KafkaToolCaptured -Tool $topicTool -Arguments @("--bootstrap-server", $BootstrapServer, "--describe", "--topic", $Topic)
    $before.Lines | ForEach-Object { Write-Host "    $_" }
    Assert-KafkaToolOutputClean -Lines $before.Lines -ExitCode $before.ExitCode -Tool "kafka-topics --describe" -Consequence "读不到 topic $Topic 的现状,没有删除任何东西"
    $current = ConvertFrom-KafkaTopicDescribe -Lines $before.Lines -Topic $Topic
    if ($current.PartitionCount -ne $Partitions -or $current.ReplicationFactor -ne $ReplicationFactor) {
        throw ("拒绝删除 topic $Topic(尚未做任何改动):现有 PartitionCount=$($current.PartitionCount) ReplicationFactor=$($current.ReplicationFactor)," +
            "与本次 -Partitions $Partitions -ReplicationFactor $ReplicationFactor 不一致。重建必须保持原分区数与副本数;要改分区数走 TopicGeneration 换代。")
    }

    if (-not $Apply) {
        Write-Host "[DRY-RUN] Would delete topic '$Topic' and recreate with partitions=$Partitions rf=$ReplicationFactor (matches the existing topic)" -ForegroundColor Yellow
        exit 0
    }

    Write-Host "Step 2: delete" -ForegroundColor Yellow
    $deleted = Invoke-KafkaToolCaptured -Tool $topicTool -Arguments @("--bootstrap-server", $BootstrapServer, "--delete", "--topic", $Topic)
    $deleted.Lines | ForEach-Object { Write-Host "    $_" }
    Assert-KafkaToolOutputClean -Lines $deleted.Lines -ExitCode $deleted.ExitCode -Tool "kafka-topics --delete" -Consequence "删除是否生效不确定。$recreateHint"

    # Kafka 的删除是异步的:等它从 --list 里消失再 create;--list 失败或超时都 throw,不 create。
    Wait-KafkaTopicDeleted -TopicTool $topicTool -BootstrapServer $BootstrapServer -Topic $Topic -TimeoutSec 30 -RecreateHint $recreateHint

    Write-Host "Step 3: create" -ForegroundColor Yellow
    $created = Invoke-KafkaToolCaptured -Tool $topicTool -Arguments $createArgs
    $created.Lines | ForEach-Object { Write-Host "    $_" }
    Assert-KafkaToolOutputClean -Lines $created.Lines -ExitCode $created.ExitCode -Tool "kafka-topics --create" -Consequence "topic $Topic 已删除但重建失败。$recreateHint"

    Write-Host "Step 4: describe (post-create)" -ForegroundColor Yellow
    $after = Invoke-KafkaToolCaptured -Tool $topicTool -Arguments @("--bootstrap-server", $BootstrapServer, "--describe", "--topic", $Topic)
    $after.Lines | ForEach-Object { Write-Host "    $_" }
    Assert-KafkaToolOutputClean -Lines $after.Lines -ExitCode $after.ExitCode -Tool "kafka-topics --describe" -Consequence "重建之后读不到 topic $Topic 的现状。$recreateHint"
    $recreated = ConvertFrom-KafkaTopicDescribe -Lines $after.Lines -Topic $Topic
    if ($recreated.PartitionCount -ne $Partitions -or $recreated.ReplicationFactor -ne $ReplicationFactor) {
        throw ("topic $Topic 重建之后是 PartitionCount=$($recreated.PartitionCount) ReplicationFactor=$($recreated.ReplicationFactor)," +
            "不是期望的 $Partitions / $ReplicationFactor(可能在删除与重建之间被 producer 自动创建了)。$recreateHint")
    }

    Write-Host "OK: topic '$Topic' recreated with $Partitions partitions, replication factor $ReplicationFactor" -ForegroundColor Green
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
    # 复核必须先于删除 -RetireGroup 的位点:重置静默无效时再删掉旧组位点,两组都没有位点,go/db 会从 Oldest 重放整个保留期。
    # 重放的写是否被游标吸收取决于 Redis 里的 consumer:applied 游标还在不在,不能假定回档清过它(R7 复核 [4]):
    #   - K8s 上游标在共享 DB0,回档 Step 4 的 FLUSHDB 防护(k8s_zone_rollback.ps1 Get-SharedRedisGuardPatterns 含 consumer:applied*)
    #     恰好拒绝清它:已落过库的消息 seq 不大于游标,判 stale 吸收;从未推进过游标的(zone-down 时的积压、本 topic 的重试收据)照常落库;
    #   - 游标被清(本机单库,或带 -ConfirmFlushSharedRedis)时没有游标,按 apply_no_cursor 全部重新落库。
    # 回档要「丢弃 TargetTime 之后的写」还是「从 TargetTime 起灾难恢复重放」尚待负责人裁决(docs/design/zone_data_rollback.md、
    # db-consumer-group-per-zone.md §3(d) 与回档脚本 Step 5 现有说法不一致),本脚本只负责把位点改到计划值并复核。
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
# 成功路径显式以 0 退出,与 -FinalizeHandoff / -DeleteAndRecreateTopic 两个分支一致;失败一律 throw(R7 复核 [5])。
# 调用方(k8s_zone_rollback.ps1 Step 5 与第 0 步 d、dev_tools.ps1 kafka-offset-reset)按 $LASTEXITCODE 判定成败:不显式 exit 时,
# 它停在最后一次原生调用的退出码上 —— 「组不存在」的 describe 在有的 Kafka 版本以非 0 退出、被判为 missing 后正常返回,
# 回档就会在 PITR / FLUSHDB 之后把成功判成失败。
exit 0
