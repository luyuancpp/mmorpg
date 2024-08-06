#!/usr/bin/env python
# coding=utf-8

import os
import xlrd
import gencommon  # Assuming gencommon provides beginrowidx and mywrite functions
from os import listdir
from os.path import isfile, join

begin_row_idx = gencommon.beginrowidx
cpp_dir = "cpp/"
xls_dir = "xlsx/"
gen_file_list = ["global_variable"]

def generate_id_enum(sheet):
    n_rows = sheet.nrows
    name = sheet.name
    file_str = "#pragma once\n"
    file_str += f"enum e_{name}_configid : uint32_t\n{{\n"
    for idx in range(1, n_rows):
        if idx >= begin_row_idx:
            row_data = sheet.cell_value(idx, 0)
            file_str += f"    {name}_config_id_{int(row_data)},\n"
    file_str += "};\n"
    return file_str

def generate_id_cpp(workbook):
    workbook_data = {}
    for sheet_name in workbook.sheet_names():
        if sheet_name not in gen_file_list:
            continue
        worksheet = workbook.sheet_by_name(sheet_name)
        file_data = generate_id_enum(worksheet)
        workbook_data[sheet_name] = file_data
    return workbook_data

def main():
    if not os.path.exists(cpp_dir):
        os.makedirs(cpp_dir)

    for filename in listdir(xls_dir):
        full_path = os.path.join(xls_dir, filename)
        if isfile(full_path) and (filename.endswith('.xlsx') or filename.endswith('.xls')):
            workbook = xlrd.open_workbook(full_path)
            workbook_data = generate_id_cpp(workbook)
            for sheet_name, data in workbook_data.items():
                output_file_path = os.path.join(cpp_dir, f"{sheet_name}_config_id.h")
                gencommon.mywrite(data, output_file_path)
                print(f"Generated C++ enum file: {output_file_path}")

if __name__ == "__main__":
    main()
