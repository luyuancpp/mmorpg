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

    2026-10-17 起(R7 复核)另钉住:db_task topic 拒绝 -DeleteAndRecreateTopic、重建要显式 -ReplicationFactor、topic describe 摘要解析、
    删除后等待(--list 失败 / 超时即 throw);重置分支以 exit 0 结束、Invoke-KafkaToolCaptured 复位 LASTEXITCODE;收尾前置检查对 ≥ N 的分区报问题;
    「组不存在」白名单与表头矛盾时 throw;kafka-0 exec 的命令形状与分类(lib Get-DbGroupDescribeExecArgs / Resolve-DbGroupDescribeExecResult)。

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

# ─────────────────────────────────────────────────────────────────
# R7 复核(2026-10-17):[1] 删 topic 重建、[2] kafka-0 exec 读法、[5] 重置分支退出码、[7] 收尾分区数口径、[8] 白名单与表头矛盾
# ─────────────────────────────────────────────────────────────────

$recreateFunctions = @('ConvertFrom-KafkaTopicDescribe', 'Wait-KafkaTopicDeleted')
foreach ($name in $recreateFunctions) {
    $matched = @($ast.FindAll({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true))
    if ($matched.Count -ne 1) { throw "kafka_offset_reset.ps1 里应有且只有一个 $name,实际 $($matched.Count) 个" }
    . ([scriptblock]::Create($matched[0].Extent.Text))
}

Test-Case '[8] 白名单命中又列出本 topic 的数值位点 = 矛盾输出,throw;有表头但本 topic 没有数值位点 → no-offsets;没有表头仍判 missing' {
    # 管理客户端在新旧 describe API 之间回退时可能先打一行带 GroupIdNotFoundException 的 WARN,之后照常打印表格(4.3.1 上是否如此待 V3 实测)。
    $warn = '[2026-10-17 03:00:00,000] WARN describe fell back to the classic API after org.apache.kafka.common.errors.GroupIdNotFoundException'
    $contradictory = @($warn, $DescribeHeader, (New-DescribeRow -Partition 0 -Current '40' -End '40' -Lag '0'))
    $message = Get-ThrownMessage { Resolve-KafkaGroupTopicOffsets -Lines $contradictory -ExitCode 0 -Topic $Topic }
    Assert-True -Condition ($null -ne $message) -Because '当成 missing 会让 keep 去掉交接闸、显式切组跳过 F2;不猜哪一边为真'
    Assert-Match -Text $message -Pattern '自相矛盾' -Because '错误文本要点明是矛盾输出'
    Assert-Match -Text $message -Pattern '分区 0' -Because '要列出表里有位点的分区'

    $otherTopic = @($warn, $DescribeHeader, (New-DescribeRow -RowTopic 'db_task_zone_102' -Partition 0 -Current '5' -End '5' -Lag '0'))
    Assert-Equal -Expected 'no-offsets' -Actual (Resolve-KafkaGroupTopicOffsets -Lines $otherTopic -ExitCode 0 -Topic $Topic).State -Because '有表头时不认定 missing;本 topic 没有数值位点就是 no-offsets'
    $dashOnly = @($warn, $DescribeHeader, (New-DescribeRow -Partition 0 -Current '-' -End '5' -Lag '-'))
    Assert-Equal -Expected 'no-offsets' -Actual (Resolve-KafkaGroupTopicOffsets -Lines $dashOnly -ExitCode 0 -Topic $Topic).State -Because '"-" 行不算位点'
    $missing = Resolve-KafkaGroupTopicOffsets -Lines @('Error: Executing consumer group command failed due to org.apache.kafka.common.errors.GroupIdNotFoundException: Group g not found.') -ExitCode 1 -Topic $Topic
    Assert-Equal -Expected 'missing' -Actual $missing.State -Because '没有表头时白名单照旧认定组不存在(不论退出码)'
}

Test-Case '[7] Test-HandoffFinalizeReady:-Partitions 小于 topic 实际分区数时,超出部分的分区一律报问题(与 Get-ResetPlanProblems 同口径)' {
    $earliest = @{ 0 = 0; 1 = 0; 2 = 0; 3 = 0 }
    $latest = @{ 0 = 10; 1 = 10; 2 = 10; 3 = 10 }
    # 分区 0 本身没问题;1..3 只在 get-offsets(与旧组)里出现 —— --delete-offsets --topic 会删掉旧组在全部分区上的位点。
    $problems = Test-HandoffFinalizeReady -NewOffsets @{ 0 = 10 } -OldOffsets @{ 0 = 9; 3 = 7 } -Earliest $earliest -Latest $latest -Partitions 1
    foreach ($p in 1..3) {
        Assert-True -Condition (@($problems | Where-Object { $_ -match "^分区 ${p}:超出 -Partitions 1" }).Count -eq 1) -Because "分区 $p 超出 -Partitions 1,必须报问题。实际: $($problems -join '; ')"
    }
    Assert-Equal -Expected 3 -Actual $problems.Count -Because "只有 1..3 三项。实际: $($problems -join '; ')"
    $extra = Test-HandoffFinalizeReady -NewOffsets @{ 0 = 10; 5 = 1 } -OldOffsets @{} -Earliest @{ 0 = 0 } -Latest @{ 0 = 10 } -Partitions 1
    Assert-True -Condition (@($extra | Where-Object { $_ -match '^分区 5:超出' }).Count -eq 1) -Because "新组位点表里出现 ≥ N 的分区同样报问题。实际: $($extra -join '; ')"
}

Test-Case '[5] 「组不存在」的 describe 以非 0 退出时,Remove-RetiredGroupTopicOffsets 正常返回,且不把非 0 留在 $LASTEXITCODE' {
    # 有的 Kafka 版本对不存在的组以非 0 退出(lib 判为 missing);以前这个退出码留在全局 LASTEXITCODE 上,
    # 重置分支又不以 exit 结束,回档 Step 5 / dev_tools 在 PITR、FLUSHDB 之后把成功判成失败。
    Reset-FakeCg -Replies @(@{ Lines = @('Error: Executing consumer group command failed due to org.apache.kafka.common.errors.GroupIdNotFoundException: Group db_rpc_consumer_group not found.'); ExitCode = 1 })
    $global:LASTEXITCODE = 0
    $message = Get-ThrownMessage { Remove-RetiredGroupTopicOffsets -CgTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -RetireGroup 'db_rpc_consumer_group' -Topic $Topic -Apply 6>$null }
    Assert-True -Condition ($null -eq $message) -Because "组不存在 = nothing to retire,应当正常返回。错误: $message"
    Assert-Equal -Expected 0 -Actual $global:LASTEXITCODE -Because 'Invoke-KafkaToolCaptured 只经返回值报告退出码,全局 LASTEXITCODE 必须复位为 0'
}

Test-Case '[5] 重置模式的成功路径以 exit 0 结束(与 -FinalizeHandoff / -DeleteAndRecreateTopic 一致)' {
    $last = @($ast.EndBlock.Statements)[-1]
    Assert-True -Condition ($last -is [System.Management.Automation.Language.ExitStatementAst]) -Because "脚本最后一条顶层语句必须是 exit(实际: $($last.Extent.Text))"
    Assert-Equal -Expected '0' -Actual $last.Pipeline.Extent.Text -Because '成功路径以 0 退出,失败一律 throw'
}

Test-Case '[1] -DeleteAndRecreateTopic:db_task topic(含 _g<gen>)一律拒绝;其它 topic 缺 -ReplicationFactor 拒绝;都早于解析 Kafka 工具' {
    foreach ($topicName in @('db_task_zone_101', 'db_task_zone_101_g2')) {
        $run = Invoke-ToolScript -ScriptName 'kafka_offset_reset.ps1' -Arguments @('-BootstrapServer', 'b:9092', '-Topic', $topicName, '-DeleteAndRecreateTopic', '-Partitions', '10', '-ReplicationFactor', '3', '-Apply')
        Assert-True -Condition ($run.ExitCode -ne 0) -Because "$topicName 必须拒绝。输出: $($run.Output)"
        Assert-Match -Text $run.Output -Pattern 'DeleteAndRecreateTopic 拒绝 db_task topic' -Because "${topicName}:同名重建后新存盘被旧游标判 stale 静默丢弃"
        Assert-Match -Text $run.Output -Pattern 'TopicGeneration' -Because "${topicName}:错误文本要指向换代"
        Assert-NotMatch -Text $run.Output -Pattern 'Step 1: describe' -Because "${topicName}:拒绝早于任何 Kafka 调用"
    }
    $noRf = Invoke-ToolScript -ScriptName 'kafka_offset_reset.ps1' -Arguments @('-BootstrapServer', 'b:9092', '-Topic', 'contract_recreate_topic', '-DeleteAndRecreateTopic', '-Partitions', '3')
    Assert-True -Condition ($noRf.ExitCode -ne 0) -Because "缺 -ReplicationFactor 必须拒绝。输出: $($noRf.Output)"
    Assert-Match -Text $noRf.Output -Pattern '必须显式给 -ReplicationFactor' -Because '以前缺省 1,3 broker 档上重建出的 topic 写入全部 NotEnoughReplicas'

    foreach ($name in @('db_task_zone_1', 'db_task_zone_101_g2', 'db_task_zone_9999_g12')) {
        Assert-True -Condition (Test-DbTaskTopicName -Topic $name) -Because "$name 是 db_task topic"
    }
    foreach ($name in @('db_task_zone_', 'db_task_zone_1_g', 'db_task_zone_1x', 'xdb_task_zone_1', 'db_task_topic', 'contract_recreate_topic')) {
        Assert-True -Condition (-not (Test-DbTaskTopicName -Topic $name)) -Because "$name 不是 db_task topic"
    }
}

Test-Case '[1] ConvertFrom-KafkaTopicDescribe:读摘要行的 PartitionCount / ReplicationFactor(新旧两种写法);找不到或多于一行即 throw' {
    $lines = @("Topic: contract_t`tTopicId: abc`tPartitionCount: 10`tReplicationFactor: 3`tConfigs: min.insync.replicas=2",
        "`tTopic: contract_t`tPartition: 0`tLeader: 1`tReplicas: 1,2,3`tIsr: 1,2,3")
    $contract = ConvertFrom-KafkaTopicDescribe -Lines $lines -Topic 'contract_t'
    Assert-Equal -Expected 10 -Actual $contract.PartitionCount -Because '分区数取摘要行'
    Assert-Equal -Expected 3 -Actual $contract.ReplicationFactor -Because '副本数取摘要行(分区明细行不算)'
    $old = ConvertFrom-KafkaTopicDescribe -Lines @("Topic:contract_t`tPartitionCount:5`tReplicationFactor:1`tConfigs:") -Topic 'contract_t'
    Assert-Equal -Expected '5/1' -Actual "$($old.PartitionCount)/$($old.ReplicationFactor)" -Because '旧版本冒号后没有空格'
    Assert-True -Condition ($null -ne (Get-ThrownMessage { ConvertFrom-KafkaTopicDescribe -Lines @("Topic: contract_t_other`tPartitionCount: 1`tReplicationFactor: 1") -Topic 'contract_t' })) -Because '别的 topic 的摘要行不算,读不出就不删'
    Assert-True -Condition ($null -ne (Get-ThrownMessage { ConvertFrom-KafkaTopicDescribe -Lines @() -Topic 'contract_t' })) -Because '没有输出必须失败'
}

Test-Case '[1] Wait-KafkaTopicDeleted:topic 消失才返回;--list 失败或超时一律 throw(不进入 create)' {
    Reset-FakeCg -Replies @(@{ Lines = @('other_topic', 'contract_t'); ExitCode = 0 }, @{ Lines = @('other_topic'); ExitCode = 0 })
    $message = Get-ThrownMessage { Wait-KafkaTopicDeleted -TopicTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -Topic 'contract_t' -TimeoutSec 30 -PollMilliseconds 0 -RecreateHint 'RECREATE-HINT' }
    Assert-True -Condition ($null -eq $message) -Because "第二次 --list 里已没有该 topic,应当返回。错误: $message"
    Assert-Equal -Expected 2 -Actual $script:FakeCgCalls.Count -Because '轮询到消失为止'

    Reset-FakeCg -Replies @(@{ Lines = @('Error: Timed out waiting for a node assignment.'); ExitCode = 1 })
    $message = Get-ThrownMessage { Wait-KafkaTopicDeleted -TopicTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -Topic 'contract_t' -TimeoutSec 30 -PollMilliseconds 0 -RecreateHint 'RECREATE-HINT' }
    Assert-Match -Text $message -Pattern '读不到 topic 列表' -Because '以前 --list 失败被 2>$null 当成「已消失」直接 create'
    Assert-Match -Text $message -Pattern 'RECREATE-HINT' -Because '要告诉操作者 topic 可能已删、怎么手工重建'

    Reset-FakeCg -Replies @(@{ Lines = @('contract_t'); ExitCode = 0 })
    $message = Get-ThrownMessage { Wait-KafkaTopicDeleted -TopicTool 'Invoke-FakeCg' -BootstrapServer 'b:9092' -Topic 'contract_t' -TimeoutSec 0 -PollMilliseconds 0 -RecreateHint 'RECREATE-HINT' }
    Assert-Match -Text $message -Pattern '仍在 --list' -Because '超时不能照样 create'
    Assert-Equal -Expected 1 -Actual $script:FakeCgCalls.Count -Because '-TimeoutSec 0 只看一次'
}

Test-Case '[2] Get-DbGroupDescribeExecArgs / Resolve-DbGroupDescribeExecResult:kafka-0 exec 的命令形状与分类只此一份' {
    $execArgs = Get-DbGroupDescribeExecArgs -InfraNamespace 'contract-infra' -Group 'db_rpc_consumer_group_z101'
    $expected = @('exec', '-n', 'contract-infra', 'kafka-0', '-c', 'kafka', '--', '/bin/sh', '-c',
        "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group db_rpc_consumer_group_z101")
    Assert-Equal -Expected ($expected -join '|') -Actual ($execArgs -join '|') -Because 'k8s_deploy.ps1 Read-DbGroupTopicFacts 与回档第 0 步 e 必须是同一条命令'
    foreach ($bad in @('', 'a b', 'g;rm -rf /', "g'x")) {
        Assert-True -Condition ($null -ne (Get-ThrownMessage { Get-DbGroupDescribeExecArgs -InfraNamespace 'contract-infra' -Group $bad })) -Because "组名 '$bad' 会拼进 sh -c,必须拒绝"
    }
    $missing = Resolve-DbGroupDescribeExecResult -Result ([pscustomobject]@{ ExitCode = 0; Stdout = ''; Stderr = "Error: Consumer group 'g' does not exist." }) -Topic $Topic
    Assert-Equal -Expected 'missing' -Actual $missing.State -Because '「组不存在」那行在 stderr 上也要认(stdout / stderr 合并后分类)'
    $held = Resolve-DbGroupDescribeExecResult -Result ([pscustomobject]@{ ExitCode = 0; Stdout = ($DescribeHeader + "`n" + (New-DescribeRow -Partition 1 -Current '7' -End '9' -Lag '2')); Stderr = '' }) -Topic $Topic
    Assert-Equal -Expected 'offsets' -Actual $held.State -Because '本 topic 有数值位点'
    $forbidden = Get-ThrownMessage { Resolve-DbGroupDescribeExecResult -Result ([pscustomobject]@{ ExitCode = 1; Stdout = ''; Stderr = 'Error from server (Forbidden): pods "kafka-0" is forbidden: cannot create resource "pods/exec"' }) -Topic $Topic }
    Assert-True -Condition ($null -ne $forbidden) -Because 'exec 本身失败必须按读不到 throw,不当成「没有位点」'
}

# ─────────────────────────────────────────────────────────────────
# 回档丢弃语义(负责人 2026-10-17 裁决,2026-10-18 落码):Step 5 -ToLatest;lib 新增 --state / CONSUMER-ID / 静默 / 切点判定
# ─────────────────────────────────────────────────────────────────

Test-Case '-ToLatest + -RetireGroup + -Partitions + -Apply 通过参数校验;重置分支把 -ToLatest 映射成 --to-latest、-Apply 映射成 --execute' {
    # -KafkaBin 指向空目录:参数校验通过后在解析 Kafka 工具那一步停下,保证不连任何 Kafka(本机装了 CLI 也不会被调到)。
    $emptyBin = Join-Path ([IO.Path]::GetTempPath()) ('mmorpg-empty-kafka-bin-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $emptyBin | Out-Null
    try {
        $run = Invoke-ResetScript -Arguments @('-Group', 'db_rpc_consumer_group_z101', '-RetireGroup', 'db_rpc_consumer_group', '-ToLatest', '-Partitions', '10', '-KafkaBin', $emptyBin, '-Apply')
    }
    finally {
        Remove-Item -LiteralPath $emptyBin -Recurse -Force -ErrorAction SilentlyContinue
    }
    Assert-True -Condition ($run.ExitCode -ne 0) -Because "空的 -KafkaBin 必须在解析工具时失败。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern 'Kafka tool not found at' -Because "回档 Step 5 的参数组合必须通过参数校验,停在解析工具那一步。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern 'mode\s+: ToLatest' -Because '模式是 ToLatest'
    Assert-Match -Text $run.Output -Pattern 'retire\s+: db_rpc_consumer_group ' -Because '-RetireGroup 与 -ToLatest 可以同用'

    $text = Get-Content $scriptPath -Raw
    Assert-Match -Text $text -Pattern '"ToLatest"\s*\{\s*\$cmdArgs \+= "--to-latest"\s*\}' -Because '-ToLatest 映射成 --to-latest'
    Assert-Match -Text $text -Pattern 'if \(\$Apply\) \{\s*\$cmdArgs \+= "--execute"' -Because '-Apply 映射成 --execute'
    Assert-NotMatch -Text $text -Pattern '尚待负责人裁决' -Because '回档语义已定,不再留「待裁决」注释'
}

Test-Case 'ConvertFrom-KafkaGroupStateDescribe:从右往左取 STATE / #MEMBERS(COORDINATOR (ID) 带空格、Empty 组策略为空);组不存在判 missing' {
    $header = 'GROUP                       COORDINATOR (ID)          ASSIGNMENT-STRATEGY  STATE           #MEMBERS'
    $empty = ConvertFrom-KafkaGroupStateDescribe -Lines @('', $header, 'db_rpc_consumer_group_z101  kafka-0.kafka:9092 (0)                       Empty           0') -ExitCode 0 -Group 'db_rpc_consumer_group_z101'
    Assert-Equal -Expected 'Empty|0' -Actual "$($empty.State)|$($empty.Members)" -Because 'Empty 组的 ASSIGNMENT-STRATEGY 为空,按下标取会错位'
    Assert-True -Condition (Test-KafkaGroupStateInactive -State $empty.State) -Because 'Empty 视为没有成员在消费'
    $stable = ConvertFrom-KafkaGroupStateDescribe -Lines @($header, 'g  kafka-1.kafka:9092 (1)  range  Stable  3') -ExitCode 0 -Group 'g'
    Assert-Equal -Expected 'Stable|3' -Actual "$($stable.State)|$($stable.Members)" -Because 'Stable 组'
    Assert-True -Condition (-not (Test-KafkaGroupStateInactive -State $stable.State)) -Because 'Stable 有成员在消费'
    foreach ($state in @('Dead', 'missing')) { Assert-True -Condition (Test-KafkaGroupStateInactive -State $state) -Because "$state 视为没有成员" }
    Assert-True -Condition (-not (Test-KafkaGroupStateInactive -State 'empty')) -Because '大小写敏感,不猜'
    $missing = ConvertFrom-KafkaGroupStateDescribe -Lines @("Error: Consumer group 'g' does not exist.") -ExitCode 0 -Group 'g'
    Assert-Equal -Expected 'missing' -Actual $missing.State -Because '没有表头 + 白名单文案 = 组不存在'
}

Test-Case 'ConvertFrom-KafkaGroupStateDescribe:Error 行、退出码非 0、没有表头、表头末两列不对、组的行不是恰好一行、值非法,一律 throw' {
    $header = 'GROUP  COORDINATOR (ID)  ASSIGNMENT-STRATEGY  STATE  #MEMBERS'
    $cases = @(
        @{ Label = 'Error 行'; Lines = @($header, 'g  k:9092 (0)  Empty  0', 'Error: Executing consumer group command failed due to TimeoutException'); Exit = 0 },
        @{ Label = '退出码非 0'; Lines = @($header, 'g  k:9092 (0)  Empty  0'); Exit = 1 },
        @{ Label = '没有表头'; Lines = @('g  k:9092 (0)  Empty  0'); Exit = 0 },
        @{ Label = '表头末两列不是 STATE #MEMBERS'; Lines = @('GROUP  COORDINATOR (ID)  STATE  #MEMBERS  EXTRA', 'g  k:9092 (0)  Empty  0  x'); Exit = 0 },
        @{ Label = '组的行有两行'; Lines = @($header, 'g  k:9092 (0)  Empty  0', 'g  k:9092 (0)  Stable  1'); Exit = 0 },
        @{ Label = '组的行一行都没有'; Lines = @($header, 'other  k:9092 (0)  Empty  0'); Exit = 0 },
        @{ Label = '#MEMBERS 不是整数'; Lines = @($header, 'g  k:9092 (0)  Empty  x'); Exit = 0 },
        @{ Label = 'STATE 含非字母'; Lines = @($header, 'g  k:9092 (0)  Em-pty  0'); Exit = 0 }
    )
    foreach ($c in $cases) {
        $message = Get-ThrownMessage { ConvertFrom-KafkaGroupStateDescribe -Lines $c.Lines -ExitCode $c.Exit -Group 'g' }
        Assert-True -Condition ($null -ne $message) -Because "$($c.Label):判不出就不能当成 Empty(回档 Step 1b 据此确认没有 db 实例还在消费)"
    }
}

Test-Case 'Get-KafkaTopicActiveConsumers:CONSUMER-ID 不是 "-" 的分区列出来;没有行返回空;表头缺 CONSUMER-ID 列即 throw' {
    $assigned = Resolve-KafkaGroupTopicOffsets -Lines @($DescribeHeader, "g  $Topic  0  10  12  2  consumer-1-abc  /10.0.0.7  consumer-1", (New-DescribeRow -Partition 1 -Current '5' -End '5' -Lag '0')) -ExitCode 0 -Topic $Topic
    # 以 `, $数组` 返回:直接赋值拿到数组本身(不套 @(...),否则空表也成了 Count 1)。
    $active = Get-KafkaTopicActiveConsumers -Resolved $assigned
    Assert-Equal -Expected 1 -Actual $active.Count -Because "只有分区 0 分配给了成员(实际: $($active -join '; '))"
    Assert-Match -Text $active[0] -Pattern '分区 0:consumer-1-abc' -Because '要列出分区与成员'
    $idle = Resolve-KafkaGroupTopicOffsets -Lines @($DescribeHeader, (New-DescribeRow -Partition 0 -Current '5' -End '5' -Lag '0')) -ExitCode 0 -Topic $Topic
    Assert-Equal -Expected 0 -Actual (Get-KafkaTopicActiveConsumers -Resolved $idle).Count -Because '"-" 表示没有成员'
    $missing = Resolve-KafkaGroupTopicOffsets -Lines @("Error: Consumer group 'g' does not exist.") -ExitCode 0 -Topic $Topic
    Assert-Equal -Expected 0 -Actual (Get-KafkaTopicActiveConsumers -Resolved $missing).Count -Because '组不存在 = 没有成员'
    $noColumn = Resolve-KafkaGroupTopicOffsets -Lines @('GROUP  TOPIC  PARTITION  CURRENT-OFFSET  LOG-END-OFFSET  LAG', "g  $Topic  0  5  5  0") -ExitCode 0 -Topic $Topic
    Assert-True -Condition ($null -ne (Get-ThrownMessage { Get-KafkaTopicActiveConsumers -Resolved $noColumn })) -Because '表头没有 CONSUMER-ID 列就判不出,不能当成没有成员'
    Assert-True -Condition (-not $noColumn.HasConsumerIdColumn -and $idle.HasConsumerIdColumn) -Because 'HasConsumerIdColumn 随表头而定'
}

Test-Case 'Test-KafkaTopicQuiescent:两次日志末端完全相同且覆盖 0..N-1 才静默;变化、缺分区、超出分区数都报问题' {
    Assert-Equal -Expected 0 -Actual (Test-KafkaTopicQuiescent -First @{ 0 = 10; 1 = 20 } -Second @{ 0 = 10; 1 = 20 } -Partitions 2).Count -Because '相同即静默'
    $moved = Test-KafkaTopicQuiescent -First @{ 0 = 10; 1 = 20 } -Second @{ 0 = 11; 1 = 20 } -Partitions 2
    Assert-True -Condition (@($moved | Where-Object { $_ -match '分区 0:日志末端从 10 变为 11' }).Count -eq 1) -Because "变化要报出(实际: $($moved -join '; '))"
    $short = Test-KafkaTopicQuiescent -First @{ 0 = 10 } -Second @{ 0 = 10 } -Partitions 2
    Assert-True -Condition (@($short | Where-Object { $_ -match '分区 1' }).Count -eq 1) -Because '缺分区判不出,要报问题'
    $extra = Test-KafkaTopicQuiescent -First @{ 0 = 1; 1 = 1; 2 = 1 } -Second @{ 0 = 1; 1 = 1; 2 = 1 } -Partitions 2
    Assert-True -Condition (@($extra | Where-Object { $_ -match '分区 2:超出' }).Count -eq 1) -Because '超出分区数说明与 db.yaml 不一致'
    Assert-True -Condition ((Test-KafkaTopicQuiescent -First @{} -Second @{} -Partitions 0).Count -eq 1) -Because '分区数必须 > 0'
}

Test-Case 'Get-KafkaGroupCutProblems / ConvertTo-KafkaCutText:每个分区 CURRENT == LOG-END(给了切点还要等于切点);切点文本按分区升序' {
    $rows = @(
        [pscustomobject]@{ Partition = 1; CurrentOffset = [long]20; LogEndOffset = [long]20; Lag = [long]0 },
        [pscustomobject]@{ Partition = 0; CurrentOffset = [long]10; LogEndOffset = [long]10; Lag = [long]0 }
    )
    Assert-Equal -Expected 0 -Actual (Get-KafkaGroupCutProblems -Rows $rows -Partitions 2).Count -Because '都在末端'
    Assert-Equal -Expected 0 -Actual (Get-KafkaGroupCutProblems -Rows $rows -Partitions 2 -ExpectedCut @{ 0 = 10; 1 = 20 }).Count -Because '仍等于切点'
    $moved = Get-KafkaGroupCutProblems -Rows $rows -Partitions 2 -ExpectedCut @{ 0 = 10; 1 = 19 }
    Assert-True -Condition (@($moved | Where-Object { $_ -match '分区 1:位点 / 日志末端 20 偏离 Step 5 的切点 19' }).Count -eq 1) -Because "偏离切点要报出(实际: $($moved -join '; '))"
    $lagging = Get-KafkaGroupCutProblems -Rows @([pscustomobject]@{ Partition = 0; CurrentOffset = [long]10; LogEndOffset = [long]12; Lag = [long]2 }) -Partitions 1
    Assert-True -Condition (@($lagging | Where-Object { $_ -match 'CURRENT-OFFSET 10 不等于 LOG-END-OFFSET 12' }).Count -eq 1) -Because 'LAG > 0 说明切点之后又有写入'
    $absent = Get-KafkaGroupCutProblems -Rows @($rows[1]) -Partitions 2
    Assert-True -Condition (@($absent | Where-Object { $_ -match '分区 1:zone 组在本 topic 上没有这一行' }).Count -eq 1) -Because '缺分区要报出'
    $dash = Get-KafkaGroupCutProblems -Rows @([pscustomobject]@{ Partition = 0; CurrentOffset = $null; LogEndOffset = [long]5; Lag = $null }) -Partitions 1
    Assert-True -Condition (@($dash | Where-Object { $_ -match '不是数值' }).Count -eq 1) -Because '"-" 不算位点'
    Assert-Equal -Expected '0=10,1=20,10=7' -Actual (ConvertTo-KafkaCutText -Cut @{ 10 = 7; 1 = 20; 0 = 10 }) -Because '按分区号数值升序,不按字符串'
}

exit (Complete-TestRun -SuiteName 'kafka_offset_reset 位点重置结果判定')
