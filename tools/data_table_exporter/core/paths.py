# paths.py
from pathlib import Path

# 常量定义
DESIGNER = "designer"
CONSTANTS_NAME = "constants_name"
SERVER_TYPE = "server"
COMMON_TYPE = "common"
SERVER_GEN_TYPE = [SERVER_TYPE, COMMON_TYPE]

# 项目根目录（建议使用绝对路径；可根据实际调整）
PROJECT_DIR = Path("../../../")

GENERATOR_DIR = PROJECT_DIR / "generated"

# 项目结构路径
PROJECT_GENERATED_JSON_DIR = GENERATOR_DIR / "generated_tables"
PROJECT_GENERATED_CODE_DIR = GENERATOR_DIR / "generated_code"
DATA_TABLES_DIR = PROJECT_DIR / "data_tables"

# Go 代码生成相关
PROJECT_GENERATED_CODE_TABLE_GO_DIR = PROJECT_DIR / "go" / "generated" / "table"

# Proto 文件路径
PROTO_DIR = PROJECT_GENERATED_CODE_DIR / "proto"
PROJECT_GENERATED_CODE_PROTO_TIP_DIR = PROTO_DIR / "tip"
PROJECT_GENERATED_CODE_PROTO_OPERATOR_DIR = PROTO_DIR / "operator"

# 特定 Excel 文件路径
PROJECT_OPERATOR_XLSX = DATA_TABLES_DIR / "operator" / "Operator.xlsx"

# 存储目录结构
GENERATOR_STORAGE_DIR = Path("state")
GENERATOR_STORAGE_OPERATOR_DIR = GENERATOR_STORAGE_DIR / "operator"
GENERATOR_STORAGE_OPERATOR_FILE_DIR = GENERATOR_STORAGE_OPERATOR_DIR / "id_pool.json"

GENERATOR_TABLE_INDEX_MAPPING_DIR = GENERATOR_STORAGE_DIR / "mapping" / "table_index_mapping"
GENERATOR_TIP_MAPPING_DIR = GENERATOR_STORAGE_DIR / "mapping" / "tip_enum_ids"

# 假设 PROJECT_DIR 已经是 Path 类型
GENERATED_CODE_DIR = GENERATOR_DIR /  "generated_code"
CPP_GEN_DIR = PROJECT_DIR / "cpp" / "generated" / "table"
GO_GEN_DIR = PROJECT_DIR / "go"

# 源目录路径
SRC_CPP = GENERATED_CODE_DIR / "cpp"
SRC_GO = GENERATED_CODE_DIR / "go"
SRC_CPP_ID_BIT = SRC_CPP / "bit_index"
SRC_CPP_CONSTANTS = SRC_CPP / "constants"
SRC_CPP_TABLE_ID = SRC_CPP / "table_id"
SRC_GO_TABLE_ID = SRC_GO / "table"/ "table_id"
SRC_PROTO_CPP = GENERATED_CODE_DIR / "proto" / "cpp"
SRC_PROTO_CPP_OPERATOR = SRC_PROTO_CPP / "operator"
SRC_PROTO_CPP_TIP = SRC_PROTO_CPP / "tip"
SRC_PROTO_GO = GENERATED_CODE_DIR / "proto" / "go" / "pb" / "game"
SRC_GO_ID_BIT = SRC_GO / "go" / "bit_index"
SRC_GO_CONSTANTS = SRC_GO /  "constants"

# 目标目录路径
DST_CPP_CODE = CPP_GEN_DIR / "code"
DST_GO_CODE = GO_GEN_DIR / "generated"
DST_CPP_BIT = DST_CPP_CODE / "bit_index"
DST_CPP_CONSTANTS = DST_CPP_CODE / "constants"
DST_PROTO_CPP = CPP_GEN_DIR / "proto"
DST_PROTO_CPP_OPERATOR = DST_PROTO_CPP / "operator"
DST_PROTO_CPP_TIP = DST_PROTO_CPP / "tip"
DST_PROTO_GO = DST_GO_CODE / "pb" / "table"
DST_CPP_TABLE_ID = DST_CPP_CODE / "table_id"
DST_GO_TABLE_ID = GO_GEN_DIR / "table_id"