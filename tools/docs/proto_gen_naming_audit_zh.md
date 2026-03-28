# Proto-Gen 命名审计

本文档是从 `pbgen` 到用户侧名称 `proto-gen` 以及内部规范名称 `protogen` 迁移后的当前命名状态快照。

## 当前结果

- 用户侧命令名称现为 `proto-gen`。
- 用户侧二进制文件名称现为 `proto-gen.exe`。
- 规范的内部目录/模块名称现为 `protogen`。
- 历史 `pbgen` 命令名称仍作为兼容别名可用。
- 历史 `pbgen.exe` 仍作为兼容副本可用。
- VS Code 任务标签和启动标签优先使用 `proto-gen`。
- 帮助文本和主要工具文档引导新用户首先使用 `proto-gen`。

## 已完成的用户侧变更

- 在 `tools/scripts/dev_tools.ps1` 中添加了主命令：
  - `proto-gen-build`
  - `proto-gen-run`
- 构建现在生成：
  - `proto-gen.exe`
  - `pbgen.exe`（兼容副本）
- 保留了兼容别名：
  - `pbgen-build`
  - `pbgen-run`
- 添加了帮助输出以便命令发现。
- 更新了根目录和工具文档，在示例中优先使用 `proto-gen`。
- 更新了 VS Code 任务以使用 `proto-gen:*` 标签。
- 更新了启动配置名称以使用 `proto-gen`。

## 有意保留的兼容项

以下内容是有意未变更的，不应被视为遗漏的清理项：

- 目录名称：
  - `tools/proto_generator/protogen/`
  - `tools/proto/protogen/`
  - `tools/docs/protogen/`
- 二进制文件和环境变量名：
  - `proto-gen.exe`
  - `pbgen.exe`
  - `PROTOGEN_ENABLE_PPROF`
  - `PBGEN_ENABLE_PPROF`
- 内部源码引用：
  - Go 模块/导入路径如 `protogen/...`
- 生成和归档输出：
  - `generated/` 下已跟踪的生成文件
  - `tools/docs/protogen/` 下的归档日志

## 禁止操作区域

以下区域在日常清理工作中不应被重命名：

- `generated/**`
- `tools/proto_generator/protogen/**`
- `tools/docs/protogen/**`
- 现有本地工作流引用的兼容目录或二进制文件

## 已完成的低风险清理

- 根目录 `.gitignore` 涵盖了生成器的当前和历史本地产物路径。
- `tools/.gitignore` 现在忽略了规范当前路径下的本地二进制文件：
  - `proto_generator/protogen/*.exe`
  - `proto_generator/protogen/*.dll`

## 仍需专门迁移的内容

以下是独立的项目，而非清理任务：

- 移除 `pbgen.exe` 兼容输出
- 移除 `pbgen-build` / `pbgen-run` 兼容别名
- 移除对 `PBGEN_ENABLE_PPROF` 的支持

## 推荐的判断方式

如果未来搜索仍发现 `pbgen`，请按以下顺序分类：

1. 是否出现在用户侧命令示例或标签中？
2. 是否属于明确的兼容边界？
3. 是否为生成或归档输出？
4. 是否为需要真正迁移计划的内部导入/路径？

仅第 1 类应被视为日常清理项。
