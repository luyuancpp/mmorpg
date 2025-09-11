#!/usr/bin/env python
# coding=utf-8

import subprocess
import logging
from pathlib import Path
import constants

# Set up logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Base directory references
GEN_CODE_DIR = constants.PROJECT_GENERATED_CODE_DIR
GEN_JSON_DIR = constants.PROJECT_GENERATED_JSON_DIR
PROJECT_DIR = constants.PROJECT_DIR

# List of directories to create (Path 类型)
directories = [
    GEN_CODE_DIR / "cpp",
    GEN_CODE_DIR / "proto",
    GEN_CODE_DIR / "proto" / "cpp",
    GEN_CODE_DIR / "proto" / "go",
    constants.DST_PROTO_GO,
    GEN_CODE_DIR / "cpp_table_id_constants_name",
    GEN_JSON_DIR / "json",
    PROJECT_DIR / "cpp" / "generated" / "table" / "code",
    PROJECT_DIR / "cpp" / "generated" / "table" / "code" / "constants",
    PROJECT_DIR / "cpp" / "generated" / "table" / "code" / "bit_index",
    PROJECT_DIR / "cpp" / "generated" / "table" / "proto" / "operator",
    PROJECT_DIR / "cpp" / "generated" / "table" / "proto" / "tip",
]

# Create directories if they don't exist
for directory in directories:
    if not directory.exists():
        directory.mkdir(parents=True, exist_ok=True)
        logger.info(f"Created directory: {directory}")

# Define commands to execute
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

    f"python md5tool.py md5copy {constants.SRC_CPP} {constants.DST_CPP_CODE}",
    f"python md5tool.py md5copy {constants.SRC_CPP_ID_BIT} {constants.DST_CPP_BIT}",
    f"python md5tool.py md5copy {constants.SRC_CPP_CONSTANTS} {constants.DST_CPP_CONSTANTS}",
    f"python md5tool.py md5copy {constants.SRC_PROTO_CPP} {constants.DST_PROTO_CPP}",
    f"python md5tool.py md5copy {constants.SRC_PROTO_CPP_OPERATOR} {constants.DST_PROTO_CPP_OPERATOR}",
    f"python md5tool.py md5copy {constants.SRC_PROTO_CPP_TIP} {constants.DST_PROTO_CPP_TIP}",
    f"python md5tool.py md5copy {constants.SRC_PROTO_GO} {constants.DST_PROTO_GO}",
]


# Execute commands and capture return codes
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
