# ==== 配置部分 ====
$RootPath = "F:\work\mmorpg\cpp"               # 根目录
$ExcludePaths = @(
"F:\work\mmorpg\cpp\libs\base\muduo_windows",
"F:\work\mmorpg\cpp\unit_test"
)

$OutputFile = "F:\work\mmorpg\tools\tree.txt"  # 输出文件路径

# ==== 主函数 ====
function Show-Tree {
    param (
        [string]$Path,
        [int]$Indent = 0
    )

    # 检查是否在排除路径中
    foreach ($exclude in $ExcludePaths) {
        if ($Path -like "$exclude*") {
            return
        }
    }

    $prefix = ' ' * $Indent + '|-- '
    Add-Content -Path $OutputFile -Value "$prefix$(Split-Path $Path -Leaf)"

    # 列出子目录
    Get-ChildItem -Path $Path -Directory -Force | Sort-Object Name | ForEach-Object {
        Show-Tree -Path $_.FullName -Indent ($Indent + 4)
    }

    # 列出文件
    Get-ChildItem -Path $Path -File -Force | Sort-Object Name | ForEach-Object {
        $filePrefix = ' ' * ($Indent + 4) + '|-- '
        Add-Content -Path $OutputFile -Value "$filePrefix$($_.Name)"
    }
}

# ==== 执行 ====
# 新建/清空输出文件
Set-Content -Path $OutputFile -Value "" -Force
# 开始构建树
Show-Tree -Path $RootPath
