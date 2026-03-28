# 根目录与工具目录审计 (2026-03-24)

## 关键发现：robot_client 目录
- **状态**：正在从 git 中删除（已暂存 `tools/robot_client/proto/` 下所有 *.pb.go 文件的删除）
- **问题**：文档引用了 `tools/robot_client/main.go`，但该路径不存在
- **实际路径**：根目录下的 `robot/go.mod`
- **需要操作**：更新所有文档引用

## 根目录下的过时/可疑文件
1. **game.slnLaunch.user**：VS 用户特定配置，应加入 .gitignore
2. **Dockerfile**：已过时（引用了 "turn-based-game"，旧的 Linux 构建）
3. **lib/ 下的过时 .exp 文件**：大量 Windows 构建产物

## 目录重复问题
1. **scripts/ vs tools/scripts/**：scripts/ 是旧版；tools/scripts/ 是当前版
2. **lib/ vs third_party/**：用途不同（编译输出 vs 源码）——不是真正的重复
3. **robot/ vs tools/robot/**：命名容易混淆——需要澄清

## bin/ 目录内容
- 大部分 .exe、.pdb、.log、.txt 已在 .gitignore 中
- 配置：bin/config/ 和 bin/etc/ 包含合法的配置文件

## 待办事项
1. 修复 AGENTS.md 文件中的 robot_client 引用
2. 将 scripts/ 迁移到 tools/scripts/ 或者删除
3. 将 game.slnLaunch.user 添加到 .gitignore
4. 更新或标记 Dockerfile 为旧版
5. 审查 bin/ 输出文件
6. 验证 lib/ 产物是否应该纳入版本控制
