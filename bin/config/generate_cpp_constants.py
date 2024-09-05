#!/usr/bin/env python
# coding=utf-8
import multiprocessing
import os
import json
import logging
import concurrent.futures
from os import listdir
from os.path import isfile, join
import openpyxl
import gen_common  # Assuming gen_common contains the necessary functions
from common import constants

# Setup Logging
logging.basicConfig(level=logging.WARNING, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def generate_cpp_constants(excel_file):
    # 载入工作簿和工作表
    workbook = openpyxl.load_workbook(excel_file)

    # 结果字符串
    cpp_constants = "#pragma once\n\n"

    # 遍历所有工作表
    sheet = workbook.sheetnames[0]
    worksheet = workbook[sheet]

    # 获取列标题
    headers = [cell.value for cell in worksheet[1]]

    # 确定 `constants_name` 列的索引
    constants_name_index = headers.index('constants_name') if 'constants_name' in headers else None

    # 处理每一行数据
    for row in worksheet.iter_rows(min_row=20, values_only=True):
        id_value = row[0]
        if id_value is None:
            continue  # Skip rows with no ID

        # 生成常量名称
        if constants_name_index is not None and row[constants_name_index]:
            constant_name = f'k{sheet}_{row[constants_name_index]}'
        else:
            constant_name = f"k{sheet}_{id_value}"

        # 生成 C++ 常量代码
        cpp_constant = f"constexpr uint32_t {constant_name} = {id_value};\n"
        cpp_constants += cpp_constant

    return cpp_constants

def save_cpp_constants_to_file(cpp_constants, output_file):
    with open(output_file, 'w') as file:
        file.write(cpp_constants)

def process_file(excel_file) -> None:
    cpp_constants = generate_cpp_constants(excel_file)
    workbook = openpyxl.load_workbook(excel_file)
    sheet_name = workbook.sheetnames[0]
    save_cpp_constants_to_file(cpp_constants, constants.GENERATOR_CONSTANTS_NAME_DIR + sheet_name.lower() + '_table_id_constants.h')

def get_xlsx_files(directory: str):
    """List all .xlsx files in the specified directory."""
    return [join(directory, filename) for filename in listdir(directory) if isfile(join(directory, filename)) and filename.endswith('.xlsx')]
def main() -> None:
    os.makedirs(constants.GENERATOR_CONSTANTS_NAME_DIR, exist_ok=True)
    try:
        xlsx_files = get_xlsx_files(constants.XLSX_DIR)
    except Exception as e:
        logger.error(f"Failed to list .xlsx files: {e}")
        return

    num_threads = min(multiprocessing.cpu_count(), len(xlsx_files))

    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:

        futures = [executor.submit(process_file, file_path) for file_path in xlsx_files]
        for future in concurrent.futures.as_completed(futures):
            try:
                future.result()
            except Exception as e:
                logger.error(f"An error occurred during processing: {e}")

if __name__ == "__main__":
    main()


