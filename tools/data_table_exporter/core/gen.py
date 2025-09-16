#!/usr/bin/env python
# coding=utf-8
import os
import subprocess
import logging
from pathlib import Path
import paths
from md5copy_utils import md5_copy

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# 基础目录引用
GEN_CODE_DIR = paths.PROJECT_GENERATED_CODE_DIR  # 生成代码的根目录
GEN_JSON_DIR = paths.PROJECT_GENERATED_JSON_DIR  # 生成JSON的根目录
PROJECT_DIR = paths.PROJECT_DIR  # 项目根目录


# 首先定义所有复制任务
def get_copy_tasks():
    """获取所有需要执行的复制任务"""
    copy_tasks = [
        (paths.SRC_CPP, paths.DST_CPP_CODE),
        (paths.SRC_CPP_ID_BIT_INDEX, paths.DST_CPP_ID_BIT_INDEX),
        (paths.SRC_CPP_CONSTANTS, paths.DST_CPP_CONSTANTS),
        (paths.SRC_PROTO_CPP, paths.DST_PROTO_CPP),
        (paths.SRC_PROTO_CPP_OPERATOR, paths.DST_PROTO_CPP_OPERATOR),
        (paths.SRC_PROTO_CPP_TIP, paths.DST_PROTO_CPP_TIP),
        (paths.SRC_CPP_TABLE_ID, paths.DST_CPP_TABLE_ID),
    ]

    # 处理GRPC服务目录
    try:
        grpc_service_dir = Path(paths.GRPC_SERVICE_DIR)
        dst_go_grpc = Path(paths.DST_GO_GRPC)

        if grpc_service_dir.exists() and grpc_service_dir.is_dir():
            # 获取第一层子目录
            for item in grpc_service_dir.iterdir():
                if item.is_dir():
                    src_dir = Path(paths.SRC_GO)
                    dst_dir = dst_go_grpc / item.name
                    copy_tasks.append((src_dir, dst_dir))
                    logger.info(f"添加复制任务: {src_dir} -> {dst_dir}")
        else:
            logger.warning(f"GRPC服务目录不存在或不是目录: {grpc_service_dir}")
    except Exception as e:
        logger.error(f"设置GRPC复制任务时出错: {e}")

    return copy_tasks


# 获取复制任务
copy_tasks = get_copy_tasks()


# 定义所有需要创建的基础目录（不包含复制任务中已有的目录）
def get_base_directories():
    """
    定义基础目录列表，这些目录不包含在复制任务的源目录和目标目录中
    避免与复制任务中的目录重复定义
    """
    return [
        # 生成代码相关目录
        GEN_CODE_DIR / "cpp",
        GEN_CODE_DIR / "proto",
        GEN_CODE_DIR / "proto" / "cpp",
        GEN_CODE_DIR / "proto" / "go",
        paths.DST_PROTO_GO,

        # JSON相关目录
        GEN_JSON_DIR / "json",

        # Go语言相关源目录
        paths.SRC_GO_ID_BIT,
        paths.SRC_GO_CONSTANTS,
        paths.SRC_GO_TABLE_ID,
    ]


def get_all_directories():
    """
    整合所有需要创建的目录：
    1. 基础目录（已去除与复制任务重复的目录）
    2. 复制任务中的所有源目录和目标目录
    """
    # 获取基础目录
    directories = get_base_directories()

    # 添加复制任务中的所有源目录和目标目录
    for src, dst in copy_tasks:
        src_path = Path(src)
        dst_path = Path(dst)

        # 如果是文件，添加其父目录；如果是目录，直接添加
        if src_path.suffix:  # 有扩展名，视为文件
            directories.append(src_path.parent)
        else:
            directories.append(src_path)

        if dst_path.suffix:  # 有扩展名，视为文件
            directories.append(dst_path.parent)
        else:
            directories.append(dst_path)

    # 去重并返回
    return list(set(directories))


def create_directories(dirs):
    """创建目录列表中的所有目录，如果它们不存在"""
    created = 0
    for directory in dirs:
        dir_path = Path(directory)
        if not dir_path.exists():
            try:
                dir_path.mkdir(parents=True, exist_ok=True)
                logger.info(f"创建目录: {dir_path}")
                created += 1
            except Exception as e:
                logger.error(f"创建目录 {dir_path} 失败: {e}")
    return created


def run_commands(cmds, timeout=300):
    """执行命令列表中的所有命令，并返回执行结果统计"""
    success = 0
    failed = 0

    for cmd in cmds:
        try:
            logger.info(f"执行命令: {cmd}")
            result = subprocess.run(
                cmd,
                shell=True,
                check=True,
                capture_output=True,
                text=True,
                timeout=timeout
            )

            if result.stdout:
                logger.info(f"命令输出: {result.stdout.strip()}")
            if result.stderr:
                if "ERROR" in result.stderr:
                    logger.error(f"命令错误输出: {result.stderr.strip()}")
                    failed += 1
                else:
                    logger.info(f"命令标准错误: {result.stderr.strip()}")

            success += 1

        except subprocess.CalledProcessError as e:
            logger.error(f"命令 '{cmd}' 执行失败 (返回码: {e.returncode}): {e.stderr}")
            failed += 1
        except subprocess.TimeoutExpired:
            logger.error(f"命令 '{cmd}' 执行超时")
            failed += 1
        except Exception as e:
            logger.error(f"命令 '{cmd}' 执行时发生错误: {str(e)}")
            failed += 1

    return success, failed


def execute_copy_tasks(tasks):
    """执行所有复制任务"""
    copied = 0
    failed = 0

    for src, dst in tasks:
        try:
            logger.info(f"正在从 {src} 复制到 {dst} (带MD5校验)...")
            md5_copy(str(src), str(dst))
            copied += 1
        except Exception as e:
            logger.error(f"复制 {src} 到 {dst} 失败: {e}")
            failed += 1

    return copied, failed


def main():
    logger.info("===== 开始项目构建流程 =====")

    # 获取所有需要创建的目录
    all_directories = get_all_directories()

    # 创建目录
    logger.info("===== 开始创建目录 =====")
    created_dirs = create_directories(all_directories)
    logger.info(f"目录创建完成，共创建 {created_dirs} 个新目录")

    # 定义需要执行的命令
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

    # 执行命令
    logger.info("===== 开始执行生成命令 =====")
    cmd_success, cmd_failed = run_commands(commands)
    logger.info(f"命令执行完成，成功: {cmd_success}, 失败: {cmd_failed}")

    # 执行复制任务
    logger.info("===== 开始执行文件复制 =====")
    copy_success, copy_failed = execute_copy_tasks(copy_tasks)
    logger.info(f"文件复制完成，成功: {copy_success}, 失败: {copy_failed}")

    # 总结
    logger.info("===== 项目构建流程结束 =====")
    logger.info(f"总览: 目录创建 {created_dirs} 个, 命令成功 {cmd_success} 个, 复制成功 {copy_success} 个")

    if cmd_failed > 0 or copy_failed > 0:
        logger.warning(f"注意: 有 {cmd_failed} 个命令和 {copy_failed} 个复制任务失败")
    else:
        logger.info("所有操作均成功完成!")


if __name__ == "__main__":
    main()
