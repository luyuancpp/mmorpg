#!/bin/bash
set -euo pipefail

# 只检出父仓 gitlink 记录的提交，不跟踪分支尖端；任一步失败立即退出。
REPO_ROOT="$(git rev-parse --show-toplevel)"
echo ">>> 按 gitlink 同步子模块: $REPO_ROOT"
git -C "$REPO_ROOT" submodule sync --recursive
git -C "$REPO_ROOT" submodule update --init --recursive --checkout
echo '>>> 子模块已按父仓记录的提交检出。'
