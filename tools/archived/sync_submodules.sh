#!/bin/bash

# 确保在项目根目录
cd "$(git rev-parse --show-toplevel)"

# 初始化所有子模块
git submodule update --init --recursive

# 遍历 .gitmodules 中所有子模块
git config -f .gitmodules --get-regexp path | while read -r key path; do
    name=${key#submodule.}
    name=${name%.path}

    # 获取子模块的分支配置
    branch=$(git config -f .gitmodules submodule.$name.branch)

    if [ -n "$branch" ]; then
        echo "设置子模块 $name 使用分支 $branch"

        # 设置 Git 本地配置（.git/config）
        git config submodule.$name.branch "$branch"

        # 进入子模块目录，切换分支，拉取更新
        (
            cd "$path" || exit
            git fetch origin
            git checkout "$branch"
            git pull origin "$branch"
        )
    else
        echo "子模块 $name 没有配置分支，跳过"
    fi
done
