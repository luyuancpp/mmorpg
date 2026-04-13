# 文档与报告

工具和构建过程的历史日志、报告和文档。

## 内容

- `protogen/` — Proto 生成器运行日志和报告（历史存档）
- `proto_gen_naming_audit.md` / `proto_gen_naming_audit_zh.md` — `proto-gen`、`protogen` 与保留兼容别名的当前命名状态快照
- `proto_gen_naming_migration.md` / `proto_gen_naming_migration_zh.md` — `proto-gen` / `protogen` / `pbgen` 的命名边界、保留的有意引用和迁移检查清单
- 其他工具相关文档和输出

## 组织方式

按工具组织文档：
- 每个主要工具一个子目录
- 每次运行或报告一个文件
- 包含时间戳以便历史追踪

## 说明

- 本目录中的文档用于参考和调试
- 日志通常在版本控制中被忽略（参见 `../.gitignore`）
- 重要报告或里程碑日志应归档以供日后参考
