#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)


def compile_protobuf_files(source_dir, include_dirs, output_dir):
    """
    Compile all .proto files in the source directory to C++ using protoc compiler.

    Args:
    - source_dir (str): Directory containing .proto files.
    - include_dirs (list[str]): Directories containing protobuf headers (and other .proto dependencies).
    - output_dir (str): Directory where generated C++ files will be placed.
    """
    proto_files = []
    for dirpath, _, filenames in os.walk(source_dir):
        for filename in filenames:
            if filename.endswith(".proto"):
                proto_files.append(os.path.join(dirpath, filename))

    if not proto_files:
        logger.warning(f"No .proto files found in {source_dir}")
        return

    os.makedirs(output_dir, exist_ok=True)

    command = ["protoc"]
    for inc in include_dirs:
        command.append(f"--proto_path={os.path.abspath(inc)}")

    command.append(f"--cpp_out={os.path.abspath(output_dir)}")
    command.extend(proto_files)

    try:
        result = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        logger.info(f"Compiled all proto files in {source_dir} successfully.")
        if result.stdout:
            logger.info(result.stdout)
        if result.stderr:
            logger.warning(result.stderr)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to compile proto files in {source_dir}. Error: {e.stderr}")


if __name__ == "__main__":
    # 公共的依赖目录（protobuf 源码路径、第三方 proto 等）
    common_includes = [
        "../../third_party/grpc/third_party/protobuf/src",
        "generated/proto",        # 自己的 proto 根目录
    ]

    # 多个 proto 源目录配置
    proto_jobs = [
        {
            "source_dir": "generated/proto/",
            "output_dir": "generated/proto/cpp/",
        },
        {
            "source_dir": "generated/proto/tip/",
            "output_dir": "generated/proto/cpp/tip/",
        },
    ]

    for job in proto_jobs:
        compile_protobuf_files(job["source_dir"], common_includes, job["output_dir"])
