# Excel 6-Row Header Format (data_table_exporter)

**Date**: 2025-06 (session 3 of exporter rewrite)

## Format
```
Row 1: field names
Row 2: proto types (int32, string, uint32, double, …)
Row 3: struct — repeated | set | map_key | map_value | message:Name | (empty)
Row 4: owner — server | client | common | design | constants_name
Row 5: options — space-separated: bit_index key multi fk:T gfk:T expr:type expr_params:a,b
Row 6: comment — human-readable
Row 7+: data
```

## Key Design Decisions
- **struct row explicit**: No more name-pattern inference. `repeated`, `set`, `map_key`/`map_value`, `message:Name` are all declared explicitly per column.
- **ALL columns tagged**: For sets, ALL columns (not just first) get `set`. For maps, ALL pairs get `map_key`/`map_value`. For messages, ALL member columns get `message:Name`.
- **options consolidated**: Old R5 (multi), R6 (table_key/bit_index), R7 (expr_type), R8 (expr_params), R9 (fk), R10 (gfk) all merged into one space-separated row.
- **Proto dedup**: Template uses `seen.update()` dict trick + `table.col_to_group` property to skip duplicate names and group members.
- **MapField added**: `TableSchema.maps` dict collects proto `map<K,V>` fields (tag, immunetag pattern).
- **col_to_group property**: `TableSchema.col_to_group` returns `{col_index: group_name}` for fast group membership lookup.

## struct keyword reference

| Keyword | Proto output | JSON output |
|---------|-------------|-------------|
| (empty) | `type name = N;` | `"name": value` |
| `repeated` | `repeated type name = N;` | `"name": [v1, v2, ...]` |
| `set` | `map <type, bool> name = N;` | `"name": {"v1": true, ...}` |
| `map_key` + `map_value` | `map <K, V> prefix = N;` | `"prefix": {"k1": v1, ...}` |
| `message:Name` | `repeated TableNameName { ... } = N;` | `"name": [{...}, ...]` |

## Migration Script
- `migrate_xlsx.py` in `tools/data_table_exporter/` — run once, converts 19-row to 6-row format.
- Uses range-merging algorithm for message group detection.
- Old R3 map_type markers propagated to all same-name/pair columns.

## Files Changed
- `core/schema.py`: ColumnDef has `struct`, `options`, `comment` with backward-compat properties
- `core/excel_reader.py`: Struct-driven `_detect_layout`, `col_to_group` in `_build_row`
- `core/config_loader.py`: Removed `field_info_end_row`
- `exporter_config.yaml`: `data_begin_row: 7`, 6 metadata rows
- `templates/proto_table.proto.j2`: Dedup + maps + group exclusion
