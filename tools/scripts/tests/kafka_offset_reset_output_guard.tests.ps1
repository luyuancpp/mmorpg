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

    判定函数按 AST 取出执行,不连任何 Kafka。

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

exit (Complete-TestRun -SuiteName 'kafka_offset_reset 位点重置结果判定')
