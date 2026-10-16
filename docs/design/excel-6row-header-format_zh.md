# 配置表格式：权威 proto schema 与 Excel 数据

**更新日期**：2026-10-15

> 旧的“6 行表头”说明已过期。2026-09-03 的提交 `722bac105` 已将 schema 迁入
> `data/schema/*.proto`，当时遗漏了本文件及同名文档入口。本次按现行实现补齐说明，
> 保留文件名以兼容既有链接。维护规范与全表索引见 [data/AGENTS.md](../../data/AGENTS.md)。

## 当前 Excel 格式

普通配置表的 xlsx 只提供列名与数据；类型、结构、归属、选项、字段号及注释由权威 schema 定义。

| 行 | 内容 |
|---|---|
| 第 1 行 | 英文列名，导表器据此绑定 schema 字段 |
| 第 2–4 行 | 空，不再填写类型、owner 或选项 |
| 第 5 行 | 中文说明，由 schema 注释首行投影生成；在 Excel 中修改不会改变 schema，重新投影会覆盖 |
| 第 6 行起 | 数据；仓库配置为 `data_begin_row: 6` |

`data/operator/Operator.xlsx` 与 `data/tip/Tip.xlsx` 走独立的枚举生成路径，格式不适用本节。

## schema 的事实源

每张普通配置表对应 `data/schema/<sheet>_table.proto`，例如
[mission_table.proto](../../data/schema/mission_table.proto)。可用选项与含义以
[cfg_options.proto](../../data/schema/cfg_options.proto) 为准，导表器运行时直接解析这份词表。

| 内容 | 定义位置或写法 |
|---|---|
| 表与主键 | message 上的 `(cfg_sheet)`、`(cfg_primary_key)` |
| 字段类型与字段号 | proto 字段声明；字段号显式固定，不随 Excel 列位重算 |
| 数组、映射、子消息 | proto 的 `repeated`、`map`、子消息；非标量用 `(cfg_slots)` 显式声明容量，集合等形态由 `(cfg_shape)` 补充 |
| 列名绑定 | 缺省用 proto 字段名，必要时用 `(cfg_col)` 指定 |
| 归属 | `(cfg_owner)`，缺省 `common`；服务端导出归属为 `server`、`common` |
| 键、索引、位序 | `(cfg_key)`、`(cfg_multi)`、`(cfg_index)`、`(cfg_bit_index)` |
| 外键与表达式 | `(cfg_fk)`、`(cfg_gfk)`、`(cfg_expr_type)`、`(cfg_expr_param)` |
| 中文说明 | schema 中的 `//` 注释 |

权威 schema 由导表器读取，不直接进入各语言构建；`generated/code/proto/` 下的 proto
是导表产物，不能手改。

## 绑定与修改流程

导表器按工作簿首个 sheet 的名字匹配 `(cfg_sheet)`，再按第 1 行列名绑定字段。
工作簿文件名不承担表名匹配；调整物理列位不会改变其它字段的字段号。
首列仍必须是主键；数组、映射与子消息列表的整段物理列须保持完整，后续占位列的表头留空，
不能拆散槽位或单独重排组内成员。

1. 修改 xlsx 的列名或数据；新增普通配置表时首列必须为 `id`，第 6 行起填数据。
2. 同步对应权威 schema。新增字段取当前最大字段号加一；上线后不复用字段号，删字段写 `reserved`。
3. 按[导表器说明](../../tools/data_table_exporter/readme.md)运行导表。默认配置含客户端部署目标，执行范围须符合任务授权。
4. 运行 `py tools/data_table_exporter/tools/gen_schema_index.py` 更新配置表索引，并按受影响语言完成构建与测试。

schema 与源表不匹配会中止读取，生成流程不会继续。需要重建 Excel 中文说明时，
`tools/data_table_exporter/tools/strip_header_rows.py` 默认会从 schema 投影到第 5 行；
它会修改源表，日常改数据无需重跑迁移工具。

## 兼容路径与历史格式

[table_source.py](../../tools/data_table_exporter/core/table_source.py) 优先读取权威 schema；
没有匹配 schema 时，仍保留旧的 **5 行表头**读取路径，由配置指定列名、类型、owner、选项和注释行。
这是迁移兼容入口，新表按权威 schema 流程维护。

本文件过去描述的是更早的一版格式：第 1–6 行依次为列名、类型、独立 `struct`、owner、
选项、注释，第 7 行起数据。现行默认配置没有独立 `struct` 行，也不会自动识别该六行格式。
旧说明中的 `data_begin_row: 7` 与“运行 `migrate_xlsx.py` 转成六行表头”均已不适用于当前仓库。

当前配置与实现入口：

- [exporter_config.yaml](../../tools/data_table_exporter/exporter_config.yaml)：仓库行号配置与部署目标。
- [schema_proto.py](../../tools/data_table_exporter/core/schema_proto.py)：解析权威 schema 并绑定 Excel 列。
- [excel_reader.py](../../tools/data_table_exporter/core/excel_reader.py)：按数据起始行配置读取数据并展开结构。
- [导表器说明](../../tools/data_table_exporter/readme.md)：导表、沙盒比对与迁移工具用法。
