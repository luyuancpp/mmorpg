#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
import multiprocessing
import logging

# Configure logging
logging.basicConfig(level=logging.WARNING, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def compile_protobuf_file(protobuf_file, protobuf_include_dir, output_dir):
    """
    Compile a single .proto file to C++ using protoc compiler.

    Args:
    - protobuf_file (str): Full path to the .proto file.
    - protobuf_include_dir (str): Directory containing protobuf headers (usually protobuf installation directory).
    - output_dir (str): Directory where generated C++ files will be placed.
    """
    filename = os.path.basename(protobuf_file)
    command = f'protoc --proto_path={os.path.dirname(protobuf_file)} --proto_path={protobuf_include_dir} --cpp_out={output_dir} {protobuf_file}'

    try:
        # Capture the output and error messages from subprocess
        result = subprocess.run(command, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        logger.info(f"Compiled {filename} successfully. Output: {result.stdout}")
    except subprocess.CalledProcessError as e:
        logger.error(f"Failed to compile {filename}: {e}. Error: {e.stderr}")
    except Exception as e:
        logger.error(f"Unexpected error while compiling {filename}: {e}")

def compile_protobuf_files(source_dir, protobuf_include_dir, output_dir):
    """
    Compile all .proto files in the source directory to C++ using protoc compiler with multiple threads.

    Args:
    - source_dir (str): Directory containing .proto files.
    - protobuf_include_dir (str): Directory containing protobuf headers (usually protobuf installation directory).
    - output_dir (str): Directory where generated C++ files will be placed.
    """
    # Get the number of CPU cores
    num_cores = multiprocessing.cpu_count()
    logger.info(f"Detected {num_cores} CPU cores. Compiling .proto files with {num_cores} threads.")

    # Use a thread pool to handle tasks
    with ThreadPoolExecutor(max_workers=num_cores) as executor:
        futures = []
        for (dirpath, _, filenames) in os.walk(source_dir):
            if not filenames:
                logger.warning(f"No files found in directory {dirpath}")
                continue

            for filename in filenames:
                if filename.endswith('.proto'):
                    full_file_path = os.path.join(dirpath, filename)
                    # Submit each task with its own data
                    future = executor.submit(compile_protobuf_file, full_file_path, protobuf_include_dir, output_dir)
                    futures.append(future)

        # Wait for all tasks to complete
        for future in as_completed(futures):
            try:
                future.result()  # Get the task's return result, if any
            except Exception as e:
                logger.error(f"Exception occurred: {e}")

if __name__ == "__main__":
    # Adjust paths relative to the script's current directory
    source_dir = 'generated/proto'  # Source directory containing .proto files
    protobuf_include_dir = '../../third_party/grpc/third_party/protobuf/src'  # Protobuf headers directory
    output_dir = './generated/proto/cpp'  # Output directory for generated C++ files

    compile_protobuf_files(source_dir, protobuf_include_dir, output_dir)
