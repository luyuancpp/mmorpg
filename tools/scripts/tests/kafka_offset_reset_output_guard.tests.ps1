#requires -Version 7
<#
.SYNOPSIS
    kafka_offset_reset.ps1 位点重置结果判定的契约测试(2026-10-12)。

.DESCRIPTION
    背景(docs/design/db-task-pipeline-bottlenecks.md 修复项 ⓪):对仍有活跃成员的消费组执行 --reset-offsets 时,
    kafka-consumer-groups.sh 只打印 "Error: Assignments can only be reset if the group ... is inactive ..."
    而退出码可能仍为 0,位点没动。K8s 上所有 zone 共用 db_rpc_consumer_group,单 zone 回档时这是常态,
    旧脚本照样打印 OK。

    本套钉住:
      - 退出码非 0:失败;
      - 退出码 0 但任一行以 "Error:" 开头(允许前导空白):失败,错误文本带上原始错误行;
      - 正常的计划表 / 执行结果输出:放行;
      - 行中间出现 error 字样(不是行首的 "Error:"):放行;
      - 脚本在调用 kafka-consumer-groups 之后确实走这道判定,且 stderr 一并收集。

    2026-10-15 起(go/db 消费组按 zone 拆分,docs/design/db-consumer-group-per-zone.md)另钉住:
      - lib/kafka_consumer_groups.ps1 的解析:describe 按表头列名定位、只取本 topic、"-" 记 null;
        「组不存在」白名单(does not exist / GroupIdNotFoundException / GROUP_ID_NOT_FOUND)、无表头 + has no active members
        判为 no-offsets、Error 行与无法解析的输出一律 throw;重置计划表、kafka-get-offsets 输出的解析;
      - 交接收尾前置检查 Test-HandoffFinalizeReady(空分区 earliest == latest 放行、新组落后于旧组报问题);
      - 重置计划覆盖检查与 -Apply 后逐分区复核(复核失败的 throw 在删除 RetireGroup 位点之前);
      - Remove-RetiredGroupTopicOffsets:旧组无位点不删、不带 -Apply 只打印、删后再 describe 复核,仍有位点即失败;
      - 参数互斥:-FinalizeHandoff 与其它模式互斥,缺 -RetireGroup / -Partitions 拒绝;-RetireGroup 缺 -Partitions、
        等于 -Group、配 -DeleteAndRecreateTopic 都拒绝(子进程只跑到参数校验,不连 Kafka)。

    判定函数按 AST 取出执行,Kafka CLI 用本文件里的假函数顶替,不连任何 Kafka。
    「组不存在」等文案是按版本行为推定的;钉版 apache/kafka:4.3.1 的原文由 Codex 实测后(设计文档 §11 V3)追加为 fixture。

.EXAMPLE
    pwsh -File tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1
#>

$ErrorActionPreference = "Stop"

. "$PSScriptRoot/lib/test_harness.ps1"

Write-Host ""
Write-Host "=== kafka_offset_reset.ps1 offset reset result contract tests ==="

$scriptPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'kafka_offset_reset.ps1'
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($scriptPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "kafka_offset_reset.ps1 解析失败: $($parseErrors | Out-String)" }
$guard = @($ast.FindAll({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Assert-KafkaToolOutputClean' }, $true))
if ($guard.Count -ne 1) { throw "kafka_offset_reset.ps1 里应有且只有一个 Assert-KafkaToolOutputClean,实际 $($guard.Count) 个" }
. ([scriptblock]::Create($guard[0].Extent.Text))

function Get-ThrownMessage {
    param([Parameter(Mandatory = $true)][scriptblock]$Body)
    try { & $Body; return $null } catch { return $_.Exception.Message }
}

$activeGroupError = "Error: Assignments can only be reset if the group 'db_rpc_consumer_group' is inactive, but the current state is Stable."

Test-Case '退出码 0 但输出 Error:(活跃组不能重置):失败,并带上原始错误行' {
    $lines = @('', $activeGroupError, '')
    $message = Get-ThrownMessage { Assert-KafkaToolOutputClean -Lines $lines -ExitCode 0 -Tool 'kafka-consumer-groups' }
    Assert-True -Condition ($null -ne $message) -Because '位点没动却放行,就是这次要修的静默失败'
    Assert-Match -Text $message -Pattern '位点没有重置' -Because '错误文本要说明位点没有重置'
    Assert-Match -Text $message -Pattern 'current state is Stable' -Because '要带上 Kafka 原始错误,方便定位'
}

Test-Case '"Error:" 前有空白也算失败' {
    $message = Get-ThrownMessage { Assert-KafkaToolOutputClean -Lines @("   $activeGroupError") -ExitCode 0 -Tool 'kafka-consumer-groups' }
    Assert-True -Condition ($null -ne $message) -Because 'Kafka 工具常在错误行前打空行或缩进'
}

Test-Case '退出码非 0:失败' {
    $message = Get-ThrownMessage { Assert-KafkaToolOutputClean -Lines @() -ExitCode 1 -Tool 'kafka-consumer-groups' }
    Assert-True -Condition ($null -ne $message) -Because '退出码非 0 必须失败'
    Assert-Match -Text $message -Pattern 'exit code 1' -Because '错误文本要带上退出码'
}

Test-Case '正常的计划表输出:放行' {
    $lines = @(
        '',
        'GROUP                          TOPIC                          PARTITION  NEW-OFFSET',
        'db_rpc_consumer_group_z1       db_task_zone_1_g1              0          12345',
        'db_rpc_consumer_group_z1       db_task_zone_1_g1              1          23456'
    )
    $message = Get-ThrownMessage { Assert-KafkaToolOutputClean -Lines $lines -ExitCode 0 -Tool 'kafka-consumer-groups' }
    Assert-True -Condition ($null -eq $message) -Because "正常输出不应被拒。错误: $message"
}

Test-Case '没有任何输出且退出码 0:放行' {
    $message = Get-ThrownMessage { Assert-KafkaToolOutputClean -Lines @() -ExitCode 0 -Tool 'kafka-consumer-groups' }
    Assert-True -Condition ($null -eq $message) -Because "空输出不是错误。错误: $message"
}

Test-Case '行中间出现 error 字样:放行(只认行首的 Error:)' {
    $lines = @('[2026-10-12 03:00:00,000] WARN [AdminClient] Connection error, retrying (org.apache.kafka.clients.NetworkClient)')
    $message = Get-ThrownMessage { Assert-KafkaToolOutputClean -Lines $lines -ExitCode 0 -Tool 'kafka-consumer-groups' }
    Assert-True -Condition ($null -eq $message) -Because "重试类告警不代表重置失败,不应误拒。错误: $message"
}

Test-Case '脚本调用 kafka-consumer-groups 后走这道判定,且收集 stderr' {
    $text = Get-Content $scriptPath -Raw
    Assert-Match -Text $text -Pattern '& \$cgTool @cmdArgs 2>&1' -Because 'Error: 行可能走 stderr,必须一并收集'
    Assert-Match -Text $text -Pattern 'Assert-KafkaToolOutputClean -Lines \$cgOutput -ExitCode \$exitCode' -Because '调用之后必须判定输出'
}

# ─────────────────────────────────────────────────────────────────
# lib/kafka_consumer_groups.ps1 与 db_task 消费组交接收尾(2026-10-15)
# ─────────────────────────────────────────────────────────────────

. "$PSScriptRoot/lib/deploy_capture.ps1"
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'lib' 'kafka_consumer_groups.ps1')

$resetFunctions = @('Invoke-KafkaToolCaptured', 'Invoke-KafkaConsumerGroupDescribe', 'Invoke-KafkaGetOffsets',
    'Get-ResetPlanProblems', 'Get-ResetVerificationProblems', 'Remove-RetiredGroupTopicOffsets')
$found = @($ast.FindAll({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -in $resetFunctions }, $true))
foreach ($name in $resetFunctions) {
    $matched = @($found | Where-Object { $_.Name -eq $name })
    if ($matched.Count -ne 1) { throw "kafka_offset_reset.ps1 里应有且只有一个 $name,实际 $($matched.Count) 个" }
    . ([scriptblock]::Create($matched[0].Extent.Text))
}

$Topic = 'db_task_zone_101'
$DescribeHeader = 'GROUP                      TOPIC             PARTITION  CURRENT-OFFSET  LOG-END-OFFSET  LAG   CONSUMER-ID  HOST  CLIENT-ID'

# 拼一行 describe 表格行(列顺序同 $DescribeHeader)。
function New-DescribeRow {
    param([string]$Group = 'g', [string]$RowTopic = $Topic, [int]$Partition, [string]$Current, [string]$End, [string]$Lag)
    return "$Group  $RowTopic  $Partition  $Current  $End  $Lag  -  -  -"
}

Test-Case 'describe 解析:按表头列名定位、只取本 topic、"-" 记为 null' {
    $lines = @(
        "Consumer group 'g' has no active members.",
        '',
        $DescribeHeader,
        (New-DescribeRow -Partition 0 -Current '10' -End '15' -Lag '5'),
        (New-DescribeRow -Partition 1 -Current '-' -End '7' -Lag '-'),
        (New-DescribeRow -RowTopic 'db_task_zone_102' -Partition 0 -Current '99' -End '100' -Lag '1')
    )
    $parsed = ConvertFrom-KafkaConsumerGroupDescribe -Lines $lines -Topic $Topic
    Assert-True -Condition $parsed.HeaderFound -Because '"has no active members" 行之后的表头要能识别'
    Assert-True -Condition (-not $parsed.GroupMissing) -Because 'has no active members 不是组不存在'
    Assert-Equal -Expected 2 -Actual @($parsed.Rows).Count -Because '别的 topic 的行不计入'
    $p1 = @($parsed.Rows | Where-Object { $_.Partition -eq 1 })[0]
    Assert-True -Condition ($null -eq $p1.CurrentOffset -and $null -eq $p1.Lag) -Because '"-" 表示没有已提交位点,必须记为 null,不能当成 0'
    $map = Get-KafkaCommittedOffsetMap -Parsed $parsed
    Assert-Equal -Expected 1 -Actual $map.Count -Because '只有数值 CURRENT-OFFSET 的分区算已提交'
    Assert-Equal -Expected 10 -Actual $map[0] -Because '分区 0 的已提交位点'
}

Test-Case 'describe 解析:列顺序打乱也按表头识别' {
    $lines = @(
        'LAG  PARTITION  TOPIC  GROUP  LOG-END-OFFSET  CURRENT-OFFSET',
        "3  4  $Topic  g  50  47"
    )
    $parsed = ConvertFrom-KafkaConsumerGroupDescribe -Lines $lines -Topic $Topic
    $row = @($parsed.Rows)[0]
    Assert-Equal -Expected 4 -Actual $row.Partition -Because '分区按 PARTITION 列取,不按下标'
    Assert-Equal -Expected 47 -Actual $row.CurrentOffset -Because 'CURRENT-OFFSET 按列名取'
    Assert-Equal -Expected 50 -Actual $row.LogEndOffset -Because 'LOG-END-OFFSET 按列名取'
    Assert-Equal -Expected 3 -Actual $row.Lag -Because 'LAG 按列名取'
}

Test-Case 'describe 解析:"Error: Consumer group ''x'' does not exist." 判为 GroupMissing' {
    $parsed = ConvertFrom-KafkaConsumerGroupDescribe -Lines @('', "Error: Consumer group 'db_rpc_consumer_group_z101' does not exist.") -Topic $Topic
    Assert-True -Condition $parsed.GroupMissing -Because 'Kafka 3.x 对不存在的组打印这一行'
    Assert-True -Condition (-not $parsed.HeaderFound) -Because '没有表头'
}

Test-Case 'Resolve-KafkaGroupTopicOffsets:组不存在的两种文案都判 missing(不论退出码)' {
    $cases = @(
        @{ Lines = @("Error: Consumer group 'g' does not exist."); Exit = 0 },
        @{ Lines = @('Error: Executing consumer group command failed due to org.apache.kafka.common.errors.GroupIdNotFoundException: Group g not found.'); Exit = 1 },
        @{ Lines = @('Error: GROUP_ID_NOT_FOUND'); Exit = 1 }
    )
    foreach ($c in $cases) {
        $state = Resolve-KafkaGroupTopicOffsets -Lines $c.Lines -ExitCode $c.Exit -Topic $Topic
        Assert-Equal -Expected 'missing' -Actual $state.State -Because "白名单文案 '$($c.Lines[0])' 必须判为组不存在"
        Assert-Equal -Expected 0 -Actual $state.Committed.Count -Because '组不存在时没有任何位点'
    }
}

Test-Case 'Resolve-KafkaGroupTopicOffsets:无表头 + has no active members 判 no-offsets;有表头但本 topic 只有 "-" 也是 no-offsets' {
    $empty = Resolve-KafkaGroupTopicOffsets -Lines @('', "Consumer group 'g' has no active members.") -ExitCode 0 -Topic $Topic
    Assert-Equal -Expected 'no-offsets' -Actual $empty.State -Because 'Empty 组一个位点都没有时 CLI 不打表头'
    $dashOnly = Resolve-KafkaGroupTopicOffsets -Lines @($DescribeHeader, (New-DescribeRow -Partition 0 -Current '-' -End '5' -Lag '-')) -ExitCode 0 -Topic $Topic
    Assert-Equal -Expected 'no-offsets' -Actual $dashOnly.State -Because '分配了但还没提交过,不算有位点'
    $other = Resolve-KafkaGroupTopicOffsets -Lines @($DescribeHeader, (New-DescribeRow -RowTopic 'other_topic' -Partition 0 -Current '5' -End '5' -Lag '0')) -ExitCode 0 -Topic $Topic
    Assert-Equal -Expected 'no-offsets' -Actual $other.State -Because '组只在别的 topic 上有位点,本 topic 视为没有'
    $held = Resolve-KafkaGroupTopicOffsets -Lines @($DescribeHeader, (New-DescribeRow -Partition 2 -Current '8' -End '9' -Lag '1')) -ExitCode 0 -Topic $Topic
    Assert-Equal -Expected 'offsets' -Actual $held.State -Because '本 topic 有数值位点'
    Assert-Equal -Expected 8 -Actual $held.Committed[2] -Because 'Committed 以分区号为键'
}

Test-Case 'Resolve-KafkaGroupTopicOffsets:Error 行、退出码非 0、无表头无关键字一律 throw(fail-closed)' {
    $cases = @(
        @{ Label = 'Error 行'; Lines = @('Error: Executing consumer group command failed due to org.apache.kafka.common.errors.TimeoutException'); Exit = 0 },
        @{ Label = '退出码非 0'; Lines = @($DescribeHeader, (New-DescribeRow -Partition 0 -Current '1' -End '1' -Lag '0')); Exit = 1 },
        @{ Label = '无表头无关键字'; Lines = @('Warning: Consumer group ''g'' is rebalancing.'); Exit = 0 },
        @{ Label = '无输出'; Lines = @(); Exit = 0 }
    )
    foreach ($c in $cases) {
        $message = Get-ThrownMessage { Resolve-KafkaGroupTopicOffsets -Lines $c.Lines -ExitCode $c.Exit -Topic $Topic }
        Assert-True -Condition ($null -ne $message) -Because "$($c.Label):判不出就不能当成「没有位点」,否则回档 / 收尾会删光位点"
    }
}

Test-Case 'ConvertFrom-KafkaResetPlan:按 NEW-OFFSET 表头解析,只取本 topic;缺表头、非数值、本 topic 无行都 throw' {
    $lines = @(
        'GROUP                          TOPIC                          PARTITION  NEW-OFFSET',
        "db_rpc_consumer_group_z101     $Topic               0          12345",
        "db_rpc_consumer_group_z101     $Topic               1          23456",
        'db_rpc_consumer_group_z101     db_task_zone_102               0          1'
    )
    $plan = ConvertFrom-KafkaResetPlan -Lines $lines -Topic $Topic
    Assert-Equal -Expected 2 -Actual $plan.Count -Because '别的 topic 不计入'
    Assert-Equal -Expected 23456 -Actual $plan[1] -Because '分区 1 的 NEW-OFFSET'
    Assert-True -Condition ($null -ne (Get-ThrownMessage { ConvertFrom-KafkaResetPlan -Lines @('OK') -Topic $Topic })) -Because '没有计划表就没法复核,必须失败'
    Assert-True -Condition ($null -ne (Get-ThrownMessage { ConvertFrom-KafkaResetPlan -Lines @($lines[0], "g  $Topic  0  -") -Topic $Topic })) -Because 'NEW-OFFSET 不是数值必须失败'
    Assert-True -Condition ($null -ne (Get-ThrownMessage { ConvertFrom-KafkaResetPlan -Lines @($lines[0]) -Topic $Topic })) -Because '计划里没有本 topic 的分区必须失败'
}

Test-Case 'ConvertFrom-KafkaGetOffsets:解析 topic:partition:offset,只取本 topic;一行都没有就 throw' {
    $map = ConvertFrom-KafkaGetOffsets -Lines @("${Topic}:0:100", "${Topic}:1:0", 'db_task_zone_1011:0:7', 'WARN something') -Topic $Topic
    Assert-Equal -Expected 2 -Actual $map.Count -Because '前缀相同的别的 topic(db_task_zone_1011)不能混进来'
    Assert-Equal -Expected 100 -Actual $map[0] -Because '分区 0'
    Assert-True -Condition ($null -ne (Get-ThrownMessage { ConvertFrom-KafkaGetOffsets -Lines @('Error: no such topic') -Topic $Topic })) -Because '读不到日志边界必须失败'
}

Test-Case 'Test-HandoffFinalizeReady:新组缺非空分区、新组落后于旧组、缺日志边界都报问题;空分区与旧组缺行放行' {
    $earliest = @{ 0 = 0; 1 = 0; 2 = 5; 3 = 0 }
    $latest = @{ 0 = 100; 1 = 50; 2 = 5; 3 = 30 }
    # 分区 2 是空分区(earliest == latest):新组没有位点也放行。分区 3 旧组没有位点:不比较。
    $ok = Test-HandoffFinalizeReady -NewOffsets @{ 0 = 100; 1 = 40; 3 = 30 } -OldOffsets @{ 0 = 90; 1 = 40 } -Earliest $earliest -Latest $latest -Partitions 4
    Assert-Equal -Expected 0 -Actual $ok.Count -Because "新组都不小于旧组、空分区缺位点属正常,应当通过。实际问题: $($ok -join '; ')"

    $missing = Test-HandoffFinalizeReady -NewOffsets @{ 0 = 100 } -OldOffsets @{ 0 = 90; 1 = 40 } -Earliest $earliest -Latest $latest -Partitions 4
    Assert-True -Condition (@($missing | Where-Object { $_ -match '分区 1' }).Count -eq 1) -Because "非空分区 1 新组没有位点必须报问题(删掉旧组位点后就找不回续接点)。实际: $($missing -join '; ')"
    Assert-True -Condition (@($missing | Where-Object { $_ -match '分区 2' }).Count -eq 0) -Because '空分区 2 不报问题'

    $behind = Test-HandoffFinalizeReady -NewOffsets @{ 0 = 80; 1 = 40; 3 = 30 } -OldOffsets @{ 0 = 90 } -Earliest $earliest -Latest $latest -Partitions 4
    Assert-True -Condition (@($behind | Where-Object { $_ -match '分区 0' -and $_ -match '小于' }).Count -eq 1) -Because "新组落后于旧组必须报问题。实际: $($behind -join '; ')"

    $noBounds = Test-HandoffFinalizeReady -NewOffsets @{ 0 = 1 } -OldOffsets @{} -Earliest @{ 0 = 0 } -Latest @{ 0 = 1 } -Partitions 2
    Assert-True -Condition (@($noBounds | Where-Object { $_ -match '分区 1' -and $_ -match 'earliest' }).Count -eq 1) -Because 'get-offsets 缺分区时判不出是不是空分区,必须报问题'
}

Test-Case 'Get-DbTaskTopicName:与 go/db DbTaskTopicForGeneration 同规则' {
    Assert-Equal -Expected 'db_task_zone_101' -Actual (Get-DbTaskTopicName -ZoneId 101 -Generation 1) -Because '世代 1 不带后缀'
    Assert-Equal -Expected 'db_task_zone_101' -Actual (Get-DbTaskTopicName -ZoneId 101 -Generation 0) -Because '世代 ≤ 1 不带后缀'
    Assert-Equal -Expected 'db_task_zone_101_g3' -Actual (Get-DbTaskTopicName -ZoneId 101 -Generation 3) -Because '世代 > 1 带 _g<gen>'
}

Test-Case '重置计划覆盖检查与 -Apply 后复核' {
    $plan = @{ 0 = 10; 1 = 20 }
    Assert-Equal -Expected 0 -Actual (Get-ResetPlanProblems -Plan $plan -Partitions 2).Count -Because '计划覆盖 0..1'
    Assert-Equal -Expected 0 -Actual (Get-ResetPlanProblems -Plan $plan -Partitions 0).Count -Because '没给 -Partitions 不做覆盖检查'
    $short = Get-ResetPlanProblems -Plan $plan -Partitions 3
    Assert-True -Condition (@($short | Where-Object { $_ -match '缺少分区 2' }).Count -eq 1) -Because "计划缺分区必须报出。实际: $($short -join '; ')"
    $extra = Get-ResetPlanProblems -Plan @{ 0 = 1; 1 = 1; 2 = 1 } -Partitions 2
    Assert-True -Condition (@($extra | Where-Object { $_ -match '分区 2' }).Count -eq 1) -Because '计划里多出分区说明分区数与 db.yaml 不一致'

    $verified = Get-ResetVerificationProblems -Plan $plan -Described ([pscustomobject]@{ State = 'offsets'; Committed = @{ 0 = 10; 1 = 20 } }) -Partitions 2
    Assert-Equal -Expected 0 -Actual $verified.Count -Because "CURRENT-OFFSET 与计划一致应当通过。实际: $($verified -join '; ')"
    $stillMissing = Get-ResetVerificationProblems -Plan $plan -Described ([pscustomobject]@{ State = 'missing'; Committed = @{} }) -Partitions 2
    Assert-True -Condition (@($stillMissing | Where-Object { $_ -match '仍不存在' }).Count -eq 1) -Because '重置后组仍不存在 = --execute 没建出位点,必须失败(此时删 RetireGroup 位点会让两组都没有位点)'
    $mismatch = Get-ResetVerificationProblems -Plan $plan -Described ([pscustomobject]@{ State = 'offsets'; Committed = @{ 0 = 10; 1 = 25 } }) -Partitions 2
    Assert-True -Condition (@($mismatch | Where-Object { $_ -match '分区 1' }).Count -eq 1) -Because "位点与计划不符必须报出。实际: $($mismatch -join '; ')"
    $absent = Get-ResetVerificationProblems -Plan $plan -Described ([pscustomobject]@{ State = 'offsets'; Committed = @{ 0 = 10 } }) -Partitions 2
    Assert-True -Condition (@($absent | Where-Object { $_ -match '分区 1' -and $_ -match '没有已提交位点' }).Count -eq 1) -Because '计划内的分区重置后没有位点必须报出'
}

Test-Case '重置模式:复核失败的 throw 位于删除 RetireGroup 位点之前' {
    $text = Get-Content $scriptPath -Raw
    $marker = $text.IndexOf('# ── Offset reset (group + topic mode)')
    Assert-True -Condition ($marker -ge 0) -Because '找不到重置模式段落的锚点注释(契约测试的前提被改掉了)'
    $section = $text.Substring($marker)
    $verifyThrow = $section.IndexOf('重置后复核失败')
    $retireCall = $section.IndexOf('Remove-RetiredGroupTopicOffsets -CgTool')
    Assert-True -Condition ($verifyThrow -ge 0 -and $retireCall -ge 0) -Because '重置模式段落里必须既有复核失败的 throw,也有删除 RetireGroup 位点的调用'
    Assert-True -Condition ($verifyThrow -lt $retireCall) -Because '复核没过就删旧组位点,重置静默无效时两组都没有位点,go/db 从 Oldest 重放整个保留期'
    Assert-Match -Text $section -Pattern 'Get-ResetVerificationProblems -Plan \$plan -Described \$afterReset' -Because '-Apply 之后必须按 --execute 的计划逐分区复核'
}

# 假的 kafka-consumer-groups.sh:按调用顺序回放 $script:FakeCgReplies(@{ Lines; ExitCode }),并记下每次的参数。
$script:FakeCgCalls = [System.Collections.Generic.List[string]]::new()
$script:FakeCgReplies = [System.Collections.Generic.Queue[object]]::new()
function Invoke-FakeCg {
    $script:FakeCgCalls.Add(($args -join ' '))
    if ($script:FakeCgReplies.Count -eq 0) { throw '假 kafka-consumer-groups.sh 没有准备好的回应(被测函数多调了一次)' }
    $reply = $script:FakeCgReplies.Dequeue()
    $global:LASTEXITCODE = $reply.ExitCode
    return $reply.Lines
}
function Reset-FakeCg {
    param([object[]]$Replies)
    $script:FakeCgCalls.Clear()
    $script:FakeCgReplies.Clear()
    foreach ($reply in $Replies) { $script:FakeCgReplies.Enqueue($reply) }
}
$withOffsets = @{ Lines = @($DescribeHeader, (New-DescribeRow -Group 'db_rpc_consumer_group' -Partition 0 -Current '40' -End '40' -Lag '0')); ExitCode = 0 }
$withoutOffsets = @{ Lines = @('', "Consumer group 'db_rpc_consumer_group' has no active members."); ExitCode = 0 }
$deleted = @{ Lines = @("Request succeed for deleting offsets with topic $Topic group db_rpc_consumer_group", '', 'TOPIC  PARTITION  STATUS', "$Topic  0  Successful"); ExitCode = 0 }

Test-Case 'Remove-RetiredGroupTopicOffsets:旧组不存在或没有位点时什么都不删' {
    foreach ($reply in @(@{ Lines = @("Error: Consumer group 'db_rpc_consumer_group' does not exist."); ExitCode = 0 }, $withoutOffsets)) {
        Reset-FakeCg -Replies @($reply)
        Remove-RetiredGroupTopicOffsets -CgTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -RetireGroup 'db_rpc_consumer_group' -Topic $Topic -Apply 6>$null
        Assert-Equal -Expected 1 -Actual $script:FakeCgCalls.Count -Because '只 describe 一次,不执行 --delete-offsets'
        Assert-NotMatch -Text ($script:FakeCgCalls -join '|') -Pattern '--delete-offsets' -Because '没有可退役的位点'
    }
}

Test-Case 'Remove-RetiredGroupTopicOffsets:不带 -Apply 只打印,不删除' {
    Reset-FakeCg -Replies @($withOffsets)
    Remove-RetiredGroupTopicOffsets -CgTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -RetireGroup 'db_rpc_consumer_group' -Topic $Topic 6>$null
    Assert-Equal -Expected 1 -Actual $script:FakeCgCalls.Count -Because 'dry-run 只 describe'
}

Test-Case 'Remove-RetiredGroupTopicOffsets:-Apply 删除后再 describe 复核;仍有位点即失败' {
    Reset-FakeCg -Replies @($withOffsets, $deleted, $withoutOffsets)
    $message = Get-ThrownMessage { Remove-RetiredGroupTopicOffsets -CgTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -RetireGroup 'db_rpc_consumer_group' -Topic $Topic -Apply 6>$null }
    Assert-True -Condition ($null -eq $message) -Because "删干净时应当成功。错误: $message"
    Assert-Equal -Expected 3 -Actual $script:FakeCgCalls.Count -Because 'describe → --delete-offsets → describe 复核'
    Assert-Match -Text $script:FakeCgCalls[1] -Pattern "--delete-offsets --group db_rpc_consumer_group --topic $Topic" -Because '只删本 topic 上的位点,不删整个组(legacy 组还有别的 zone 在用)'

    Reset-FakeCg -Replies @($withOffsets, $deleted, $withOffsets)
    $message = Get-ThrownMessage { Remove-RetiredGroupTopicOffsets -CgTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -RetireGroup 'db_rpc_consumer_group' -Topic $Topic -Apply 6>$null }
    Assert-True -Condition ($null -ne $message) -Because '复核仍有位点(多半仍有成员订阅该 topic)必须失败'
    Assert-Match -Text $message -Pattern 'GROUP_SUBSCRIBED_TO_TOPIC' -Because '错误文本要点出最可能的原因'

    Reset-FakeCg -Replies @($withOffsets, @{ Lines = @('Error: Deletion of offsets failed due to: GROUP_SUBSCRIBED_TO_TOPIC'); ExitCode = 0 })
    $message = Get-ThrownMessage { Remove-RetiredGroupTopicOffsets -CgTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -RetireGroup 'db_rpc_consumer_group' -Topic $Topic -Apply 6>$null }
    Assert-True -Condition ($null -ne $message) -Because '--delete-offsets 输出 Error: 行必须失败'
    Assert-Equal -Expected 2 -Actual $script:FakeCgCalls.Count -Because '删除失败就不再复核'
}

# 参数校验:子进程跑真脚本,参数校验先于解析任何 Kafka 工具,不连 Kafka。
function Invoke-ResetScript {
    param([string[]]$Arguments)
    return Invoke-ToolScript -ScriptName 'kafka_offset_reset.ps1' -Arguments (@('-BootstrapServer', 'b:9092', '-Topic', $Topic) + $Arguments)
}

Test-Case '参数互斥:-FinalizeHandoff 与其它模式互斥;缺 -RetireGroup / -Partitions 拒绝' {
    $both = Invoke-ResetScript -Arguments @('-Group', 'z', '-RetireGroup', 'l', '-Partitions', '10', '-FinalizeHandoff', '-ToEarliest')
    Assert-True -Condition ($both.ExitCode -ne 0) -Because "两种模式同时给必须拒绝。输出: $($both.Output)"
    Assert-Match -Text $both.Output -Pattern 'FinalizeHandoff' -Because '错误文本要列出 -FinalizeHandoff 这一模式'
    foreach ($extra in @(@('-Group', 'z', '-Partitions', '10', '-FinalizeHandoff'), @('-Group', 'z', '-RetireGroup', 'l', '-FinalizeHandoff'))) {
        $run = Invoke-ResetScript -Arguments $extra
        Assert-True -Condition ($run.ExitCode -ne 0) -Because "$($extra -join ' ') 必须拒绝。输出: $($run.Output)"
        Assert-Match -Text $run.Output -Pattern '-FinalizeHandoff 必须同时给' -Because '错误文本要说明收尾需要哪些参数'
    }
}

Test-Case '-RetireGroup:缺 -Partitions、等于 -Group、配 -DeleteAndRecreateTopic 都拒绝' {
    $noPartitions = Invoke-ResetScript -Arguments @('-Group', 'z', '-RetireGroup', 'l', '-ToDatetime', '2026-05-15T03:17:00Z')
    Assert-True -Condition ($noPartitions.ExitCode -ne 0) -Because "缺 -Partitions 必须拒绝。输出: $($noPartitions.Output)"
    Assert-Match -Text $noPartitions.Output -Pattern '-RetireGroup 必须同时给 -Partitions' -Because '删旧组位点之前要逐分区复核'
    $same = Invoke-ResetScript -Arguments @('-Group', 'z', '-RetireGroup', 'z', '-Partitions', '10', '-ToDatetime', '2026-05-15T03:17:00Z')
    Assert-True -Condition ($same.ExitCode -ne 0) -Because "RetireGroup 等于 Group 必须拒绝。输出: $($same.Output)"
    Assert-Match -Text $same.Output -Pattern '-RetireGroup 不能等于 -Group' -Because '否则会删掉权威组自己的位点'
    $recreate = Invoke-ResetScript -Arguments @('-RetireGroup', 'l', '-Partitions', '10', '-DeleteAndRecreateTopic')
    Assert-True -Condition ($recreate.ExitCode -ne 0) -Because "配 -DeleteAndRecreateTopic 必须拒绝。输出: $($recreate.Output)"
    Assert-Match -Text $recreate.Output -Pattern '-RetireGroup 不能与 -DeleteAndRecreateTopic 同用' -Because '删 topic 没有旧组位点可退役'
}

exit (Complete-TestRun -SuiteName 'kafka_offset_reset 位点重置结果判定')
