#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging

# Set up logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# List of directories to create
directories = [
    "generated/cpp/",
    "generated/proto/",
    "generated/proto/cpp/",
    "generated/proto/go/",
    "generated/cpp_table_id_constants_name/",
    "generated/json",
    "../../pkg/config/src/pbc/",
    "../../pkg/config/src/",
    "../../pkg/config/src/cpp_table_id_constants_name/",
    "../../pkg/config/src/cpp_table_id_bit_index/"
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
    "python gen_xlsx_cpp_tip_to_proto.py",
    "python operator_xlsx_to_proto.py",
    "python gen_xls_to_cpp_config.py",
    "python gen_xls_cpp_id.py",
    "python gen_cpp_proto.py",
    "python gen_go_proto.py",
    "python gen_cpp_constants_from_xlsx.py",
    "python generate_xlsx_cpp_id_bit_index.py",
    "python md5tool.py md5copy ./generated/cpp/ ../../pkg/config/src/",
    "python md5tool.py md5copy ./generated/cpp_table_id_bit_index/ "
    "../../pkg/config/src/cpp_table_id_bit_index/",
    "python md5tool.py md5copy ./generated/cpp_table_id_constants_name/ "
    "../../pkg/config/src/cpp_table_id_constants_name/",
    "python md5tool.py md5copy ./generated/proto/cpp/ ../../pkg/config/src/pbc/",
    "python md5tool.py md5copy ./generated/proto/go/pb/game/ ../../node/login/pb/game/",
    "python md5tool.py md5copy ./generated/proto/go/pb/game/ ../../node/db/pb/game/",
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