#!/usr/bin/env python
# coding=utf-8
import os
import subprocess
import logging
from pathlib import Path
import paths

# Set up logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Base directory references
GEN_CODE_DIR = paths.PROJECT_GENERATED_CODE_DIR
GEN_JSON_DIR = paths.PROJECT_GENERATED_JSON_DIR
PROJECT_DIR = paths.PROJECT_DIR

# List of directories to create (Path 类型)
directories = [
    GEN_CODE_DIR / "cpp",
    GEN_CODE_DIR / "proto",
    GEN_CODE_DIR / "proto" / "cpp",
    GEN_CODE_DIR / "proto" / "go",
    paths.DST_PROTO_GO,
    GEN_JSON_DIR / "json",
    paths.SRC_CPP,
    paths.SRC_GO,
    paths.SRC_CPP_ID_BIT_INDEX,
    paths.SRC_CPP_CONSTANTS,
    paths.SRC_PROTO_CPP,
    paths.SRC_PROTO_CPP_OPERATOR,
    paths.SRC_PROTO_CPP_TIP,
    paths.SRC_GO_ID_BIT,
    paths.SRC_GO_CONSTANTS,
    paths.DST_CPP_CODE,
    paths.DST_CPP_ID_BIT_INDEX,
    paths.DST_CPP_CONSTANTS,
    paths.DST_PROTO_CPP,
    paths.DST_PROTO_CPP_OPERATOR,
    paths.DST_PROTO_CPP_TIP,
    paths.DST_PROTO_GO,
    paths.SRC_CPP_TABLE_ID,
    paths.DST_CPP_TABLE_ID,
    paths.SRC_GO_TABLE_ID,
    paths.SRC_GO,
]

# Create directories if they don't exist
for directory in directories:
    if not directory.exists():
        directory.mkdir(parents=True, exist_ok=True)
        logger.info(f"Created directory: {directory}")
from md5copy_utils import md5_copy  # 放在你的 import 区域

# ...

# 继续保留原先生成文件的命令
commands = [
    "python gen_json_from_xlsx.py",
    "python gen_proto_from_xlsx.py",
    "python gen_xlsx_to_tip_proto.py",
    "python gen_xls_to_operator_proto.py",
    "python gen_xls_to_language_config_file.py",
    "python gen_xls_cpp_table_id.py",
    "python gen_xls_go_table_id.py",
    "python gen_cpp_pb.py",
    "python gen_go_pb.py",
    "python gen_constants_from_xlsx.py",
    "python generate_xlsx_to_id_bit_index.py",
]

# 执行命令行生成任务
for command in commands:
    try:
        logger.info(f"Running command: {command}")
        result = subprocess.run(command, shell=True, check=True, capture_output=True, text=True)
        if result.stdout:
            logger.info(result.stdout.strip())
        if result.stderr:
            if "ERROR" in result.stderr:
                logger.error(result.stderr.strip())
            else:
                logger.info(result.stderr.strip())
    except subprocess.CalledProcessError as e:
        logger.error(f"Error running command '{command}': {e}")

# 执行 md5copy 复制任务（替代 md5tool.py）
copy_tasks = [
    (paths.SRC_CPP, paths.DST_CPP_CODE),
    (paths.SRC_CPP_ID_BIT_INDEX, paths.DST_CPP_ID_BIT_INDEX),
    (paths.SRC_CPP_CONSTANTS, paths.DST_CPP_CONSTANTS),
    (paths.SRC_PROTO_CPP, paths.DST_PROTO_CPP),
    (paths.SRC_PROTO_CPP_OPERATOR, paths.DST_PROTO_CPP_OPERATOR),
    (paths.SRC_PROTO_CPP_TIP, paths.DST_PROTO_CPP_TIP),
    (paths.SRC_CPP_TABLE_ID, paths.DST_CPP_TABLE_ID),
]

# 转换为Path对象
grpc_service_dir = Path(paths.GRPC_SERVICE_DIR)
dst_go_grpc = Path(paths.DST_GO_GRPC)

# 获取第一层子目录（不递归）
for item in grpc_service_dir.iterdir():
    # 只处理目录，且是直接子目录（第一层）
    if item.is_dir():
        # 源路径：第一层子目录
        src_dir = paths.SRC_GO
        # 目标路径：DST_GO_GRPC + 子目录名称
        dst_dir = dst_go_grpc / item.name

        # 添加到复制任务
        copy_tasks.append((src_dir, dst_dir))
        logger.info(f"Added copy task: {src_dir} -> {dst_dir}")

for src, dst in copy_tasks:
    logger.info(f"Copying from {src} to {dst} with MD5 check...")
    md5_copy(str(src), str(dst))
