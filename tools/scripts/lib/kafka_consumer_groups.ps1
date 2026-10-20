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
    白名单命中只在**没有表头**时才认定 missing(2026-10-17,R7 复核 [8]):白名单在整段输出上匹配,管理客户端在新旧 describe API
    之间回退时可能先往 stderr 打一行带 GroupIdNotFoundException 的 WARN、随后照常打印表格。白名单命中又有表头时:
    本 topic 有数值位点即矛盾输出,throw;没有数值位点则两种读法都是「没有位点」,交给常规判定。Go 侧 groupMissingInDescribe 同口径。

    db 消费组在 K8s 上的位点读法(kubectl exec 进 kafka-0 的命令与结果分类)也只在这里一份(Get-DbGroupDescribeExecArgs /
    Resolve-DbGroupDescribeExecResult):k8s_deploy.ps1 写路径入口判组与 k8s_zone_rollback.ps1 第 0 步 e 停服前预检共用,
    两边走同一条访问路径。这两个函数只拼参数、只分类,kubectl 本身仍由调用方执行。

    回档「丢弃 TargetTime 之后的写」(负责人 2026-10-17 裁决,2026-10-18 落码,k8s_zone_rollback.ps1 Step 1b / 5 / 5b)另用到:
    --state 输出解析(ConvertFrom-KafkaGroupStateDescribe / Test-KafkaGroupStateInactive)、describe 的 CONSUMER-ID 列
    (Get-KafkaTopicActiveConsumers)、两次日志末端比对(Test-KafkaTopicQuiescent)与 Step 5 切点核对(Get-KafkaGroupCutProblems /
    ConvertTo-KafkaCutText)。--state 与 CONSUMER-ID 的原文格式未在钉版 apache/kafka:4.3.1 上实测(设计文档 §11 V3),
    按推定格式 fail-closed:格式不符只会误拒,不会误放。

    线上 go-svc-db-config 的 db 消费组形状(ConvertFrom-LiveDbConsumerGroup,2026-10-18 从 k8s_deploy.ps1 移来,回档复核 [0]):
    k8s_deploy.ps1 写路径入口判组与 k8s_zone_rollback.ps1 第 0 步 f 核对部署事实共用这一份四种形状的规则,不各写一份。
    它是本文件里唯一用到 YAML 解析的函数,依赖 lib/release_common.ps1 的 ConvertFrom-YamlToFlatMap / Remove-YamlInlineComment /
    ConvertFrom-YamlScalarLiteral:调用方须先 dot-source release_common.ps1(k8s_deploy.ps1 与 k8s_zone_rollback.ps1 都是先 release_common 后本文件)。
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
    [pscustomobject]@{ GroupMissing [bool]; HeaderFound [bool]; HasConsumerIdColumn [bool];
                       Rows = @([pscustomobject]@{ Partition [int]; CurrentOffset; LogEndOffset; Lag; ConsumerId }) }
    CurrentOffset / LogEndOffset / Lag 为 [long] 或 $null("-")。ConsumerId 是 CONSUMER-ID 列的原文("-" = 该分区没有分配给成员);
    表头没有该列、或该行短到取不到这一列时为 $null(判不出,由 Get-KafkaTopicActiveConsumers 按读不到处理)。

.DESCRIPTION
    只做语法解析,不判成败(成败见 Resolve-KafkaGroupTopicOffsets)。表头是同时含 TOPIC、PARTITION、CURRENT-OFFSET 的那一行,
    列按名字定位,列顺序变了也认;表头缺 LOG-END-OFFSET 或 LAG 时 throw。同一分区出现两行时 throw(不猜哪行生效)。
    本 topic 的行里分区号不是整数、位点不是整数也不是 "-" 时同样 throw。CONSUMER-ID 是可选列,只有回档 Step 1b 用它(2026-10-18)。
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
        $consumerId = $null
        if ($columns.ContainsKey('CONSUMER-ID') -and $tokens.Count -gt $columns['CONSUMER-ID']) {
            $consumerId = [string]$tokens[$columns['CONSUMER-ID']]
        }
        $rows.Add([pscustomobject]@{
                Partition     = $partition
                CurrentOffset = (ConvertTo-KafkaOffsetCell -Cell $tokens[$columns['CURRENT-OFFSET']] -Column 'CURRENT-OFFSET')
                LogEndOffset  = (ConvertTo-KafkaOffsetCell -Cell $tokens[$columns['LOG-END-OFFSET']] -Column 'LOG-END-OFFSET')
                Lag           = (ConvertTo-KafkaOffsetCell -Cell $tokens[$columns['LAG']] -Column 'LAG')
                ConsumerId    = $consumerId
            })
    }

    return [pscustomobject]@{
        GroupMissing        = $groupMissing
        HeaderFound         = $headerFound
        HasConsumerIdColumn = ($null -ne $columns -and $columns.ContainsKey('CONSUMER-ID'))
        Rows                = $rows.ToArray()
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
    [pscustomobject]@{ State = 'missing' | 'no-offsets' | 'offsets'; Committed = @{ [int]p = [long]offset }; Rows; HasConsumerIdColumn }
    HasConsumerIdColumn 只在找到表头时可能为 $true(见 ConvertFrom-KafkaConsumerGroupDescribe);没有表头的两种结论都没有行,恒为 $false。

.DESCRIPTION
    判定顺序(先命中先返回):
      1. 命中「组不存在」白名单:
         a. 没有表头 → missing(不论退出码:不同版本对不存在的组退出码不一致);
         b. 有表头且本 topic 有数值 CURRENT-OFFSET → throw(矛盾输出:一边说组不存在、一边列出已提交位点,不猜哪一边为真;
            当成 missing 会让 keep 去掉交接闸、显式切组跳过 F2,当成 offsets 则可能把别处的 WARN 当真);
         c. 有表头但本 topic 没有数值位点 → 不认定 missing,继续往下判(两种读法都是「没有位点」,Error 行 / 退出码照旧 throw);
      2. 有行首 "Error:" 的行,或退出码非 0 → throw;
      3. 找到表头 → 只看本 topic 的行,有数值 CURRENT-OFFSET 即 offsets,否则 no-offsets
         (组在但没人订阅本 topic、或订阅了还没提交过,都是 no-offsets);
      4. 没有表头但有 "has no active members" → no-offsets(Empty 组一个已提交位点都没有时 CLI 不打表头);
      5. 其余一律 throw(无法解析 describe 输出)。
    规则 1 与 tools/merge_zone/preflight.go 的 groupMissingInDescribe 同口径,改一边必须同步另一边。
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
        $missingParsed = ConvertFrom-KafkaConsumerGroupDescribe -Lines $allLines -Topic $Topic
        if (-not $missingParsed.HeaderFound) {
            return [pscustomobject]@{ State = 'missing'; Committed = @{}; Rows = @(); HasConsumerIdColumn = $false }
        }
        $missingCommitted = Get-KafkaCommittedOffsetMap -Parsed $missingParsed
        if ($missingCommitted.Count -gt 0) {
            $partitionsText = (@($missingCommitted.Keys | Sort-Object) -join ',')
            throw ("kafka-consumer-groups --describe 的输出自相矛盾,按读不到处理:既命中「组不存在」文案,又列出了 topic $Topic 在分区 $partitionsText 上的已提交位点。" +
                "不猜哪一边为真;拿到该 Kafka 版本的原文后同步修正白名单(lib Get-KafkaGroupMissingPatterns 与 tools/merge_zone/preflight.go consumerGroupMissingPatterns)。")
        }
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
        return [pscustomobject]@{ State = $state; Committed = $committed; Rows = $parsed.Rows; HasConsumerIdColumn = $parsed.HasConsumerIdColumn }
    }
    if (Test-KafkaGroupNoActiveMembersText -Text $text) {
        return [pscustomobject]@{ State = 'no-offsets'; Committed = @{}; Rows = @(); HasConsumerIdColumn = $false }
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
    -Partitions 必须等于 topic 的实际分区数(与 kafka_offset_reset.ps1 Get-ResetPlanProblems 同一口径):kafka-get-offsets 列出、
    或新 / 旧组位点表里出现任何 ≥ N 的分区都报问题。随后的 --delete-offsets --topic 不带分区范围,会删掉旧组在**全部**分区上的位点;
    -Partitions 手敲小了时,超出部分的分区没被检查,新组还没接手的分区的冻结续接点就此丢失。
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
    $outOfRange = @(@($low.Keys) + @($high.Keys) + @($new.Keys) + @($old.Keys) | Where-Object { [int]$_ -ge $Partitions } | ForEach-Object { [int]$_ } | Sort-Object -Unique)
    foreach ($p in $outOfRange) {
        $problems.Add("分区 ${p}:超出 -Partitions $Partitions(kafka-get-offsets 或两组的位点里出现了它);topic 的实际分区数与 -Partitions / go/db/etc/db.yaml PartitionCnt 不一致,--delete-offsets 会删掉旧组在全部分区上的位点,超出部分没有检查")
    }
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

<#
.SYNOPSIS
    -Topic 是不是 Get-DbTaskTopicName 规则下的 db_task topic(db_task_zone_<id> 或 db_task_zone_<id>_g<gen>,大小写敏感)。纯函数。
    kafka_offset_reset.ps1 -DeleteAndRecreateTopic 据此拒绝同名重建(go/db 的已落库游标按 topic 名区分、永不过期)。
#>
function Test-DbTaskTopicName {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Topic)

    return ($Topic -cmatch '^db_task_zone_\d+(_g\d+)?$')
}

# go-svc-db-config 的 db.yaml 只有四种形状(k8s_deploy.ps1 的生成器只写这四种,docs/design/db-consumer-group-per-zone.md §2):
#   final(G)      一行 `    GroupID: "G"`,G ∈ {L, Z}。任何二进制都能加载;最终形状只用它,不写 ConsumerGroup。
#   handoff(X←Y)  `    ConsumerGroup: "X"` 加 `    GroupHandoffFrom: "Y"`,{X, Y} = {L, Z},不写 GroupID。
# L = go/db/etc/db.yaml 的 ServerConfig.Kafka.GroupID(legacy 共享组,也是按 zone 推导组名的基名),Z = L + "_z" + ZoneId。
<#
.SYNOPSIS
    把线上 go-svc-db-config 里 db.yaml 的文本归到四种形状之一。返回 [pscustomobject]@{ Shape = 'final' | 'handoff'; Group; HandoffFrom }
    (final 的 HandoffFrom 为空串);不属于四种形状一律 throw。

.DESCRIPTION
    纯函数(契约测试直接喂文本)。2026-10-18 从 k8s_deploy.ps1 原样移入本库(回档复核 [0]:回档第 0 步 f 要用同一份规则判线上形状,
    不另写一份),行为不变;k8s_deploy.ps1 的 Initialize-DbConsumerGroupPlans 照旧调用它。
    GroupID / ConsumerGroup / GroupHandoffFrom 三个键按路径取:只认 ServerConfig.Kafka 的直接子键
    (go/db 只在 KafkaConfig 上定义这三个键,go/db/internal/config/config.go)。值走 release_common.ps1 的 ConvertFrom-YamlToFlatMap
    (与 k8s_deploy.ps1 Get-AuthoritativeScalar 同一套缩进、行尾注释、引号规则),所以注释行(# ConsumerGroup: …)与带前缀的同名键
    (SnapshotConsumerGroup:)都不算;去掉一层引号后必须是组名字符集。
    别的路径上的同名键(顶层 GroupID、ServerConfig.Database.GroupID、Kafka 下更深一层的 X.GroupID)一律忽略:go/db 不读它们,
    对消费组没有影响,不因此误拒;组名键若被手改写到别的段,ServerConfig.Kafka 下就一个都没有,照样 throw(fail-closed)。
    同一路径出现两次、值为空或含非法字符、组名不是 L / Z 之一、以及四种之外的任何组合,都 throw:生成器只写这四种,读到别的
    说明 ConfigMap 被手改过或来自别的工具,照着它渲染等于把未知状态当成已知(fail-closed)。
    依赖 release_common.ps1 的 ConvertFrom-YamlToFlatMap / Remove-YamlInlineComment / ConvertFrom-YamlScalarLiteral(见文件头)。
#>
function ConvertFrom-LiveDbConsumerGroup {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$DbYaml,
        [Parameter(Mandatory = $true)][string]$LegacyGroup,
        [Parameter(Mandatory = $true)][string]$ZoneGroup
    )

    $shapes = "合法形状只有四种:最终形状只写 GroupID: `"G`";交接形状只写 ConsumerGroup: `"X`" 与 GroupHandoffFrom: `"Y`"(不写 GroupID);G、X、Y 只能是 $LegacyGroup 或 ${ZoneGroup},且 X ≠ Y,三个键都在 ServerConfig.Kafka 下(docs/design/db-consumer-group-per-zone.md §2)"
    $scalars = (ConvertFrom-YamlToFlatMap -Text $DbYaml).Scalars
    # 扁平表遇到同一路径的重复键会静默取最后一处,所以出现次数另数一遍。规则与 ConvertFrom-YamlToFlatMap 逐条相同
    # (tab 归一成 4 格、去行尾注释、跳过文档分隔行、按缩进维护父路径栈、序列项只出栈不入栈),两边对同一行算出的路径一致。
    $occurrences = @{}
    $pathStack = [System.Collections.Generic.List[object]]::new()
    foreach ($rawLine in ($DbYaml -split "`r?`n")) {
        $line = Remove-YamlInlineComment -Value ($rawLine -replace "`t", '    ')
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $indent = $line.Length - $line.TrimStart(' ').Length
        $content = $line.Trim()
        if ($content.StartsWith('---') -or $content.StartsWith('...')) { continue }
        $isListItem = ($content.StartsWith('- ') -or $content -eq '-')
        $keyMatch = [regex]::Match($content, '^(?<k>[^:]+):')
        if (-not $isListItem -and -not $keyMatch.Success) { continue }
        while ($pathStack.Count -gt 0 -and $pathStack[$pathStack.Count - 1].Indent -ge $indent) { $pathStack.RemoveAt($pathStack.Count - 1) }
        if ($isListItem) { continue }
        $pathStack.Add(@{ Indent = $indent; Key = (ConvertFrom-YamlScalarLiteral -Value $keyMatch.Groups['k'].Value) })
        $path = ($pathStack | ForEach-Object { $_.Key }) -join '.'
        $occurrences[$path] = 1 + [int]$occurrences[$path]
    }

    $values = @{}
    foreach ($key in @('GroupID', 'ConsumerGroup', 'GroupHandoffFrom')) {
        $path = "ServerConfig.Kafka.$key"
        $count = [int]$occurrences[$path]
        if ($count -gt 1) {
            throw "线上 go-svc-db-config 的 db.yaml 里 $path 出现了 $count 次:重复键各 YAML 解析器行为不一,不猜哪一处生效。${shapes}。"
        }
        if (-not $scalars.Contains($path)) { continue }
        $value = [string]$scalars[$path]
        if ($value -cnotmatch '^[A-Za-z0-9._-]+$') {
            throw "线上 go-svc-db-config 的 db.yaml 里 $path 的值 '$value' 不是合法的消费组名(空值,或含 [A-Za-z0-9._-] 以外的字符)。${shapes}。"
        }
        $values[$key] = $value
    }

    $groupId = $values['GroupID']
    $consumerGroup = $values['ConsumerGroup']
    $handoffFrom = $values['GroupHandoffFrom']
    $known = @($LegacyGroup, $ZoneGroup)
    if ($null -ne $groupId) {
        if ($null -ne $consumerGroup) {
            throw "线上 go-svc-db-config 的 db.yaml 同时写了 GroupID 与 ConsumerGroup(go/db 拒启这种形状)。${shapes}。"
        }
        if ($null -ne $handoffFrom) {
            throw "线上 go-svc-db-config 的 db.yaml 是 GroupID 配 GroupHandoffFrom:go/db 拒启这种形状(交接只能写 ConsumerGroup,好让交接闸之前的二进制拒启)。${shapes}。"
        }
        if ($known -cnotcontains $groupId) {
            throw "线上 go-svc-db-config 的 db.yaml 的 GroupID=$groupId 既不是 $LegacyGroup 也不是 ${ZoneGroup},不猜它的归属。${shapes}。"
        }
        return [pscustomobject]@{ Shape = 'final'; Group = $groupId; HandoffFrom = '' }
    }
    if ($null -ne $consumerGroup) {
        if ($null -eq $handoffFrom) {
            throw "线上 go-svc-db-config 的 db.yaml 只有 ConsumerGroup、没有 GroupHandoffFrom:生成器从不写这种形状(最终形状只写 GroupID)。${shapes}。"
        }
        if ($known -cnotcontains $consumerGroup -or $known -cnotcontains $handoffFrom -or $consumerGroup -ceq $handoffFrom) {
            throw "线上 go-svc-db-config 的 db.yaml 的交接形状 ConsumerGroup=$consumerGroup GroupHandoffFrom=$handoffFrom 不是 $ZoneGroup ← $LegacyGroup 或 $LegacyGroup ← $ZoneGroup 之一。${shapes}。"
        }
        return [pscustomobject]@{ Shape = 'handoff'; Group = $consumerGroup; HandoffFrom = $handoffFrom }
    }
    if ($null -ne $handoffFrom) {
        throw "线上 go-svc-db-config 的 db.yaml 只有 GroupHandoffFrom、没有组名键。${shapes}。"
    }
    throw "线上 go-svc-db-config 的 db.yaml 里 ServerConfig.Kafka 下 GroupID / ConsumerGroup / GroupHandoffFrom 一个都没有(别的段里的同名键 go/db 不读),判不出 db 的消费组。${shapes}。"
}

<#
.SYNOPSIS
    kubectl exec 进 infra namespace 的 kafka-0(kafka 容器)describe 一个 db 消费组的 kubectl 参数(纯函数:只拼参数,不执行)。

.DESCRIPTION
    k8s_deploy.ps1 Read-DbGroupTopicFacts(写路径入口判组)与 k8s_zone_rollback.ps1 第 0 步 e(停服之前确认 Step 6 读得到同一份事实)
    共用这一份命令:同一个 kafka-0、同一个容器、同一个 CLI 与 bootstrap,回档预检过了的路径就是 Step 6 要走的路径。
    KAFKA_HEAP_OPTS 在命令里覆盖成小堆,与 kafka.yaml 探针命令同一写法:容器 env 里 broker 自己的大堆参数会被 /opt/kafka/bin/*.sh 继承,
    在 broker 的 memory limit 里再起一个大堆 JVM 会把 kafka-0 撑到 OOMKilled。所以经 /bin/sh -c 执行,组名先过字符集
    ^[A-Za-z0-9._-]+$(与 k8s_deploy.ps1 Assert-DbConsumerGroupName 同一集合),不合格即 throw。
    返回值交给调用方的 kubectl 出口(lib/k8s_client_entry.ps1 Invoke-ClientEntryKubectl,带 -KubeContext / -KubeConfig)执行。
#>
function Get-DbGroupDescribeExecArgs {
    param(
        [Parameter(Mandatory = $true)][string]$InfraNamespace,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Group
    )

    if ($Group -cnotmatch '^[A-Za-z0-9._-]+$') {
        throw "消费组名 '$Group' 不合法(空值,或含 [A-Za-z0-9._-] 以外的字符),不能拼进 kafka-0 里的 sh -c 命令行。"
    }
    $describe = "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group $Group"
    return , @('exec', '-n', $InfraNamespace, 'kafka-0', '-c', 'kafka', '--', '/bin/sh', '-c', $describe)
}

<#
.SYNOPSIS
    把 Get-DbGroupDescribeExecArgs 那次 kubectl exec 的结果(@{ ExitCode; Stdout; Stderr },Invoke-ClientEntryKubectl -CaptureOutput 的返回值)
    交给 Resolve-KafkaGroupTopicOffsets 分类,返回同一个 @{ State; Committed; Rows };判不出就 throw。纯函数。

.DESCRIPTION
    stdout 与 stderr 拆行后合并:「组不存在」那行在不同版本里可能打到任一路。exec 本身失败(缺 pods/exec 权限、kafka-0 不存在或未就绪、
    容器名不对)表现为退出码非 0 且没有表头,与「Error:」行、无法解析的输出一样按读不到 throw(fail-closed)。
#>
function Resolve-DbGroupDescribeExecResult {
    param(
        [Parameter(Mandatory = $true)]$Result,
        [Parameter(Mandatory = $true)][string]$Topic
    )

    $lines = @(([string]$Result.Stdout) -split "`r?`n") + @(([string]$Result.Stderr) -split "`r?`n")
    return (Resolve-KafkaGroupTopicOffsets -Lines $lines -ExitCode ([int]$Result.ExitCode) -Topic $Topic)
}

<#
.SYNOPSIS
    解析 kafka-consumer-groups.sh --describe --group <g> --state 的输出,返回 [pscustomobject]@{ State; Members };判不出就 throw。纯函数。

.OUTPUTS
    State = 'missing'(组不存在)或 STATE 列原文(Empty / Dead / Stable / PreparingRebalance …);Members = [int] 或 $null(missing)。

.DESCRIPTION
    表头形如 "GROUP  COORDINATOR (ID)  ASSIGNMENT-STRATEGY  STATE  #MEMBERS"。COORDINATOR (ID) 自带空格、Empty 组的 ASSIGNMENT-STRATEGY 为空,
    按空白切词后列下标会错位,所以不按下标取:表头必须以 GROUP 开头、以 "STATE #MEMBERS" 结尾,数据行从右往左取 ——
    最后一个词是成员数(非负整数),倒数第二个是状态(只含字母),第一个词必须等于 -Group(大小写敏感)。
    判定顺序(先命中先返回):
      1. 没有表头且命中「组不存在」白名单 → missing(同 Resolve-KafkaGroupTopicOffsets 规则 1a);
      2. 有行首 "Error:" 的行,或退出码非 0 → throw;
      3. 没有表头、表头末两列不是 STATE / #MEMBERS、-Group 的数据行不是恰好一行、成员数或状态不合法 → throw。
    原文格式未在钉版 apache/kafka:4.3.1 上实测(设计文档 §11 V3),按推定格式 fail-closed:格式不符只会误拒,不会误放。
#>
function ConvertFrom-KafkaGroupStateDescribe {
    param(
        [AllowEmptyCollection()][AllowNull()][string[]]$Lines,
        [Parameter(Mandatory = $true)][int]$ExitCode,
        [Parameter(Mandatory = $true)][string]$Group
    )

    $allLines = @($Lines | ForEach-Object { "$_" })
    $headerIndex = -1
    for ($i = 0; $i -lt $allLines.Count; $i++) {
        $tokens = @($allLines[$i].Trim() -split '\s+')
        if ($tokens.Count -ge 2 -and $tokens[0] -ceq 'GROUP' -and $tokens -ccontains 'STATE') {
            $headerIndex = $i
            break
        }
    }
    if ($headerIndex -lt 0 -and (Test-KafkaGroupMissingText -Text ($allLines -join "`n"))) {
        return [pscustomobject]@{ State = 'missing'; Members = $null }
    }
    $errorLines = @($allLines | Where-Object { $_ -match '^\s*Error:' })
    if ($ExitCode -ne 0 -or $errorLines.Count -gt 0) {
        $detail = if ($errorLines.Count -gt 0) { $errorLines -join ' | ' } else { (@($allLines | Where-Object { $_.Trim() -ne '' } | Select-Object -Last 5) -join ' | ') }
        throw "kafka-consumer-groups --describe --state 失败(退出码 $ExitCode),按读不到处理:$detail"
    }
    if ($headerIndex -lt 0) {
        $preview = (@($allLines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 5) -join ' | ')
        if ($preview -eq '') { $preview = '(无输出)' }
        throw "无法解析 kafka-consumer-groups --describe --state 的输出(找不到以 GROUP 开头、含 STATE 的表头),按读不到处理:$preview"
    }
    $header = @($allLines[$headerIndex].Trim() -split '\s+')
    if ($header.Count -lt 3 -or $header[-1] -cne '#MEMBERS' -or $header[-2] -cne 'STATE') {
        throw "kafka-consumer-groups --describe --state 的表头末两列不是 STATE #MEMBERS,不按下标猜:$($allLines[$headerIndex].Trim())"
    }
    $rows = [System.Collections.Generic.List[object]]::new()
    for ($i = $headerIndex + 1; $i -lt $allLines.Count; $i++) {
        $line = $allLines[$i].Trim()
        if ($line -eq '') { continue }
        $tokens = @($line -split '\s+')
        if ($tokens[0] -ceq $Group) { $rows.Add($tokens) }
    }
    if ($rows.Count -ne 1) {
        throw "kafka-consumer-groups --describe --state 的输出里组 $Group 的数据行有 $($rows.Count) 行(期望恰好 1 行),不猜哪行生效"
    }
    $row = $rows[0]
    if ($row.Count -lt 3) {
        throw "kafka-consumer-groups --describe --state 里组 $Group 的数据行太短,读不出 STATE / #MEMBERS:$($row -join ' ')"
    }
    $membersText = [string]$row[-1]
    $stateText = [string]$row[-2]
    if ($membersText -notmatch '^\d+$') {
        throw "kafka-consumer-groups --describe --state 里组 $Group 的 #MEMBERS 不是非负整数('$membersText')"
    }
    if ($stateText -cnotmatch '^[A-Za-z]+$') {
        throw "kafka-consumer-groups --describe --state 里组 $Group 的 STATE 不合法('$stateText')"
    }
    return [pscustomobject]@{ State = $stateText; Members = [int]$membersText }
}

<#
.SYNOPSIS
    ConvertFrom-KafkaGroupStateDescribe 的 State 是否表示「组里没有成员在消费」:missing / Empty / Dead(大小写敏感,原文即此写法)。纯函数。
#>
function Test-KafkaGroupStateInactive {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$State)

    return ($State -cin @('missing', 'Empty', 'Dead'))
}

<#
.SYNOPSIS
    Resolve-KafkaGroupTopicOffsets 的结果里,本 topic 哪些分区还分配给了成员(CONSUMER-ID 不是 "-"),返回 "分区 p:<CONSUMER-ID>" 列表;判不出就 throw。纯函数。

.DESCRIPTION
    没有任何行(组不存在、本 topic 没有位点也没有分配)时返回空表。有行但表头没有 CONSUMER-ID 列、或某行取不到该列时 throw:
    判不出就不能当成「没有消费者」(回档 Step 1b 据此确认 legacy 组没有成员仍在消费本 topic)。
#>
function Get-KafkaTopicActiveConsumers {
    param([Parameter(Mandatory = $true)]$Resolved)

    $rows = @($Resolved.Rows | Where-Object { $null -ne $_ })
    $active = [System.Collections.Generic.List[string]]::new()
    if ($rows.Count -eq 0) { return , $active.ToArray() }
    if (-not $Resolved.HasConsumerIdColumn) {
        throw "kafka-consumer-groups --describe 的表头没有 CONSUMER-ID 列,判不出本 topic 的分区有没有分配给活跃成员,按读不到处理"
    }
    foreach ($row in ($rows | Sort-Object Partition)) {
        if ($null -eq $row.ConsumerId -or $row.ConsumerId -eq '') {
            throw "kafka-consumer-groups --describe 里分区 $($row.Partition) 的 CONSUMER-ID 取不到,按读不到处理"
        }
        if ($row.ConsumerId -cne '-') { $active.Add("分区 $($row.Partition):$($row.ConsumerId)") }
    }
    return , $active.ToArray()
}

<#
.SYNOPSIS
    两次 kafka-get-offsets --time -1 的结果(@{ 分区 = LEO })是否完全相同且恰好覆盖 0..Partitions-1;返回问题列表 [string[]],空表示静默。纯函数。

.DESCRIPTION
    回档 Step 1b:停服之后隔 -QuiesceSeconds 读两次本 topic 的日志末端,任何一个分区变了都说明还有生产者在写本 topic
    (别的 zone 的 scene 托管本 zone 归属玩家、librdkafka 本地队列里的晚到消息、go/db saveoutbox 补投),这时拨位点、清 Redis 都不安全。
    缺分区(判不出)与超出 -Partitions 的分区(分区数与 db.yaml PartitionCnt 不一致)同样报问题。
#>
function Test-KafkaTopicQuiescent {
    param(
        [AllowNull()]$First,
        [AllowNull()]$Second,
        [Parameter(Mandatory = $true)][int]$Partitions
    )

    $problems = [System.Collections.Generic.List[string]]::new()
    if ($Partitions -le 0) {
        $problems.Add("分区数必须 > 0(实际 $Partitions)")
        return , $problems.ToArray()
    }
    $before = ConvertTo-KafkaPartitionMap -Map $First
    $after = ConvertTo-KafkaPartitionMap -Map $Second
    $outOfRange = @(@($before.Keys) + @($after.Keys) | Where-Object { [int]$_ -ge $Partitions } | ForEach-Object { [int]$_ } | Sort-Object -Unique)
    foreach ($p in $outOfRange) {
        $problems.Add("分区 ${p}:超出 -Partitions $Partitions(topic 的实际分区数与 go/db/etc/db.yaml PartitionCnt 不一致)")
    }
    for ($p = 0; $p -lt $Partitions; $p++) {
        if (-not $before.ContainsKey($p) -or -not $after.ContainsKey($p)) {
            $problems.Add("分区 ${p}:两次 kafka-get-offsets 没有都给出日志末端,判不出是否仍有写入")
            continue
        }
        if ($before[$p] -ne $after[$p]) {
            $problems.Add("分区 ${p}:日志末端从 $($before[$p]) 变为 $($after[$p]),仍有生产者在写本 topic")
        }
    }
    return , $problems.ToArray()
}

<#
.SYNOPSIS
    回档 Step 5 切点核对(纯函数):zone 组在 0..Partitions-1 每个分区都有数值 CURRENT-OFFSET 与 LOG-END-OFFSET 且两者相等(LAG = 0);
    给了 -ExpectedCut 时两者还必须都等于切点。返回问题列表 [string[]]。

.PARAMETER Rows
    Resolve-KafkaGroupTopicOffsets 的 Rows(只含本 topic)。
.PARAMETER ExpectedCut
    Step 5 之后记下的切点 @{ 分区 = 位点 };不给 = 只要求 CURRENT == LOG-END(刚拨完位点时读切点)。
#>
function Get-KafkaGroupCutProblems {
    param(
        [AllowNull()][AllowEmptyCollection()][object[]]$Rows,
        [Parameter(Mandatory = $true)][int]$Partitions,
        [AllowNull()]$ExpectedCut = $null
    )

    $problems = [System.Collections.Generic.List[string]]::new()
    if ($Partitions -le 0) {
        $problems.Add("分区数必须 > 0(实际 $Partitions)")
        return , $problems.ToArray()
    }
    $byPartition = @{}
    foreach ($row in @($Rows | Where-Object { $null -ne $_ })) { $byPartition[[int]$row.Partition] = $row }
    $cut = if ($null -ne $ExpectedCut) { ConvertTo-KafkaPartitionMap -Map $ExpectedCut } else { $null }
    foreach ($p in @($byPartition.Keys | Where-Object { [int]$_ -ge $Partitions } | Sort-Object)) {
        $problems.Add("分区 ${p}:超出 -Partitions $Partitions(topic 的实际分区数与 go/db/etc/db.yaml PartitionCnt 不一致)")
    }
    for ($p = 0; $p -lt $Partitions; $p++) {
        if (-not $byPartition.ContainsKey($p)) {
            $problems.Add("分区 ${p}:zone 组在本 topic 上没有这一行(没有已提交位点)")
            continue
        }
        $row = $byPartition[$p]
        if ($null -eq $row.CurrentOffset -or $null -eq $row.LogEndOffset) {
            $problems.Add("分区 ${p}:CURRENT-OFFSET / LOG-END-OFFSET 不是数值(current=$($row.CurrentOffset) end=$($row.LogEndOffset))")
            continue
        }
        if ([long]$row.CurrentOffset -ne [long]$row.LogEndOffset) {
            $problems.Add("分区 ${p}:CURRENT-OFFSET $($row.CurrentOffset) 不等于 LOG-END-OFFSET $($row.LogEndOffset),切点之后又有消息写入")
            continue
        }
        if ($null -ne $cut) {
            if (-not $cut.ContainsKey($p)) {
                $problems.Add("分区 ${p}:Step 5 的切点里没有这个分区")
            }
            elseif ([long]$row.LogEndOffset -ne $cut[$p]) {
                $problems.Add("分区 ${p}:位点 / 日志末端 $($row.LogEndOffset) 偏离 Step 5 的切点 $($cut[$p])")
            }
        }
    }
    return , $problems.ToArray()
}

<#
.SYNOPSIS
    切点 @{ 分区 = 位点 } 的文本形式 "0=123,1=456"(按分区号升序),交给回档工具 verify 的 -kafka-cut 与报告。纯函数。
#>
function ConvertTo-KafkaCutText {
    param([AllowNull()]$Cut)

    $map = ConvertTo-KafkaPartitionMap -Map $Cut
    return ((@($map.Keys) | Sort-Object | ForEach-Object { "$_=$($map[$_])" }) -join ',')
}
