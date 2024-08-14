#!/usr/bin/env python
# coding=utf-8

import os
import openpyxl
import md5tool
import logging

# 全局变量
END_ROW_INDEX = 4
PROTO_DIR = "generated/proto/"
XLSX_DIR = "xlsx/"

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


def get_column_names(sheet):
    """获取Excel表格的列名"""
    column_names = [cell.value for cell in sheet[1]]  # 获取第一行的列名
    return column_names


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
        if len(data) < 3 or key not in data[2]:
            logger.warning(f"Column '{key}' is missing from the data.")
            continue

        if data[2][key].strip() in ('client', 'design'):
            continue

        if key not in data[1]:
            proto_content += f'\t{data[0][key]} {key} = {index};\n'
        else:
            proto_content += f'\t{data[1][key]} {data[0][key]} {key} = {index};\n'

    proto_content += '}\n\n'
    proto_content += f'message {sheet_name}_table' + ' {\n'
    proto_content += f'\trepeated {sheet_name}_row data = 1;\n'
    proto_content += '}\n'
    return proto_content


def main():
    # 确保生成.proto文件的目录存在
    if not os.path.exists(PROTO_DIR):
        os.makedirs(PROTO_DIR)

    # 遍历指定目录下的所有Excel文件
    for filename in os.listdir(XLSX_DIR):
        file_path = os.path.join(XLSX_DIR, filename)

        # 只处理xlsx文件
        if file_path.endswith('.xlsx'):
            md5_file_path = file_path + '.md5'

            # 检查MD5值，确保文件没有改变
            if not os.path.exists(md5_file_path):
                md5tool.generate_md5_file_for(file_path, md5_file_path)
                error = md5tool.check_against_md5_file(file_path, md5_file_path)
                if error is None:
                    continue

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


if __name__ == "__main__":
    main()
