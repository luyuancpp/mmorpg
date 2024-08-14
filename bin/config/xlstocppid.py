#!/usr/bin/env python
# coding=utf-8

import os
import openpyxl
import gencommon  # Assuming gencommon provides beginrowidx and mywrite functions
import logging
from os import listdir
from os.path import isfile, join

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

begin_row_idx = gencommon.beginrowidx
cpp_dir = "generated/cpp/"
xlsx_dir = "xlsx/"
gen_file_list = ["global_variable"]


def generate_id_enum(sheet):
    n_rows = sheet.max_row
    name = sheet.title
    file_str = "#pragma once\n"
    file_str += f"enum e_{name}_configid : uint32_t\n{{\n"

    for idx in range(begin_row_idx, n_rows + 1):
        cell_value = sheet.cell(row=idx, column=1).value
        if cell_value is not None:
            file_str += f"    {name}_config_id_{int(cell_value)},\n"

    file_str += "};\n"
    return file_str


def generate_id_cpp(workbook):
    workbook_data = {}
    for sheet_name in workbook.sheetnames:
        if sheet_name not in gen_file_list:
            continue
        sheet = workbook[sheet_name]
        file_data = generate_id_enum(sheet)
        workbook_data[sheet_name] = file_data
    return workbook_data


def main():
    if not os.path.exists(cpp_dir):
        os.makedirs(cpp_dir)

    for filename in listdir(xlsx_dir):
        full_path = os.path.join(xlsx_dir, filename)
        if isfile(full_path) and filename.endswith('.xlsx'):
            try:
                workbook = openpyxl.load_workbook(full_path)
                workbook_data = generate_id_cpp(workbook)
                for sheet_name, data in workbook_data.items():
                    output_file_path = os.path.join(cpp_dir, f"{sheet_name}_config_id.h")
                    gencommon.mywrite(data, output_file_path)
                    logger.info(f"Generated C++ enum file: {output_file_path}")
            except Exception as e:
                logger.error(f"Failed to process file {full_path}: {e}")


if __name__ == "__main__":
    main()
