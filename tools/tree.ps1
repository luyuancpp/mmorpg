# ==== 配置部分 ====
$RootPath = "D:\game\luyuan\mmorpg1\proto"               # 根目录
$ExcludePaths = @(

)

$OutputFile = "D:\game\luyuan\\mmorpg1\tools\tree.txt"  # 输出文件路径
$MaxDepth = 100  # 最大递归深度（含根目录）

# ==== 主函数 ====
function Show-Tree {
    param (
        [string]$Path,
        [int]$Indent = 0,
        [int]$Depth = 20
    )

    # 检查是否在排除路径中
    foreach ($exclude in $ExcludePaths) {
        if ($Path -like "$exclude*") {
            return
        }
    }

    $prefix = ' ' * $Indent + '|-- '
    Add-Content -Path $OutputFile -Value "$prefix$(Split-Path $Path -Leaf)"

    # 如果已达最大深度，则停止递归
    if ($Depth -ge $MaxDepth) {
        return
    }

    # 列出子目录
    Get-ChildItem -Path $Path -Directory -Force | Sort-Object Name | ForEach-Object {
        Show-Tree -Path $_.FullName -Indent ($Indent + 4) -Depth ($Depth + 1)
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
