#requires -Version 7
<#
.SYNOPSIS
    k8s_zone_rollback.ps1 Step 4 FLUSHDB 防护的契约测试(2026-10-12)。

.DESCRIPTION
    背景(docs/design/player-route-mapping-durability-proposal.md §9 第 0 批):K8s 下 data_service 的 MappingRedis、
    go/db 的 RedisClient 与各 zone 的玩家缓存是同一实例的 DB 0。回档脚本 Step 4 的「zone 缓存 FLUSHDB」打在这里,
    会连带清掉所有 zone 的归属映射、落点记录、合服标记、存盘游标与重试收据。

    本套钉住:
      - 目标 DB 里有任一全局共享键(player:zone / player:placement / player:maprev / merge:* / consumer:applied* / kafka:retry*)
        即拒绝,错误文本点名命中的 pattern;
      - 查询失败按「有」处理(fail-closed);
      - -ConfirmFlushSharedRedis 时命中与查询失败都只告警、放行;
      - 没有命中时放行;
      - dry-run 只预告核对、不调 redis-cli;Step 4 打印的命令行遮住口令;
      - dev_tools.ps1 的 -RollbackConfirmFlushSharedRedis 真正转给回滚脚本。

    判定函数按 AST 取出、注入假探针执行,不连任何 Redis;子进程用例全部是 dry-run。

.EXAMPLE
    pwsh -File tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1
#>

$ErrorActionPreference = "Stop"

. "$PSScriptRoot/lib/test_harness.ps1"
. "$PSScriptRoot/lib/deploy_capture.ps1"

Write-Host ""
Write-Host "=== k8s_zone_rollback.ps1 Step 4 FLUSHDB guard contract tests ==="

$rollbackPath = Join-Path (Get-ToolsScriptsDir) 'k8s_zone_rollback.ps1'
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($rollbackPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "k8s_zone_rollback.ps1 解析失败: $($parseErrors | Out-String)" }
$guardFunctions = @('Get-SharedRedisGuardPatterns', 'Find-SharedRedisGuardHits', 'Assert-RedisFlushTargetIsZoneLocal')
$found = @($ast.FindAll({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -in $guardFunctions }, $true))
if ($found.Count -ne $guardFunctions.Count) { throw "k8s_zone_rollback.ps1 里缺少防护函数,期望 $($guardFunctions -join ', '),实际找到 $($found.Name -join ', ')" }
foreach ($definition in $found) { . ([scriptblock]::Create($definition.Extent.Text)) }

function Get-ThrownMessage {
    param([Parameter(Mandatory = $true)][scriptblock]$Body)
    try { & $Body; return $null } catch { return $_.Exception.Message }
}

# 假探针:pattern → 命中的键名;表里没有的 pattern 视为没有命中。
function New-FakeProbe {
    param([hashtable]$Hits = @{})
    return { param($pattern) if ($Hits.ContainsKey($pattern)) { return $Hits[$pattern] }; return $null }.GetNewClosure()
}

Test-Case '全局共享键清单:覆盖归属映射、落点、合服标记、存盘游标与重试收据' {
    $patterns = @(Get-SharedRedisGuardPatterns)
    # player:maprev:* 是第 1a 批新增的一族(映射持久化 maprev,发布脚本 set-if-newer 的排序依据):
    # 有 maprev 的库几乎必然也有 player:zone,但销毁路径的清单要与新增状态逐项对齐(AGENTS.md §11.4)。
    foreach ($expected in @('player:zone:*', 'player:placement:*', 'player:maprev:*', 'merge:*', 'consumer:applied*', 'kafka:retry*')) {
        Assert-True -Condition ($patterns -contains $expected) -Because "缺少 ${expected}:FLUSHDB 会静默清掉这一类键"
    }
}

Test-Case '没有命中:放行' {
    $message = Get-ThrownMessage { Assert-RedisFlushTargetIsZoneLocal -Probe (New-FakeProbe) -Target 'r:6379/db3' }
    Assert-True -Condition ($null -eq $message) -Because "zone 私有缓存里没有全局键,不应拒绝。错误: $message"
}

Test-Case '命中任一全局键:拒绝,错误文本点名命中的 pattern 与目标' {
    foreach ($pattern in @(Get-SharedRedisGuardPatterns)) {
        $message = Get-ThrownMessage { Assert-RedisFlushTargetIsZoneLocal -Probe (New-FakeProbe -Hits @{ $pattern = 'k1' }) -Target 'redis.infra:6379/db0' }
        Assert-True -Condition ($null -ne $message) -Because "命中 $pattern 必须拒绝"
        Assert-Match -Text $message -Pattern '拒绝 FLUSHDB' -Because '错误文本要写明拒绝的是 FLUSHDB'
        Assert-Match -Text $message -Pattern ([regex]::Escape($pattern)) -Because '错误文本要点名命中的 pattern'
        Assert-Match -Text $message -Pattern ([regex]::Escape('redis.infra:6379/db0')) -Because '错误文本要点名目标实例与 DB'
    }
}

Test-Case '查询失败按「有」处理:拒绝(fail-closed)' {
    $failing = { param($pattern) throw "connection refused" }
    $message = Get-ThrownMessage { Assert-RedisFlushTargetIsZoneLocal -Probe $failing -Target 'r:6379/db0' }
    Assert-True -Condition ($null -ne $message) -Because '读不到就放行等于没有防护'
    Assert-Match -Text $message -Pattern '无法核对' -Because '错误文本要区分「查询失败」与「命中」'
    Assert-Match -Text $message -Pattern 'connection refused' -Because '要带上原始原因'
}

Test-Case '-ConfirmFlushSharedRedis:命中与查询失败都只告警、放行' {
    $hit = Get-ThrownMessage { Assert-RedisFlushTargetIsZoneLocal -Probe (New-FakeProbe -Hits @{ 'player:zone:*' = 'player:zone:1' }) -Target 'r:6379/db0' -ConfirmFlushSharedRedis -WarningAction SilentlyContinue }
    Assert-True -Condition ($null -eq $hit) -Because "显式确认后不应再拒绝。错误: $hit"
    $failing = { param($pattern) throw "timeout" }
    $fail = Get-ThrownMessage { Assert-RedisFlushTargetIsZoneLocal -Probe $failing -Target 'r:6379/db0' -ConfirmFlushSharedRedis -WarningAction SilentlyContinue }
    Assert-True -Condition ($null -eq $fail) -Because "显式确认后查询失败也放行。错误: $fail"
}

$RollbackBaseArgs = @(
    "-ZoneName", "contract-test", "-ZoneId", "101",
    "-TargetTime", "2026-05-15T14:23:00Z",
    "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:0123456789ab",
    "-SkipMySqlPause"
)

Test-Case 'dry-run:给了 -RedisHost 时预告 FLUSHDB 目标核对,不读 Redis;Step 4 的命令行遮住口令' {
    $run = Invoke-ToolScript -ScriptName 'k8s_zone_rollback.ps1' -Arguments ($RollbackBaseArgs + @('-RedisHost', 'redis.infra', '-RedisPassword', 'Sup3rSecret'))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "dry-run 不该失败。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern 'FLUSHDB target check' -Because '预检段要预告 Step 4 的目标核对'
    Assert-Match -Text $run.Output -Pattern 'dry-run does not read Redis' -Because 'dry-run 不调 redis-cli'
    Assert-NotMatch -Text $run.Output -Pattern 'Sup3rSecret' -Because '口令不得进日志(AGENTS §9)'
    Assert-Match -Text $run.Output -Pattern 'redis-cli [^\r\n]*-a \*\*\*[^\r\n]*FLUSHDB' -Because 'Step 4 打印的命令行要遮住 -a 后面的口令'
}

Test-Case 'dry-run:不给 -RedisHost 时不预告核对,Step 4 照旧跳过' {
    $run = Invoke-ToolScript -ScriptName 'k8s_zone_rollback.ps1' -Arguments $RollbackBaseArgs
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "dry-run 不该失败。输出: $($run.Output)"
    Assert-NotMatch -Text $run.Output -Pattern 'FLUSHDB target check' -Because '没有 FLUSHDB 目标就没有核对'
    Assert-Match -Text $run.Output -Pattern 'skipped — pass -RedisHost to enable' -Because 'Step 4 照旧跳过'
}

Test-Case 'dev_tools k8s-zone-rollback:-RollbackConfirmFlushSharedRedis 转给回滚脚本' {
    $devArgs = @(
        '-Command', 'k8s-zone-rollback',
        '-ZoneName', 'contract-test', '-ZoneId', '101',
        '-RollbackTargetTime', '2026-05-15T14:23:00Z',
        '-NodeImage', 'ghcr.io/luyuancpp/mmorpg-node:0123456789ab',
        '-RollbackSkipMySqlPause',
        '-RollbackRedisHost', 'redis.infra',
        '-RollbackConfirmFlushSharedRedis'
    )
    $run = Invoke-ToolScript -ScriptName 'dev_tools.ps1' -Arguments $devArgs
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "经 dev_tools.ps1 的回滚 dry-run 不该失败。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern '-ConfirmFlushSharedRedis supplied' -Because 'dev_tools.ps1 必须把开关真正转给回滚脚本,不能静默吞掉'

    $plain = Invoke-ToolScript -ScriptName 'dev_tools.ps1' -Arguments ($devArgs | Where-Object { $_ -ne '-RollbackConfirmFlushSharedRedis' })
    Assert-Equal -Expected 0 -Actual $plain.ExitCode -Because "不带开关的 dry-run 不该失败。输出: $($plain.Output)"
    Assert-NotMatch -Text $plain.Output -Pattern '-ConfirmFlushSharedRedis supplied' -Because '没给就不能出现'
}

exit (Complete-TestRun -SuiteName 'k8s_zone_rollback FLUSHDB 防护')
