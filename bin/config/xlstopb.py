#!/usr/bin/env python
# coding=utf-8

import os
import openpyxl
import md5tool
import logging
import concurrent.futures
import multiprocessing

# 全局变量
END_ROW_INDEX = 4
PROTO_DIR = "generated/proto/"
XLSX_DIR = "xlsx/"

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

sheet_array_data_index = 3


def get_column_names(sheet):
    """获取Excel表格的列名"""
    column_names = [cell.value for cell in sheet[1]]  # 获取第一行的列名
    return column_names


def get_group_column_names(column_names):
    # 返回普通数组列
    array_column_names = {}
    group_column_names = {}

    column_len = len(column_names)

    same_begin_index = -1

    column_names_dict = {}

    for i in range(column_len):
        col_name = column_names[i]
        next_index = i + 1
        prev_index = i - 1

        if prev_index >= 0:
            if col_name != column_names[prev_index] and same_begin_index > 0:
                array_column_names[column_names[prev_index]] = list(range(same_begin_index, prev_index + 1))
                same_begin_index = -1

        #处理普通连续
        if next_index < column_len:
            if column_names[i] == column_names[next_index] and same_begin_index < 0:
                same_begin_index = i

        #有相同组
        if col_name in column_names_dict and prev_index >= 0 and col_name != column_names[prev_index]:
            in_group = False

            for key, value in group_column_names.items():
                group = value[0]
                for k in range(0, len(group)):
                    if column_names[group[k]] == col_name:
                        in_group = True
                        break
                if in_group:
                    break

            if not in_group:
                group_begin_index = column_names_dict[col_name]
                group_index_list = list(range(group_begin_index, prev_index + 1))
                if col_name in group_column_names:
                    group_column_names[col_name].append(group_index_list)
                else:
                    group_column_names[col_name] = [group_index_list]

        column_names_dict[col_name] = i

    return array_column_names, group_column_names


def get_row_data(row, column_names):
    """将Excel表格的一行数据转换为字典形式，并验证数据"""
    row_data = {}
    for i, cell in enumerate(row):
        col_name = column_names[i]
        if col_name and cell is not None:
            if isinstance(cell, float) and cell.is_integer():
                cell_value = int(cell)
            else:
                cell_value = cell

            if cell_value in (None, ''):
                logger.warning(f"Row {row[0].row}, column '{col_name}' is empty or invalid.")

            row_data[col_name] = cell_value
    return row_data


def get_sheet_data(sheet, column_names):
    """获取整个Excel表格的数据"""
    sheet_data = []
    for idx, row in enumerate(sheet.iter_rows(min_row=2, values_only=True), start=2):
        if idx <= END_ROW_INDEX:
            row_data = get_row_data(row, column_names)
            sheet_data.append(row_data)

    array_data, group_data = get_group_column_names(column_names)
    sheet_data.append(array_data)
    sheet_data.append(group_data)
    return sheet_data


def get_workbook_data(workbook):
    """获取整个工作簿（Workbook）的数据"""
    workbook_data = {}
    for sheet_name in workbook.sheetnames:
        sheet = workbook[sheet_name]
        if sheet.cell(row=1, column=1).value != "id":
            logger.error(f"{sheet_name} first column must be 'id'")
            continue

        column_names = get_column_names(sheet)
        sheet_data = get_sheet_data(sheet, column_names)
        workbook_data[sheet_name] = sheet_data
    return workbook_data


def generate_proto_file(data, sheet_name):
    """根据数据生成.proto文件内容"""
    proto_content = f'syntax = "proto3";\n\n'
    proto_content += f"option go_package = \"pb/game\";\n\n"
    proto_content += f'message {sheet_name}_row' + ' {\n'
    for index, key in enumerate(data[0], start=1):

        if data[1][key].strip() in ('client', 'design'):
            continue

        if key not in data[1]:
            proto_content += f'\t{data[0][key]} {key} = {index};\n'
        elif key in data[sheet_array_data_index]:
            proto_content += f'\trepeated {data[0][key]} {key} = {index};\n'

    proto_content += '}\n\n'
    proto_content += f'message {sheet_name}_table' + ' {\n'
    proto_content += f'\trepeated {sheet_name}_row data = 1;\n'
    proto_content += '}\n'
    return proto_content


def process_file(file_path):
    """处理单个Excel文件并生成.proto文件"""
    if file_path.endswith('.xlsx'):
        md5_file_path = file_path + '.md5'

        # 检查MD5值，确保文件没有改变
        if not os.path.exists(md5_file_path):
            md5tool.generate_md5_file_for(file_path, md5_file_path)
            error = md5tool.check_against_md5_file(file_path, md5_file_path)
            if error is None:
                return

        # 打开Excel文件并获取数据
        workbook = openpyxl.load_workbook(file_path)
        workbook_data = get_workbook_data(workbook)

        # 生成.proto文件并写入对应的目录
        for sheet_name, data in workbook_data.items():
            sheet_name_lower = sheet_name.lower()
            proto_content = generate_proto_file(data, sheet_name_lower)
            proto_file_path = os.path.join(PROTO_DIR, f'{sheet_name_lower}_config.proto')
            with open(proto_file_path, 'w', encoding='utf-8') as proto_file:
                proto_file.write(proto_content)
                logger.info(f"Generated .proto file: {proto_file_path}")


def main():
    # 确保生成.proto文件的目录存在
    if not os.path.exists(PROTO_DIR):
        os.makedirs(PROTO_DIR)

    # 获取所有xlsx文件
    xlsx_files = [os.path.join(XLSX_DIR, filename) for filename in os.listdir(XLSX_DIR) if filename.endswith('.xlsx')]

    # 使用线程池并发处理文件
    num_threads = min(multiprocessing.cpu_count(), len(xlsx_files))  # 根据CPU核心数和文件数量选择线程数
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        futures = [executor.submit(process_file, file_path) for file_path in xlsx_files]
        for future in concurrent.futures.as_completed(futures):
            try:
                future.result()
            except Exception as e:
                logger.error(f"An error occurred: {e}")


if __name__ == "__main__":
    main()
