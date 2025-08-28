#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


def compile_protobuf_files(source_dir, protobuf_include_dir, output_dir):
    """
    Compile all .proto files in the source directory to C++ using protoc compiler in one command.

    Args:
    - source_dir (str): Directory containing .proto files.
    - protobuf_include_dir (str): Directory containing protobuf headers (usually protobuf installation directory).
    - output_dir (str): Directory where generated C++ files will be placed.
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

    # Construct protoc command
    command = [
        "protoc",
        f"--proto_path={source_dir}",
        f"--proto_path={protobuf_include_dir}",
        f"--cpp_out={output_dir}",
    ] + proto_files

    try:
        result = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        logger.info("Compiled all proto files successfully.")
        if result.stdout:
            logger.info(result.stdout)
        if result.stderr:
            logger.warning(result.stderr)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to compile proto files. Error: {e.stderr}")


if __name__ == "__main__":
    source_dir = "generated/proto/"  # Source directory containing .proto files
    protobuf_include_dir = "../../third_party/grpc/third_party/protobuf/src"  # Protobuf headers directory
    output_dir = source_dir + "cpp/"  # Output directory for generated C++ files

    compile_protobuf_files(source_dir, protobuf_include_dir, output_dir)

    source_dir = "generated/proto/tip/"  # Source directory containing .proto files
    protobuf_include_dir = "../../third_party/grpc/third_party/protobuf/src"  # Protobuf headers directory
    output_dir = source_dir + "cpp/tip/"  # Output directory for generated C++ files

    compile_protobuf_files(source_dir, protobuf_include_dir, output_dir)