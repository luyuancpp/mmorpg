#!/usr/bin/env python
# coding=utf-8
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
import multiprocessing

def compile_protobuf_file(protobuf_file, protobuf_include_dir, output_dir):
    """
    Compile a single .proto file to C++ using protoc compiler.

    Args:
    - protobuf_file (str): Full path to the .proto file.
    - protobuf_include_dir (str): Directory containing protobuf headers (usually protobuf installation directory).
    - output_dir (str): Directory where generated C++ files will be placed.
    """
    filename = os.path.basename(protobuf_file)
    command = f'protoc -I=./ -I={protobuf_include_dir} --cpp_out={output_dir} {protobuf_file}'
    try:
        subprocess.run(command, shell=True, check=True)
        print(f"Compiled {filename} successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Failed to compile {filename}: {e}")

def compile_protobuf_files(source_dir, protobuf_include_dir, output_dir):
    """
    Compile all .proto files in the source directory to C++ using protoc compiler with multiple threads.

    Args:
    - source_dir (str): Directory containing .proto files.
    - protobuf_include_dir (str): Directory containing protobuf headers (usually protobuf installation directory).
    - output_dir (str): Directory where generated C++ files will be placed.
    """
    # 获取CPU核心数
    num_cores = multiprocessing.cpu_count()
    print(f"Detected {num_cores} CPU cores. Compiling .proto files with {num_cores} threads.")

    # 使用线程池处理任务
    with ThreadPoolExecutor(max_workers=num_cores) as executor:
        futures = []
        for (dirpath, _, filenames) in os.walk(source_dir):
            for filename in filenames:
                if filename.endswith('.proto'):
                    full_file_path = os.path.join(dirpath, filename)
                    future = executor.submit(compile_protobuf_file, full_file_path, protobuf_include_dir, output_dir)
                    futures.append(future)

        # 等待所有任务完成
        for future in as_completed(futures):
            try:
                future.result()  # 获取任务的返回结果，如果有的话
            except Exception as e:
                print(f"Exception occurred: {e}")

if __name__ == "__main__":
    source_dir = "."  # 源代码目录，包含.proto文件
    protobuf_include_dir = "../../../third_party/protobuf/src"  # Protobuf头文件目录
    output_dir = "../protocpp/"  # 生成的C++代码目录

    compile_protobuf_files(source_dir, protobuf_include_dir, output_dir)
