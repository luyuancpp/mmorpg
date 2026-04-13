# Excel 6 行表头格式 (data_table_exporter)

**日期**: 2025-06（导出器重写第 3 次会话）

## 格式
```
Row 1: field names          （字段名）
Row 2: proto types           （proto 类型：int32, string, uint32, double, …）
Row 3: struct                （结构：repeated | set | map_key | map_value | message:Name | 空）
Row 4: owner                 （归属：server | client | common | design | constants_name）
Row 5: options               （选项，空格分隔：bit_index key multi fk:T gfk:T expr:type expr_params:a,b）
Row 6: comment               （注释：人类可读说明）
Row 7+: data                 （数据行）
```

## 关键设计决策
- **struct 行显式声明**：不再通过名称模式推断。`repeated`、`set`、`map_key`/`map_value`、`message:Name` 均在每列显式声明。
- **所有列都需要标记**：对于 set，所有列（不仅仅是第一列）都标记 `set`。对于 map，所有键值对列都标记 `map_key`/`map_value`。对于 message，所有成员列都标记 `message:Name`。
- **options 合并**：旧的 R5 (multi)、R6 (table_key/bit_index)、R7 (expr_type)、R8 (expr_params)、R9 (fk)、R10 (gfk) 全部合并为一行，空格分隔。
- **Proto 去重**：模板使用 `seen.update()` 字典技巧 + `table.col_to_group` 属性来跳过重复名称和分组成员。
- **MapField 新增**：`TableSchema.maps` 字典收集 proto `map<K,V>` 字段（tag、immunetag 模式）。
- **col_to_group 属性**：`TableSchema.col_to_group` 返回 `{col_index: group_name}`，用于快速查找分组归属。

## struct 关键字参考

| 关键字 | Proto 输出 | JSON 输出 |
|---------|-------------|-------------|
| （空） | `type name = N;` | `"name": value` |
| `repeated` | `repeated type name = N;` | `"name": [v1, v2, ...]` |
| `set` | `map <type, bool> name = N;` | `"name": {"v1": true, ...}` |
| `map_key` + `map_value` | `map <K, V> prefix = N;` | `"prefix": {"k1": v1, ...}` |
| `message:Name` | `repeated TableNameName { ... } = N;` | `"name": [{...}, ...]` |

## 迁移脚本
- `migrate_xlsx.py` 位于 `tools/data_table_exporter/`——运行一次，将 19 行格式转换为 6 行格式。
- 使用区间合并算法进行 message 分组检测。
- 旧的 R3 map_type 标记会传播到所有同名/配对列。

## 变更文件
- `core/schema.py`：ColumnDef 包含 `struct`、`options`、`comment`，具有向后兼容属性
- `core/excel_reader.py`：基于 struct 驱动的 `_detect_layout`，`_build_row` 中使用 `col_to_group`
- `core/config_loader.py`：移除了 `field_info_end_row`
- `exporter_config.yaml`：`data_begin_row: 7`，6 行元数据
- `templates/proto_table.proto.j2`：去重 + map 支持 + 分组排除
