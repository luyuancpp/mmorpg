#requires -Version 7
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../lib/no_raw_pointer_project.ps1')

$script:Invocations = [Collections.Generic.List[object]]::new()
$script:FailureAt = ''
function Invoke-FakeClang {
    $script:Invocations.Add(@($args))
    $global:LASTEXITCODE = 0
    if ($args -contains $script:FailureAt) { return 'source.cpp:1: error: 不能解析' }
    return '0 matches.'
}
$sources = @('first with space.cpp', 'second.cpp', 'third.cpp')
$code = Invoke-ClangProjectSourceCheck -Executable 'Invoke-FakeClang' -Query 'rule.cq' -Sources $sources -CompilerArguments @('-DMODULE=1', '-Iinclude with space')
if ($code -ne 0 -or $script:Invocations.Count -ne 3) { throw '必须逐一检查全部翻译单元' }
for ($index=0; $index -lt 3; ++$index) {
    $invocation = $script:Invocations[$index]
    if ($invocation[2] -cne $sources[$index] -or @($invocation | Where-Object { $_ -like '*.cpp' }).Count -ne 1) { throw '一次调用只能保留一个单元的AST' }
    if ($invocation -notcontains '-DMODULE=1' -or $invocation -notcontains '-Iinclude with space') { throw '不能丢失实际编译配置' }
}
$script:Invocations.Clear(); $script:FailureAt = 'second.cpp'
$code = Invoke-ClangProjectSourceCheck -Executable 'Invoke-FakeClang' -Query 'rule.cq' -Sources $sources -CompilerArguments @('-DMODULE=1')
if ($code -ne 2 -or $script:Invocations.Count -ne 2) { throw '即使工具退出0，解析错误也必须阻断且不能记录通过' }
if ((Get-ClangQueryExitCode -OutputText '"root" binds here' -ProcessExitCode 0) -ne 1) { throw '规则违规必须阻断' }
Write-Output 'PASS：逐单元完整检查、原编译参数、解析失败与规则违规阻断'
