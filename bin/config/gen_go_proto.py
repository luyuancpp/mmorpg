#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


def compile_protobuf_files_go(source_dir, protobuf_include_dir, output_dir):
    """
    Compile all .proto files in the source directory to Go using protoc compiler in one command.

    Args:
    - source_dir (str): Directory containing .proto files.
    - protobuf_include_dir (str): Directory containing protobuf headers (usually protobuf installation directory).
    - output_dir (str): Directory where generated Go files will be placed.
    """
    proto_files = []

    for dirpath, _, filenames in os.walk(source_dir):
        for filename in filenames:
            if filename.endswith(".proto"):
                proto_files.append(os.path.join(dirpath, filename))

    if not proto_files:
        logger.warning("No .proto files found.")
        return

    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # 构造 protoc 命令，一次性编译所有 proto 文件
    command = [
        "protoc",
        f"-I={source_dir}",
        f"-I={protobuf_include_dir}",
        f"--go_out={output_dir}",
        f"--go-grpc_out={output_dir}",  # 如果你也要生成 gRPC 代码
    ] + proto_files

    try:
        result = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        logger.info("Compiled all proto files to Go successfully.")
        if result.stdout:
            logger.info(result.stdout)
        if result.stderr:
            logger.warning(result.stderr)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to compile proto files. Error: {e.stderr}")


if __name__ == "__main__":
    source_dir = "generated/proto"  # Source directory containing .proto files
    protobuf_include_dir = "../../third_party/grpc/third_party/protobuf/src"  # Protobuf headers directory
    output_dir = source_dir + "/go/"  # Output directory for generated Go files

    compile_protobuf_files_go(source_dir, protobuf_include_dir, output_dir)
