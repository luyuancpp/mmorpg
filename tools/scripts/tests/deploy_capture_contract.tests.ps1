#requires -Version 7
# 采集链路守住退出码与两条输出流,不把部署脚本真实失败格式化成假红或成功。
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/lib/test_harness.ps1"
. "$PSScriptRoot/lib/deploy_capture.ps1"
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('deploy-capture-contract-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixtureRoot) > $null
function New-CaptureFixture {
    param([string]$Name, [string]$Source)
    $path = Join-Path $fixtureRoot ($Name + '.ps1')
    [IO.File]::WriteAllText($path, $Source, [Text.UTF8Encoding]::new($false))
    return $path
}
try {
    Test-Case '显式成功保留 stdout 与原生 stderr' {
        $path = New-CaptureFixture -Name success -Source "Write-Output '标准输出'; [Console]::Error.WriteLine('标准错误'); exit 0"
        $run = Invoke-CapturedPwsh -ScriptPath $path -Arguments @('fixture-argument')
        Assert-Equal -Expected 0 -Actual $run.ExitCode -Because 'stderr 有文本不等于进程失败'
        Assert-Match -Text $run.Output -Pattern '标准输出' -Because 'stdout 原文必须保留'
        Assert-Match -Text $run.Output -Pattern '标准错误' -Because '原生 stderr 原文必须保留'
    }
    Test-Case '显式非零退出不吞码,且保留失败前两条流' {
        $path = New-CaptureFixture -Name failure -Source "Write-Output '失败前输出'; [Console]::Error.WriteLine('实际失败原因'); exit 23"
        $run = Invoke-CapturedPwsh -ScriptPath $path -Arguments @('fixture-argument')
        Assert-Equal -Expected 23 -Actual $run.ExitCode -Because '不能把 exit 23 改成成功或笼统 exit 1'
        Assert-Match -Text $run.Output -Pattern '失败前输出' -Because '失败前 stdout 仍要保留'
        Assert-Match -Text $run.Output -Pattern '实际失败原因' -Because 'stderr 真实原因仍要保留'
    }
    Test-Case '正常返回不误用脚本内部原生命令的 LASTEXITCODE' {
        $path = New-CaptureFixture -Name handled -Source "& pwsh -NoProfile -NonInteractive -Command 'exit 9'; Write-Output '已经处理的命令失败'"
        $run = Invoke-CapturedPwsh -ScriptPath $path -Arguments @('fixture-argument')
        Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '-File 目标正常返回时应为 0,即使内部 LASTEXITCODE 为 9'
        Assert-Match -Text $run.Output -Pattern '已经处理的命令失败' -Because '目标确实执行完成'
    }
    Test-Case '终止异常保留完整多行长消息并返回非零' {
        $message = ('长错误文本不会被控制台宽度拆断' * 20) + "`n配置了 -GatewayIngressHost 却没有 -GatewayTrustedProxies"
        $source = "Write-Output '异常前输出'; [Console]::Error.WriteLine('异常前错误'); throw '" + $message + "'"
        $path = New-CaptureFixture -Name terminating -Source $source
        $run = Invoke-CapturedPwsh -ScriptPath $path -Arguments @('fixture-argument')
        Assert-Equal -Expected 1 -Actual $run.ExitCode -Because '终止异常与直接 pwsh -File 一样失败'
        Assert-True -Condition ($run.Output.Replace("`r`n", "`n").Contains($message)) -Because '完整错误原文含换行必须逐字存在,不能按屏宽插入装饰'
        Assert-Match -Text $run.Output -Pattern '异常前输出' -Because '不能吞掉 stdout'
        Assert-Match -Text $run.Output -Pattern '异常前错误' -Because '不能吞掉 stderr'
        Assert-Match -Text $run.Output -Pattern 'terminating\.ps1' -Because '仍有错误源文件定位'
    }
    Test-Case '目标路径与参数包含空格和参数前缀时原样转发' {
        $path = New-CaptureFixture -Name 'path with spaces' -Source '$args | ConvertTo-Json -Compress'
        $expected = @('-ScriptPath', 'value with spaces', '中文参数')
        $run = Invoke-CapturedPwsh -ScriptPath $path -Arguments $expected
        Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '包装层不应把目标的参数当成自己的'
        $actual = @($run.Output | ConvertFrom-Json)
        Assert-Equal -Expected ($expected -join '|') -Actual ($actual -join '|') -Because '位置和原始参数值必须保持'
    }
    Test-Case '高级脚本的具名参数、开关和整数绑定与 pwsh -File 一致' {
        $source = '[CmdletBinding(PositionalBinding=$false)] param([string]$Command,[switch]$DryRun,[int]$Count); [pscustomobject]@{Command=$Command;DryRun=$DryRun.IsPresent;Count=$Count} | ConvertTo-Json -Compress'
        $path = New-CaptureFixture -Name named -Source $source
        $run = Invoke-CapturedPwsh -ScriptPath $path -Arguments @('-Command', 'foo', '-DryRun', '-Count', '3')
        Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '不能把 -Command 当成位置值或丢掉开关'
        $actual = $run.Output | ConvertFrom-Json
        Assert-Equal -Expected 'foo' -Actual $actual.Command -Because '字符串按参数名绑定'
        Assert-True -Condition $actual.DryRun -Because '开关必须为真'
        Assert-Equal -Expected 3 -Actual $actual.Count -Because '整数必须完成类型转换'
    }
}
finally {
    # 只删除本套测试创建的临时目录,先校验绝对路径仍在系统 temp 下。
    $resolvedRoot = [IO.Path]::GetFullPath($fixtureRoot)
    $tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw '夹具目录越出 temp，拒绝清理' }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
exit (Complete-TestRun -SuiteName 'deployment output capture contract')