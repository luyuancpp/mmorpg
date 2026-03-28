# Data Table Exporter

Excel → multi-language code generator for game configuration tables.

## Quick Start

```bash
pip install -r requirements.txt
python run.py                        # default config
python run.py path/to/config.yaml    # custom config
```

## What It Does

Reads `.xlsx` data tables under `data/` and generates:

| Output            | Languages       | Description                              |
|-------------------|-----------------|------------------------------------------|
| JSON data         | —               | Runtime-loadable data files              |
| `.proto` messages | —               | Protobuf message definitions             |
| Compiled protobuf | C++, Go         | `.pb.h/.pb.cc` and `.pb.go`              |
| Config managers   | C++, Go, Java   | Typed table-manager classes              |
| Named constants   | C++, Go, Java   | `constexpr` / `const` / `static final`   |
| Table-ID enums    | C++, Go, Java   | Per-row ID enumerations                  |
| Bit-index maps    | C++, Go         | Stable ID → bit-position mappings        |
| Operator enums    | Proto           | From `Operator.xlsx`                     |
| Tip error enums   | Proto           | From `Tip.xlsx`                          |

## Configuration

All settings live in `exporter_config.yaml`:

- **excel**: source data directory, special Excel paths, metadata row numbers
- **output**: generated file root directories
- **protoc**: protoc binary path and include directories
- **languages**: per-language enable flags, output directories, deploy targets
- **constant_tables**: which tables generate table-ID enums

## Excel Format

| Row | Purpose                           | Example             |
|-----|-----------------------------------|---------------------|
| 1   | Column names (first = `id`)       | `id`, `name`, `hp`  |
| 2   | Data types                        | `int32`, `string`    |
| 3   | Map type                          | `map_key`, `set`     |
| 4   | Owner                             | `server`, `client`   |
| 5   | Multi-key flag                    | `multi`              |
| 6   | Table key / bit_index marker      | `table_key`          |
| 7   | Expression type                   | `double`             |
| 8   | Expression param names            | `atk,def`            |
| 9   | **Foreign key** (NEW)             | `fk:Reward`          |
| 10  | **Group foreign key** (NEW)       | `gfk:Item`           |
| 11–19 | Reserved / planner annotations  |                      |
| 20+ | **Data rows**                     |                      |

### Foreign Key Syntax

- `fk:TableName` → references `TableName.id`
- `fk:TableName.column` → references `TableName.column`
- `gfk:TableName` → this column group references `TableName`

Foreign keys are validated at generation time and annotated in output code.

## Project Structure

```
data_table_exporter/
├── exporter_config.yaml      # Central configuration
├── run.py                    # Entry point
├── requirements.txt          # Python dependencies
├── core/
│   ├── config_loader.py      # YAML config loading & path resolution
│   ├── schema.py             # TableSchema / ColumnDef data models
│   ├── excel_reader.py       # Excel → schema + data extraction
│   ├── foreign_key.py        # FK validation across tables
│   ├── type_mapping.py       # C++ / Go / Java type conversions
│   ├── file_utils.py         # File I/O + MD5-based copy
│   ├── orchestrator.py       # Pipeline coordinator
│   └── generators/
│       ├── json_gen.py       # → JSON data files
│       ├── proto_gen.py      # → .proto + protoc compilation
│       ├── config_gen.py     # → config manager classes
│       ├── constants_gen.py  # → named constants
│       ├── table_id_gen.py   # → table-ID enums
│       ├── bit_index_gen.py  # → bit-index mappings
│       └── enum_gen.py       # → operator / tip enums
├── templates/                # Jinja2 templates (all .j2)
│   ├── cpp_*.j2              # C++ templates
│   ├── go_*.j2               # Go templates
│   ├── java_*.j2             # Java templates (NEW)
│   ├── proto_table.proto.j2  # Proto message template
│   ├── operator_enum.proto.j2
│   └── tip_enum.proto.j2
├── state/                    # Persistent ID mappings
└── mapping/                  # Reference mapping data
```

## Adding a New Language

1. Add a section under `languages:` in `exporter_config.yaml`
2. Add type mappings in `core/type_mapping.py`
3. Create templates under `templates/{lang}_*.j2`
4. Add generation logic in the relevant generator (e.g., `config_gen.py`)
5. Wire it into `orchestrator.py` if needed

## Pipeline Steps

1. **Read schemas** — parse all `.xlsx` metadata into `TableSchema` objects
2. **Validate FKs** — check all foreign key references are valid
3. **Generate JSON** — data rows → `.json` files
4. **Generate protos** — schema → `.proto` message definitions
5. **Generate enums** — Operator/Tip → proto enum files
6. **Compile protos** — `protoc` → C++ / Go compiled outputs
7. **Generate config** — schema → config manager classes (C++/Go/Java)
8. **Generate IDs** — table-ID enums, constants, bit-index maps
9. **Deploy** — MD5-checked copy to final destinations