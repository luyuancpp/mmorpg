import os
import hashlib
import shutil
import logging

logger = logging.getLogger(__name__)


def generate_md5_hash(file_path, block_size=2**20):
    """计算指定文件的 MD5 值"""
    md5 = hashlib.md5()
    try:
        with open(file_path, 'rb') as f:
            while chunk := f.read(block_size):
                md5.update(chunk)
        return md5.hexdigest()
    except Exception as e:
        logger.error(f"Error reading file for MD5: {file_path} ({e})")
        return None


def md5_files_equal(file1, file2):
    """判断两个文件的 MD5 是否相同"""
    hash1 = generate_md5_hash(file1)
    hash2 = generate_md5_hash(file2)
    if hash1 is None or hash2 is None:
        return False
    return hash1 == hash2


def md5_copy_file(src_file, dst_file):
    """如果文件内容不同，复制文件"""
    if not os.path.exists(dst_file):
        shutil.copyfile(src_file, dst_file)
        logger.info(f"Copied new file: {src_file} -> {dst_file}")
    elif not md5_files_equal(src_file, dst_file):
        shutil.copyfile(src_file, dst_file)
        logger.info(f"Overwrote file due to MD5 mismatch: {src_file} -> {dst_file}")
    else:
        logger.info(f"Skipped identical file: {src_file}")


def md5_copy_dir(src_dir, dst_dir):
    """递归地复制目录下的所有文件，仅在文件内容变化时进行复制"""
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
        logger.info(f"Created directory: {dst_dir}")

    for item in os.listdir(src_dir):
        src_path = os.path.join(src_dir, item)
        dst_path = os.path.join(dst_dir, item)

        if os.path.isdir(src_path):
            md5_copy_dir(src_path, dst_path)
        elif os.path.isfile(src_path):
            md5_copy_file(src_path, dst_path)
        else:
            logger.warning(f"Skipped unknown file type: {src_path}")


def md5_copy(src, dst):
    """根据类型调用文件/目录复制"""
    if os.path.isdir(src):
        md5_copy_dir(src, dst)
    elif os.path.isfile(src):
        md5_copy_file(src, dst)
    else:
        logger.warning(f"Source path is neither file nor directory: {src}")
