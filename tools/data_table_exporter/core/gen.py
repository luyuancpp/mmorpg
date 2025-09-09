#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging
import constants

# Set up logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# List of directories to create
directories = [
    constants.PROJECT_GENERATED_CODE_DIR + "cpp/",
    constants.PROJECT_GENERATED_CODE_DIR + "proto/",
    constants.PROJECT_GENERATED_CODE_DIR + "proto/cpp/",
    constants.PROJECT_GENERATED_CODE_DIR + "proto/go/",
    constants.PROJECT_GENERATED_CODE_DIR + "cpp_table_id_constants_name/",
    constants.PROJECT_GENERATED_JSON_DIR + "json/",
    "../../cpp/generated/table/code/",
    "../../cpp/generated/table/code/constants/",
    "../../cpp/generated/table/code/bit_index",
    "../../cpp/generated/table/proto/operator/",
    "../../cpp/generated/table/proto/tip/",
]

# Create directories if they don't exist
for directory in directories:
    if not os.path.exists(directory):
        os.makedirs(directory)
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
    "python md5tool.py md5copy ../../generated_tables/cpp/ ../../cpp/generated/table/code/",
    "python md5tool.py md5copy ..././generated_tables/cpp_table_id_bit_index/ "
    "../../cpp/generated/table/code/bit_index/",
    "python md5tool.py md5copy ../../generated_tables/cpp_table_id_constants_name/ "
    "../../cpp/generated/table/code/constants/",
    "python md5tool.py md5copy ../../generated_tables/proto/cpp/ ../../cpp/generated/table/proto",
    "python md5tool.py md5copy ../../generated_tables/proto/cpp/operator/ ../../cpp/generated/table/proto/operator/",
    "python md5tool.py md5copy ../../generated_tables/proto/cpp/tip/ ../../cpp/generated/table/proto/tip/",
    "python md5tool.py md5copy ../../generated_tables/proto/go/pb/game/ ../../go/login/pb/game/",
    "python md5tool.py md5copy ../../generated_tables/proto/go/pb/game/ ../../go/db/pb/game/",
]

# Execute commands and capture return codes
for command in commands:
    try:
        logger.info(f"Running command: {command}")
        result = subprocess.run(command, shell=True, check=True, capture_output=True, text=True)
        if result.stdout:
            logger.info(result.stdout.strip())
        if result.stderr:
            logger.error(result.stderr.strip())
    except subprocess.CalledProcessError as e:
        logger.error(f"Error running command '{command}': {e}")