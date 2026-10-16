#requires -Version 7
<#
.SYNOPSIS
    在隔离目录运行真实 dev_tools.ps1 入口，验证生成器失败不会被包装成成功。
.DESCRIPTION
    外部生成器和 go 使用同一个最小进程桩；不运行真正的协议生成、不改生成树。
    子进程同时记录 finally 后的环境和工作目录，覆盖成功、失败及各候选选择路径。
#>
param([string]$EvidenceDir = (Join-Path ([IO.Path]::GetTempPath()) 'codex-proto-gen-exit-tests'))

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
. "$PSScriptRoot/lib/test_harness.ps1"

$fixture = Join-Path $EvidenceDir ('proto generator fixture ' + [Guid]::NewGuid().ToString('N'))
$scriptDir = Join-Path $fixture 'tools/scripts'
$generatorDir = Join-Path $fixture 'tools/proto_generator/protogen'
$stubDir = Join-Path $fixture 'stub'
foreach ($directory in @($scriptDir, (Join-Path $scriptDir 'lib'), (Join-Path $generatorDir 'etc'), $stubDir)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../dev_tools.ps1') -Destination $scriptDir
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../lib/release_common.ps1') -Destination (Join-Path $scriptDir 'lib')
foreach ($configName in @('proto_gen.yaml', 'custom.yaml')) {
    Set-Content -LiteralPath (Join-Path $generatorDir "etc/$configName") -Value '# 夹具配置，不执行真实生成'
}

# 原生进程确实返回非零码，避免用 PowerShell 函数替身掩盖 LASTEXITCODE 的行为。
$stubSource = Join-Path $stubDir 'generator.cs'
@'
using System;
using System.IO;
using System.Reflection;
class GeneratorStub {
    static int Main(string[] args) {
        File.AppendAllLines(Environment.GetEnvironmentVariable("PROTOGEN_TEST_RECORD"), new[] {
            "executable=" + Path.GetFileNameWithoutExtension(Assembly.GetExecutingAssembly().Location),
            "config=" + Environment.GetEnvironmentVariable("PROTO_GEN_CONFIG_PATH"),
            "pprof=" + Environment.GetEnvironmentVariable("PROTOGEN_ENABLE_PPROF"),
            "legacy=" + Environment.GetEnvironmentVariable("PBGEN_ENABLE_PPROF"),
            "cwd=" + Environment.CurrentDirectory,
            "arguments=" + String.Join("|", args)
        });
        return Int32.Parse(Environment.GetEnvironmentVariable("PROTOGEN_TEST_EXIT"));
    }
}
'@ | Set-Content -LiteralPath $stubSource -Encoding utf8
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$stubExe = Join-Path $stubDir 'generator.exe'
& $compiler /nologo /target:exe "/out:$stubExe" $stubSource 2>&1 | Set-Content -LiteralPath (Join-Path $fixture 'stub-build.log')
if ($LASTEXITCODE -ne 0) { throw "进程桩编译失败：$fixture/stub-build.log" }
Copy-Item -LiteralPath $stubExe -Destination (Join-Path $stubDir 'go.exe')

$childPath = Join-Path $fixture 'run.ps1'
@'
param([string]$FixtureRoot, [string]$Mode, [int]$ToolExit, [bool]$InitialEnvironment)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$env:PATH = (Join-Path $FixtureRoot 'stub') + [IO.Path]::PathSeparator + $env:PATH
$env:PROTOGEN_TEST_RECORD = Join-Path $FixtureRoot 'generator-record.txt'
$env:PROTOGEN_TEST_EXIT = [string]$ToolExit
$names = @('PROTO_GEN_CONFIG_PATH', 'PROTOGEN_ENABLE_PPROF', 'PBGEN_ENABLE_PPROF')
foreach ($name in $names) {
    $value = if ($InitialEnvironment) { '原值-' + $name } else { $null }
    [Environment]::SetEnvironmentVariable($name, $value, 'Process')
}
$before = @{}
foreach ($name in $names) { $before[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
Set-Location -LiteralPath $FixtureRoot
$beforeLocation = $PWD.Path
$parameters = @{
    Command = 'proto-gen-run'; ImageTag = 'fixture'; GoSvcTag = 'fixture'
    JavaSvcTag = 'fixture'; NodeImage = 'fixture:local'
}
if ($Mode -eq 'go-run') { $parameters.UseGoRun = $true }
if ($Mode -like 'binary-*') { $parameters.UseBinary = $true }
if ($InitialEnvironment) {
    $parameters.ConfigPath = 'tools/proto_generator/protogen/etc/custom.yaml'
} else {
    $parameters.EnablePprof = $true
}
try {
    & (Join-Path $FixtureRoot 'tools/scripts/dev_tools.ps1') @parameters
} finally {
    $after = @{}
    foreach ($name in $names) { $after[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
    @{ Before = $before; After = $after; BeforeLocation = $beforeLocation; AfterLocation = $PWD.Path } |
        ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $FixtureRoot 'restored.json')
}
'@ | Set-Content -LiteralPath $childPath -Encoding utf8

$pwshPath = (Get-Process -Id $PID).Path
$modes = @(
    @{ Name = 'go-run'; Primary = $true; Legacy = $true; Executable = 'go' },
    @{ Name = 'binary-primary'; Primary = $true; Legacy = $true; Executable = 'proto-gen' },
    @{ Name = 'binary-legacy'; Primary = $false; Legacy = $true; Executable = 'pbgen' },
    @{ Name = 'default-primary'; Primary = $true; Legacy = $true; Executable = 'proto-gen' },
    @{ Name = 'default-legacy'; Primary = $false; Legacy = $true; Executable = 'pbgen' },
    @{ Name = 'default-go'; Primary = $false; Legacy = $false; Executable = 'go' }
)
foreach ($mode in $modes) {
    foreach ($candidate in @(@{ Name = 'proto-gen.exe'; Present = $mode.Primary }, @{ Name = 'pbgen.exe'; Present = $mode.Legacy })) {
        $candidatePath = Join-Path $generatorDir $candidate.Name
        if ($candidate.Present) {
            Copy-Item -LiteralPath $stubExe -Destination $candidatePath -Force
        } elseif (Test-Path -LiteralPath $candidatePath) {
            Remove-Item -LiteralPath $candidatePath
        }
    }
    foreach ($initialEnvironment in @($false, $true)) {
        foreach ($toolExit in @(0, 23)) {
            Test-Case "$($mode.Name)：生成器退出 $toolExit，原环境非空=$initialEnvironment" {
                $recordPath = Join-Path $fixture 'generator-record.txt'
                $statePath = Join-Path $fixture 'restored.json'
                foreach ($path in @($recordPath, $statePath)) {
                    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
                }
                # 原生 stderr 必须保留为测试数据，由断言判定，不能提前终止测试套件。
                $output = & $pwshPath -NoProfile -NonInteractive -File $childPath -FixtureRoot $fixture -Mode $mode.Name -ToolExit $toolExit -InitialEnvironment:$initialEnvironment 2>&1 | Out-String
                $processExit = $LASTEXITCODE
                $caseName = "$($mode.Name)-env-$initialEnvironment-exit-$toolExit"
                $output | Set-Content -LiteralPath (Join-Path $fixture "$caseName.log")
                Assert-True (Test-Path -LiteralPath $recordPath) '必须运行隔离进程桩，不能把入口自身报错算作生成器失败'
                $record = @(Get-Content -LiteralPath $recordPath)
                Assert-Equal 6 $record.Count '每次只调用一个生成器，失败时不得偷偷换旧版本再跑'
                Assert-Equal "executable=$($mode.Executable)" $record[0] '按入口参数选择生成器'
                $expectedConfig = if ($initialEnvironment) { 'custom.yaml' } else { 'proto_gen.yaml' }
                Assert-Equal ("config=" + (Join-Path $generatorDir "etc/$expectedConfig")) $record[1] '把本次配置传给生成器'
                $expectedPprof = if ($initialEnvironment) { '' } else { '1' }
                Assert-Equal "pprof=$expectedPprof" $record[2] 'pprof 开关按本次参数生效'
                Assert-Equal 'legacy=' $record[3] '本次生成不继承旧 profiling 开关'
                Assert-Equal "cwd=$generatorDir" $record[4] '在生成器模块目录运行'
                $expectedArguments = if ($mode.Executable -eq 'go') { 'arguments=run|./cmd' } else { 'arguments=' }
                Assert-Equal $expectedArguments $record[5] '可执行程序和 go run 使用各自正式参数'
                Assert-True (Test-Path -LiteralPath $statePath) '失败也必须完成 finally'
                $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json -AsHashtable
                foreach ($name in @('PROTO_GEN_CONFIG_PATH', 'PROTOGEN_ENABLE_PPROF', 'PBGEN_ENABLE_PPROF')) {
                    Assert-Equal $state.Before[$name] $state.After[$name] "finally 必须恢复 $name"
                }
                Assert-Equal $state.BeforeLocation $state.AfterLocation 'finally 必须恢复调用者的工作目录'
                if ($toolExit -eq 0) {
                    Assert-Equal 0 $processExit '成功生成应保持成功退出'
                } else {
                    Assert-True ($processExit -ne 0) "生成器退出 $toolExit 时包装入口必须失败，实际退出 $processExit"
                    Assert-Match $output '23' '失败诊断必须带原始生成器退出码'
                }
            }
        }
    }
}
Write-Host "证据目录：$fixture"
exit (Complete-TestRun -SuiteName 'proto-gen-run 退出码与 finally 恢复')
