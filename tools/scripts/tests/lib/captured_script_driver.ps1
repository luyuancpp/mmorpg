#requires -Version 7
# 测试子进程驱动:异常文本不经 ConciseView 按屏宽换行,否则完整错误原因会被 `|` 装饰拆断。
# 不声明 param 块:第一个位置参数是目标路径,后续参数原样转给目标脚本,不与包装层参数重名。
$targetScriptPath = $args[0]
$targetArguments = @($args | Select-Object -Skip 1)
$LASTEXITCODE = 0
try {
    & $targetScriptPath @targetArguments
    # 目标脚本显式 exit N 会返回这里,必须保持它的退出码;没有 exit 时默认成功。
    $scriptSucceeded = $?
    if ($scriptSucceeded) { exit 0 }
    exit $LASTEXITCODE
}
catch {
    # 保留终止异常原文和定位信息;目标此前写出的 stdout / stderr 已原样流出。
    [Console]::Error.WriteLine($_.Exception.Message)
    [Console]::Error.WriteLine($_.ScriptStackTrace)
    exit 1
}