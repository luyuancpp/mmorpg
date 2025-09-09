# constants.py
from pathlib import Path

# 常量定义
DESIGNER = "designer"
CONSTANTS_NAME = "constants_name"
SERVER_TYPE = "server"
COMMON_TYPE = "common"
SERVER_GEN_TYPE = [SERVER_TYPE, COMMON_TYPE]

# 项目根目录（建议使用绝对路径；可根据实际调整）
PROJECT_DIR = Path(__file__).resolve().parent.parent.parent  # ../../../

# 项目结构路径
PROJECT_GENERATED_JSON_DIR = PROJECT_DIR / "generated_tables"
PROJECT_GENERATED_CODE_DIR = PROJECT_DIR / "generated_code"
XLSX_DIR = PROJECT_DIR / "data_tables"

# C++ 代码生成相关
GENERATOR_TABLE_INDEX_DIR = PROJECT_GENERATED_CODE_DIR / "cpp" / "cpp_table_id_bit_index"
GENERATOR_CONSTANTS_NAME_DIR = PROJECT_GENERATED_CODE_DIR / "cpp" / "cpp_table_id_constants_name"
PROJECT_GENERATED_CODE_CPP_DIR = PROJECT_GENERATED_CODE_DIR / "cpp"

# Go 代码生成相关
GENERATOR_TABLE_INDEX_GO_DIR = PROJECT_GENERATED_CODE_DIR / "go" / "go_table_id_bit_index"
GENERATOR_CONSTANTS_NAME_GO_DIR = PROJECT_GENERATED_CODE_DIR / "go" / "go_table_id_constants_name"
PROJECT_GENERATED_CODE_GO_DIR = PROJECT_GENERATED_CODE_DIR / "go"

# Proto 文件路径
PROTO_DIR = PROJECT_GENERATED_CODE_DIR / "proto"
PROJECT_GENERATED_CODE_PROTO_TIP_DIR = PROTO_DIR / "tip"
PROJECT_GENERATED_CODE_PROTO_OPERATOR_DIR = PROTO_DIR / "operator"

# 特定 Excel 文件路径
PROJECT_OPERATOR_XLSX = XLSX_DIR / "operator" / "Operator.xlsx"

# 存储目录结构
GENERATOR_STORAGE_DIR = Path("state")
GENERATOR_STORAGE_OPERATOR_DIR = GENERATOR_STORAGE_DIR / "operator"
GENERATOR_STORAGE_OPERATOR_FILE_DIR = GENERATOR_STORAGE_OPERATOR_DIR / "id_pool.json"

GENERATOR_TABLE_INDEX_MAPPING_DIR = GENERATOR_STORAGE_DIR / "mapping" / "table_index_mapping"
GENERATOR_TIP_MAPPING_DIR = GENERATOR_STORAGE_DIR / "mapping" / "tip_enum_ids"
