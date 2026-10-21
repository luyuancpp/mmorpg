#requires -Version 7
# 全球SQL事务域和本机最小权限证明注入的行为回归，不启动服务。
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/lib/test_harness.ps1"
. "$PSScriptRoot/../lib/account_directory_dev_secret.ps1"
$taskTokens = $null
$taskErrors = $null
$taskAst = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../k8s_deploy.ps1'),[ref]$taskTokens,[ref]$taskErrors)
if ($taskErrors.Count) { throw '部署脚本解析失败' }
foreach ($taskName in @('Get-GlobalAuthoritySqlSettings','Get-FriendSqlSettings')) {
    $taskDefinition = $taskAst.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $taskName },$true)
    if ($null -eq $taskDefinition) { throw "缺少部署权威函数: $taskName" }
    Set-Item -Path "Function:script:$taskName" -Value $taskDefinition.Body.GetScriptBlock()
}
Test-Case '指定单一TiDB入口时全部旧别名归一，开发缺省只用于本地' {
    $value = Get-GlobalAuthoritySqlSettings -Profile prod -CanonicalHost 'tidb.internal:4000'
    Assert-Equal -Expected 'tidb.internal:4000' -Actual $value.Host -Because '正式域必须有唯一SQL入口'
    Assert-Equal -Expected 'true' -Actual $value.RequireTiDB -Because '显式入口必须要求后端TiDB探测'
    $alias = Get-GlobalAuthoritySqlSettings -Profile prod -FriendHost 'tidb.internal:4000' -MatchHost 'tidb.internal:4000'
    Assert-Equal -Expected $value.Host -Actual $alias.Host -Because '既有一致别名可以迁移到唯一入口'
    $dev = Get-GlobalAuthoritySqlSettings -Profile dev -DevelopmentHost 'mysql.local:3306'
    Assert-Equal -Expected 'false' -Actual $dev.RequireTiDB -Because '开发环境显式保留本机MySQL夹具'
}
Test-Case '独立SQL入口、生产缺省和带注入内容的地址全部拒绝' {
    foreach ($arguments in @(
        @{ Profile='prod' },
        @{ Profile='dev'; CanonicalHost='tidb.internal:4000'; TradeHost='other.internal:4000' },
        @{ Profile='prod'; CanonicalHost="tidb.internal:4000`nInjected: true" },
        @{ Profile='prod'; CanonicalHost='user:password@tidb.internal:4000' }
    )) {
        $rejected = $false
        try { $null = Get-GlobalAuthoritySqlSettings @arguments } catch { $rejected = $true }
        Assert-True -Condition $rejected -Because '事务目录与资产不能配置成不同SQL集群或注入清单内容'
    }
}
Test-Case '本机证明跨进程复用，match只取得scene验签能力' {
    $taskRoot = Join-Path ([IO.Path]::GetTempPath()) ('mmorpg-authority-secret-tests-'+[Guid]::NewGuid().ToString('N'))
    $taskSaved = Backup-AccountDirectoryDevSecrets -Service 'data-service'
    try {
        foreach ($key in $taskSaved.Keys) { [Environment]::SetEnvironmentVariable($key,$null,'Process') }
        Initialize-AccountDirectoryDevSecrets -RepoRoot $taskRoot -Service match
        $scene = [Environment]::GetEnvironmentVariable('MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE','Process')
        Assert-True -Condition ($scene.Length -ge 32) -Because 'scene证明需要随机充分长度'
        Assert-True -Condition ([string]::IsNullOrEmpty([Environment]::GetEnvironmentVariable('MMORPG_ACCOUNT_DIRECTORY_SECRET_TRADE','Process'))) -Because 'match不得取得trade签名能力'
        Initialize-AccountDirectoryDevSecrets -RepoRoot $taskRoot -Service 'data-service'
        Assert-True -Condition ($scene -ceq [Environment]::GetEnvironmentVariable('MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE','Process')) -Because '同一本机证明文件不能在后起进程轮换scene密钥'
        [Environment]::SetEnvironmentVariable('MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE',("x"*32)+"`nextra",'Process')
        $rejected = $false
        try { Initialize-AccountDirectoryDevSecrets -RepoRoot $taskRoot -Service match } catch { $rejected = $true }
        Assert-True -Condition $rejected -Because '显式环境值的换行不能写入证明文件'
    } finally {
        Restore-AccountDirectoryDevSecrets $taskSaved
        $resolved = [IO.Path]::GetFullPath($taskRoot)
        $allowed = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        if (-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'mmorpg-authority-secret-tests-*') { throw '临时证明目录超出范围' }
        if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    }
}
exit (Complete-TestRun -SuiteName '全球SQL与本机证明注入')
