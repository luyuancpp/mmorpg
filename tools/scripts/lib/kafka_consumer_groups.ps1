#requires -Version 7
<#
.SYNOPSIS
    kafka-consumer-groups.sh / kafka-get-offsets.sh 输出的解析与 db_task 消费组交接判定(纯函数库)。

.DESCRIPTION
    go/db 存盘消费组按 zone 拆分(db_rpc_consumer_group → db_rpc_consumer_group_z<ZoneId>,
    docs/design/db-consumer-group-per-zone.md)之后,回档、交接收尾与部署脚本都要回答同一类问题:
    「某个组在某个 topic 上有没有已提交位点、是多少」。这里是唯一一份解析规则,
    kafka_offset_reset.ps1、k8s_zone_rollback.ps1 与 k8s_deploy.ps1 都 dot-source 本文件,不各写一份。

    约束:
      - 只定义函数,不读任何脚本级变量,不设 StrictMode / ErrorActionPreference,dot-source 无副作用;
        不调用任何外部命令 —— 执行 CLI、捕获输出与退出码由调用方负责,这里只做判定。
      - 判不出一律 throw(fail-closed):无表头、出现 "Error:" 行、退出码非 0 且不是「组不存在」,都不当成"没有位点"。
        把"读不到"当成"没有",回档会把位点删光、交接会从 Oldest 全量重放。
      - 表格按表头列名定位(TOPIC / PARTITION / CURRENT-OFFSET / LOG-END-OFFSET / LAG / NEW-OFFSET),不按下标;
        "-" 记为 $null(该分区没有已提交位点)。

    「组不存在」的识别是白名单,与 tools/merge_zone/preflight.go 的 consumerGroupMissing 同一份,改一边必须同步另一边。
    当前三条文案(Kafka 3.x 的 "Consumer group 'g' does not exist."、新 group coordinator 可能给出的
    GroupIdNotFoundException / GROUP_ID_NOT_FOUND)是按版本行为推定的;钉版 apache/kafka:4.3.1 上的原文
    由 Codex 实测(设计文档 §11 的 V3 (b)(h)(i)),拿到后两边同步补正则,并把原文追加为测试 fixture。
    原文一条都不命中时现有逻辑已是 fail-closed(无表头 → throw),只是会把"组不存在"误报成失败。
#>

<#
.SYNOPSIS
    「组不存在」文案白名单(正则)。与 tools/merge_zone/preflight.go consumerGroupMissing 保持一致。
#>
function Get-KafkaGroupMissingPatterns {
    return @(
        "(?i)consumer group '[^']*' does not exist",
        'GroupIdNotFoundException',
        'GROUP_ID_NOT_FOUND'
    )
}

<#
.SYNOPSIS
    输出文本是否表明「消费组不存在」。命中任一白名单正则即为 $true。
#>
function Test-KafkaGroupMissingText {
    param([AllowEmptyString()][AllowNull()][string]$Text)

    if ([string]::IsNullOrEmpty($Text)) { return $false }
    foreach ($pattern in (Get-KafkaGroupMissingPatterns)) {
        if ($Text -match $pattern) { return $true }
    }
    return $false
}

<#
.SYNOPSIS
    Empty 组(没有成员)且一个已提交位点都没有时,CLI 只打这一行、不打表头。
#>
function Test-KafkaGroupNoActiveMembersText {
    param([AllowEmptyString()][AllowNull()][string]$Text)

    if ([string]::IsNullOrEmpty($Text)) { return $false }
    return ($Text -match "(?i)consumer group '[^']*' has no active members")
}

# 表格里的一个位点单元格:"-" → $null;整数 → [long];其它 → throw(不猜)。
function ConvertTo-KafkaOffsetCell {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Cell,
        [Parameter(Mandatory = $true)][string]$Column
    )

    if ($Cell -eq '-') { return $null }
    if ($Cell -match '^-?\d+$') { return [long]$Cell }
    throw "无法解析 kafka 输出里 $Column 列的值 '$Cell'(既不是整数也不是 '-'),按失败处理"
}

<#
.SYNOPSIS
    解析 kafka-consumer-groups.sh --describe --group <g> 的输出,只保留 TOPIC == -Topic 的行。

.OUTPUTS
    [pscustomobject]@{ GroupMissing [bool]; HeaderFound [bool]; Rows = @([pscustomobject]@{ Partition [int]; CurrentOffset; LogEndOffset; Lag }) }
    CurrentOffset / LogEndOffset / Lag 为 [long] 或 $null("-")。

.DESCRIPTION
    只做语法解析,不判成败(成败见 Resolve-KafkaGroupTopicOffsets)。表头是同时含 TOPIC、PARTITION、CURRENT-OFFSET 的那一行,
    列按名字定位,列顺序变了也认;表头缺 LOG-END-OFFSET 或 LAG 时 throw。同一分区出现两行时 throw(不猜哪行生效)。
    本 topic 的行里分区号不是整数、位点不是整数也不是 "-" 时同样 throw。
#>
function ConvertFrom-KafkaConsumerGroupDescribe {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$Topic
    )

    $allLines = @($Lines | ForEach-Object { "$_" })
    $groupMissing = Test-KafkaGroupMissingText -Text ($allLines -join "`n")
    $required = @('TOPIC', 'PARTITION', 'CURRENT-OFFSET', 'LOG-END-OFFSET', 'LAG')
    $columns = $null
    $headerFound = $false
    $rows = [System.Collections.Generic.List[object]]::new()
    $seen = @{}

    foreach ($raw in $allLines) {
        $line = $raw.Trim()
        if ($line -eq '') { continue }
        $tokens = @($line -split '\s+')
        if ($tokens -ccontains 'TOPIC' -and $tokens -ccontains 'PARTITION' -and $tokens -ccontains 'CURRENT-OFFSET') {
            $columns = @{}
            for ($i = 0; $i -lt $tokens.Count; $i++) { $columns[$tokens[$i]] = $i }
            foreach ($name in $required) {
                if (-not $columns.ContainsKey($name)) {
                    throw "kafka-consumer-groups --describe 的表头缺少 $name 列,无法解析:$line"
                }
            }
            $headerFound = $true
            continue
        }
        if ($null -eq $columns) { continue }

        $maxIndex = ($required | ForEach-Object { $columns[$_] } | Measure-Object -Maximum).Maximum
        if ($tokens.Count -le $maxIndex) { continue }
        if ($tokens[$columns['TOPIC']] -cne $Topic) { continue }

        $partitionText = $tokens[$columns['PARTITION']]
        if ($partitionText -notmatch '^\d+$') {
            throw "kafka-consumer-groups --describe 里 topic $Topic 的行分区号不是整数('$partitionText'),无法解析:$line"
        }
        $partition = [int]$partitionText
        if ($seen.ContainsKey($partition)) {
            throw "kafka-consumer-groups --describe 里 topic $Topic 的分区 $partition 出现了两行,不猜哪行生效"
        }
        $seen[$partition] = $true
        $rows.Add([pscustomobject]@{
                Partition     = $partition
                CurrentOffset = (ConvertTo-KafkaOffsetCell -Cell $tokens[$columns['CURRENT-OFFSET']] -Column 'CURRENT-OFFSET')
                LogEndOffset  = (ConvertTo-KafkaOffsetCell -Cell $tokens[$columns['LOG-END-OFFSET']] -Column 'LOG-END-OFFSET')
                Lag           = (ConvertTo-KafkaOffsetCell -Cell $tokens[$columns['LAG']] -Column 'LAG')
            })
    }

    return [pscustomobject]@{
        GroupMissing = $groupMissing
        HeaderFound  = $headerFound
        Rows         = $rows.ToArray()
    }
}

<#
.SYNOPSIS
    从 ConvertFrom-KafkaConsumerGroupDescribe 的结果取已提交位点:@{ [int]分区 = [long]位点 },只含 CURRENT-OFFSET 为数值的分区。
#>
function Get-KafkaCommittedOffsetMap {
    param([Parameter(Mandatory = $true)]$Parsed)

    $map = @{}
    foreach ($row in @($Parsed.Rows)) {
        if ($null -ne $row -and $null -ne $row.CurrentOffset) {
            $map[[int]$row.Partition] = [long]$row.CurrentOffset
        }
    }
    return $map
}

<#
.SYNOPSIS
    把一次 describe 的输出与退出码分类为:missing(组不存在)/ no-offsets(组在、本 topic 没有数值位点)/ offsets;判不出就 throw。

.OUTPUTS
    [pscustomobject]@{ State = 'missing' | 'no-offsets' | 'offsets'; Committed = @{ [int]p = [long]offset }; Rows }

.DESCRIPTION
    判定顺序(先命中先返回):
      1. 命中「组不存在」白名单 → missing(不论退出码:不同版本对不存在的组退出码不一致);
      2. 有行首 "Error:" 的行,或退出码非 0 → throw;
      3. 找到表头 → 只看本 topic 的行,有数值 CURRENT-OFFSET 即 offsets,否则 no-offsets
         (组在但没人订阅本 topic、或订阅了还没提交过,都是 no-offsets);
      4. 没有表头但有 "has no active members" → no-offsets(Empty 组一个已提交位点都没有时 CLI 不打表头);
      5. 其余一律 throw(无法解析 describe 输出)。
#>
function Resolve-KafkaGroupTopicOffsets {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][int]$ExitCode,
        [Parameter(Mandatory = $true)][string]$Topic
    )

    $allLines = @($Lines | ForEach-Object { "$_" })
    $text = $allLines -join "`n"
    if (Test-KafkaGroupMissingText -Text $text) {
        return [pscustomobject]@{ State = 'missing'; Committed = @{}; Rows = @() }
    }
    $errorLines = @($allLines | Where-Object { $_ -match '^\s*Error:' })
    if ($ExitCode -ne 0 -or $errorLines.Count -gt 0) {
        $detail = if ($errorLines.Count -gt 0) { $errorLines -join ' | ' } else { (@($allLines | Where-Object { $_.Trim() -ne '' } | Select-Object -Last 5) -join ' | ') }
        throw "kafka-consumer-groups --describe 失败(退出码 $ExitCode),按读不到处理:$detail"
    }

    $parsed = ConvertFrom-KafkaConsumerGroupDescribe -Lines $allLines -Topic $Topic
    if ($parsed.HeaderFound) {
        $committed = Get-KafkaCommittedOffsetMap -Parsed $parsed
        $state = if ($committed.Count -gt 0) { 'offsets' } else { 'no-offsets' }
        return [pscustomobject]@{ State = $state; Committed = $committed; Rows = $parsed.Rows }
    }
    if (Test-KafkaGroupNoActiveMembersText -Text $text) {
        return [pscustomobject]@{ State = 'no-offsets'; Committed = @{}; Rows = @() }
    }
    $preview = (@($allLines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 5) -join ' | ')
    if ($preview -eq '') { $preview = '(无输出)' }
    throw "无法解析 kafka-consumer-groups --describe 的输出(既没有表头,也不是「组不存在」或「没有活跃成员」),按读不到处理:$preview"
}

<#
.SYNOPSIS
    解析 kafka-consumer-groups.sh --reset-offsets(--dry-run 或 --execute)打印的计划表,返回 @{ [int]分区 = [long]NEW-OFFSET },只含 -Topic 的行。

.DESCRIPTION
    表头是同时含 TOPIC、PARTITION、NEW-OFFSET 的那一行。找不到表头、本 topic 一行都没有、NEW-OFFSET 不是非负整数、
    同一分区出现两行,都 throw:计划解析不出来就没法在 -Apply 之后逐分区复核,不能当成"计划为空"放行。
#>
function ConvertFrom-KafkaResetPlan {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$Topic
    )

    $columns = $null
    $plan = @{}
    foreach ($raw in @($Lines | ForEach-Object { "$_" })) {
        $line = $raw.Trim()
        if ($line -eq '') { continue }
        $tokens = @($line -split '\s+')
        if ($tokens -ccontains 'TOPIC' -and $tokens -ccontains 'PARTITION' -and $tokens -ccontains 'NEW-OFFSET') {
            $columns = @{}
            for ($i = 0; $i -lt $tokens.Count; $i++) { $columns[$tokens[$i]] = $i }
            continue
        }
        if ($null -eq $columns) { continue }
        $maxIndex = (@($columns['TOPIC'], $columns['PARTITION'], $columns['NEW-OFFSET']) | Measure-Object -Maximum).Maximum
        if ($tokens.Count -le $maxIndex) { continue }
        if ($tokens[$columns['TOPIC']] -cne $Topic) { continue }
        $partitionText = $tokens[$columns['PARTITION']]
        $offsetText = $tokens[$columns['NEW-OFFSET']]
        if ($partitionText -notmatch '^\d+$') {
            throw "重置计划里 topic $Topic 的分区号不是整数('$partitionText'):$line"
        }
        if ($offsetText -notmatch '^\d+$') {
            throw "重置计划里 topic $Topic 分区 $partitionText 的 NEW-OFFSET 不是数值('$offsetText'):$line"
        }
        $partition = [int]$partitionText
        if ($plan.ContainsKey($partition)) {
            throw "重置计划里 topic $Topic 的分区 $partition 出现了两行,不猜哪行生效"
        }
        $plan[$partition] = [long]$offsetText
    }
    if ($null -eq $columns) {
        throw "kafka-consumer-groups --reset-offsets 的输出里找不到 TOPIC / PARTITION / NEW-OFFSET 表头,无法得到重置计划"
    }
    if ($plan.Count -eq 0) {
        throw "kafka-consumer-groups --reset-offsets 的计划里没有 topic $Topic 的任何分区"
    }
    return $plan
}

<#
.SYNOPSIS
    解析 kafka-get-offsets.sh --topic <t> --time -2 | -1 的输出("topic:partition:offset" 每行一条),返回 @{ [int]分区 = [long]位点 }。

.DESCRIPTION
    只收 topic 名与 -Topic 完全相同的行;一行都没有就 throw。偏移为空(如 "t:0:")的行不匹配,
    该分区在结果里缺席,由调用方按"缺 earliest / latest"报问题(fail-closed)。
#>
function ConvertFrom-KafkaGetOffsets {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$Topic
    )

    $map = @{}
    foreach ($raw in @($Lines | ForEach-Object { "$_" })) {
        $line = $raw.Trim()
        if ($line -match '^(?<t>.+):(?<p>\d+):(?<o>-?\d+)$' -and $Matches['t'] -ceq $Topic) {
            $map[[int]$Matches['p']] = [long]$Matches['o']
        }
    }
    if ($map.Count -eq 0) {
        throw "kafka-get-offsets 的输出里没有 topic $Topic 的任何分区(期望每行形如 '${Topic}:<分区>:<位点>')"
    }
    return $map
}

# 把调用方给的位点表规整成 [int] 键、[long] 值;$null 视为空表。
function ConvertTo-KafkaPartitionMap {
    param([AllowNull()]$Map)

    $result = @{}
    if ($null -eq $Map) { return $result }
    foreach ($key in @($Map.Keys)) {
        $result[[int]$key] = [long]$Map[$key]
    }
    return $result
}

<#
.SYNOPSIS
    交接收尾(kafka_offset_reset.ps1 -FinalizeHandoff)的前置检查:返回问题列表 [string[]],空表示可以删除旧组在本 topic 上的位点。

.DESCRIPTION
    对每个分区 p ∈ 0..Partitions-1:
      - earliest 或 latest 缺 p → 问题(判不出是不是空分区);
      - 新组在 p 上没有已提交位点:只有 earliest == latest(从未写入或已全部过期)才放行 —— 这类分区从 Oldest 起步
        与从旧组位点续接等价,不会跳过消息;sarama 只提交被 MarkOffset 推进过的分区,空分区永远不会有位点。
        否则是问题(新组还没接手这个分区,删掉旧组位点后就再也找不回续接点);
      - 新组与旧组都有位点、且新组 < 旧组 → 问题(新组落后于旧组,删掉旧组位点会丢掉这段进度,也说明交接方向可能弄反了)。
    旧组在 p 上没有位点时不比较。
#>
function Test-HandoffFinalizeReady {
    param(
        [AllowNull()]$NewOffsets,
        [AllowNull()]$OldOffsets,
        [AllowNull()]$Earliest,
        [AllowNull()]$Latest,
        [Parameter(Mandatory = $true)][int]$Partitions
    )

    $problems = [System.Collections.Generic.List[string]]::new()
    if ($Partitions -le 0) {
        $problems.Add("分区数必须 > 0(实际 $Partitions)")
        return , $problems.ToArray()
    }
    $new = ConvertTo-KafkaPartitionMap -Map $NewOffsets
    $old = ConvertTo-KafkaPartitionMap -Map $OldOffsets
    $low = ConvertTo-KafkaPartitionMap -Map $Earliest
    $high = ConvertTo-KafkaPartitionMap -Map $Latest
    for ($p = 0; $p -lt $Partitions; $p++) {
        if (-not $low.ContainsKey($p) -or -not $high.ContainsKey($p)) {
            $problems.Add("分区 ${p}:kafka-get-offsets 没有给出 earliest / latest,判不出是不是空分区")
            continue
        }
        if (-not $new.ContainsKey($p)) {
            if ($low[$p] -ne $high[$p]) {
                $problems.Add("分区 ${p}:新组没有已提交位点,而分区非空(earliest=$($low[$p]) latest=$($high[$p]));新组还没接手这个分区")
            }
            continue
        }
        if ($old.ContainsKey($p) -and $new[$p] -lt $old[$p]) {
            $problems.Add("分区 ${p}:新组位点 $($new[$p]) 小于旧组位点 $($old[$p])")
        }
    }
    return , $problems.ToArray()
}

<#
.SYNOPSIS
    db_task topic 名:世代号 ≤ 1 为 db_task_zone_<id>,否则 db_task_zone_<id>_g<gen>。
    与 go/db/internal/config/config.go DbTaskTopicForGeneration(及 go/login 同名函数)同规则,改一边必须同步另一边。
#>
function Get-DbTaskTopicName {
    param(
        [Parameter(Mandatory = $true)][int]$ZoneId,
        [Parameter(Mandatory = $true)][int]$Generation
    )

    $base = "db_task_zone_$ZoneId"
    if ($Generation -le 1) { return $base }
    return "${base}_g$Generation"
}
