#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging

# 配置日志
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)


def compile_protobuf_files_go(source_dir, include_dirs, output_dir):
    """
    编译 source_dir 下所有 .proto 文件到 Go 代码。

    Args:
    - source_dir (str): .proto 文件所在目录
    - include_dirs (list[str]): 依赖的 proto 头文件目录
    - output_dir (str): 生成的 Go 文件输出目录
    """
    proto_files = []
    for dirpath, _, filenames in os.walk(source_dir):
        for filename in filenames:
            if filename.endswith(".proto"):
                # 相对路径并统一用 '/' 分隔符
                rel_path = os.path.relpath(os.path.join(dirpath, filename), start=source_dir)
                rel_path = rel_path.replace(os.path.sep, '/')
                proto_files.append(rel_path)

    if not proto_files:
        logger.warning(f"No .proto files found in {source_dir}")
        return

    os.makedirs(output_dir, exist_ok=True)

    # 构造 protoc 命令
    command = ["protoc", f"-I={os.path.abspath(source_dir)}"]
    for inc in include_dirs:
        command.append(f"-I={os.path.abspath(inc)}")

    command.append(f"--go_out={os.path.abspath(output_dir)}")
    command.append(f"--go-grpc_out={os.path.abspath(output_dir)}")
    command.extend(proto_files)

    try:
        result = subprocess.run(
            command,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=source_dir  # 在 proto 根目录下运行
        )
        logger.info(f"Compiled all proto files in {source_dir} to Go successfully.")
        if result.stdout:
            logger.info(result.stdout)
        if result.stderr:
            logger.warning(result.stderr)
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to compile proto files in {source_dir}. Error: {e.stderr}")


def check_protoc_exists():
    """检查系统是否安装 protoc"""
    import shutil
    if not shutil.which("protoc"):
        logger.error("protoc 编译器未找到，请先安装并确保在 PATH 中")
        exit(1)


if __name__ == "__main__":
    check_protoc_exists()

    # 公共 proto include 目录
    common_includes = [
        "generated/proto",  # 你的 proto 根目录
        "../../third_party/grpc/third_party/protobuf/src",
    ]

    # 多个任务配置（不同 source_dir -> 不同输出目录）
    proto_jobs = [
        {
            "source_dir": "generated/proto",
            "output_dir": "generated/proto/go",
        },
        {
            "source_dir": "generated/proto/tip",
            "output_dir": "generated/proto/go/tip",
        },
        {
            "source_dir": "generated/proto/operator",
            "output_dir": "generated/proto/go/operator",
        },
    ]

    for job in proto_jobs:
        compile_protobuf_files_go(job["source_dir"], common_includes, job["output_dir"])
