$ErrorActionPreference = 'Stop'

# 只检出父仓 gitlink 记录的提交，不跟踪分支尖端；已有本地修改由 Git 自行保护。
$taskRepoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) {
    throw '无法确定仓库根目录。'
}

Write-Host ">>> 按 gitlink 同步子模块: $taskRepoRoot" -ForegroundColor Cyan
git -C $taskRepoRoot submodule sync --recursive
if ($LASTEXITCODE -ne 0) {
    throw '同步子模块地址失败，未开始检出。'
}

git -C $taskRepoRoot submodule update --init --recursive --checkout
if ($LASTEXITCODE -ne 0) {
    throw '按 gitlink 初始化或检出子模块失败；请保留错误输出和本地修改。'
}

Write-Host '>>> 子模块已按父仓记录的提交检出。' -ForegroundColor Green
