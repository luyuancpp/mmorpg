# ==== 配置部分 ====
$RootPath = "D:\game\luyuan\pkg\mmorpg\cpp"               # 根目录
$ExcludePaths = @(
    "D:\game\luyuan\pkg\mmorpg\cpp\libs\base\muduo_windows"
)
$OutputFile = "D:\game\luyuan\pkg\mmorpg\cpp\tree.txt"        # 输出文件路径

# ==== 主函数 ====
function Show-Tree {
    param (
        [string]$Path,
        [int]$Indent = 0
    )

    if ($ExcludePaths -contains $Path) {
        return
    }

    $prefix = ' ' * $Indent + '|-- '
    Add-Content -Path $OutputFile -Value "$prefix$(Split-Path $Path -Leaf)"

    # 列出子目录
    Get-ChildItem -Path $Path -Directory -Force | ForEach-Object {
        Show-Tree -Path $_.FullName -Indent ($Indent + 4)
    }

    # 列出文件
    Get-ChildItem -Path $Path -File -Force | ForEach-Object {
        $filePrefix = ' ' * ($Indent + 4) + '|-- '
        Add-Content -Path $OutputFile -Value "$filePrefix$($_.Name)"
    }
}

# ==== 执行 ====
# 清空输出文件
Clear-Content -Path $OutputFile -ErrorAction SilentlyContinue
# 开始构建树
Show-Tree -Path $RootPath
