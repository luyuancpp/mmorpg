#!/usr/bin/env python
# coding=utf-8

import os
import xlrd
import md5tool

# 全局变量
END_ROW_INDEX = 3
PROTO_DIR = "proto/"
XLSX_DIR = "xlsx/"


def get_column_names(sheet):
    """获取Excel表格的列名"""
    return sheet.row_values(0, 0, sheet.row_len(0))


def get_row_data(row, column_names):
    """将Excel表格的一行数据转换为字典形式"""
    return {column_names[i]: row[i].value for i in range(len(row))}


def get_sheet_data(sheet, column_names):
    """获取整个Excel表格的数据"""
    sheet_data = []
    for idx in range(1, sheet.nrows):
        if idx <= END_ROW_INDEX:
            row = sheet.row(idx)
            row_data = get_row_data(row, column_names)
            sheet_data.append(row_data)
    return sheet_data


def get_workbook_data(workbook):
    """获取整个工作簿（Workbook）的数据"""
    workbook_data = {}
    for sheet_name in workbook.sheet_names():
        sheet = workbook.sheet_by_name(sheet_name)
        column_names = get_column_names(sheet)
        sheet_data = get_sheet_data(sheet, column_names)
        workbook_data[sheet_name] = sheet_data
    return workbook_data


def generate_proto_file(data, sheet_name):
    """根据数据生成.proto文件内容"""
    proto_content = f'syntax = "proto3";\n\n'
    proto_content += f'message {sheet_name}_row' + ' {\n'
    for index, key in enumerate(data[0], start=1):
        if data[2][key].strip() in ('client', 'design'):
            continue
        if data[1][key].strip() == '':
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
        # 只处理xlsx和xls文件
        if file_path.endswith('.xlsx') or file_path.endswith('.xls'):
            md5_file_path = file_path + '.md5'
            # 检查MD5值，确保文件没有改变
            if not os.path.exists(md5_file_path):
                error = md5tool.check_against_md5_file(file_path, md5_file_path)
                if error is None:
                    continue

            # 打开Excel文件并获取数据
            workbook = xlrd.open_workbook(file_path)
            workbook_data = get_workbook_data(workbook)

            # 生成.proto文件并写入对应的目录
            for sheet_name, data in workbook_data.items():
                sheet_name_lower = sheet_name.lower()
                proto_content = generate_proto_file(data, sheet_name_lower)
                proto_file_path = os.path.join(PROTO_DIR, f'{sheet_name_lower}_config.proto')
                with open(proto_file_path, 'w', encoding='utf-8') as proto_file:
                    proto_file.write(proto_content)


if __name__ == "__main__":
    main()
