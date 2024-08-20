#!/usr/bin/env python
# coding=utf-8

import os
import openpyxl
import logging
import concurrent.futures
import multiprocessing
import gencommon

# 全局变量
PROTO_DIR = "generated/proto/"
XLSX_DIR = "xlsx/"

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

file_type_index = 0
map_type_index = 1
owner_index = 2
object_name_index = 3
sheet_array_data_index = 4
sheet_group_array_data_index = 5


def get_workbook_data(workbook):
    """获取整个工作簿（Workbook）的数据"""
    workbook_data = {}
    for sheet_name in workbook.sheetnames:
        try:
            sheet = workbook[sheet_name]
            if sheet.cell(row=1, column=1).value != "id":
                logger.error(f"{sheet_name} first column must be 'id'")
                continue

            column_names = gencommon.get_column_names(sheet)
            if not column_names:
                continue
            sheet_data = gencommon.get_sheet_data(sheet, column_names)
            workbook_data[sheet_name] = sheet_data
        except Exception as e:
            logger.error(f"Failed to process sheet {sheet_name}: {e}")
    return workbook_data


def generate_proto_file(data, sheet_name):
    """根据数据生成.proto文件内容"""
    try:
        proto_content = f'syntax = "proto3";\n\n'
        proto_content += f"option go_package = \"pb/game\";\n\n"

        names_type_dict = data[0]
        column_names = data[6]

        for k, v in data[sheet_group_array_data_index].items():
            obj_name = gencommon.set_to_string(gencommon.find_common_words(column_names[v[0]], column_names[v[1]], '_'))
            proto_content += f'message {obj_name}' + ' {\n'
            for i in range(len(v)):
                name = column_names[v[i]]
                proto_content += f'\t{names_type_dict[name]} {name} = {i + 1};\n'
            proto_content += '}\n\n'

        proto_content += f'message {sheet_name}_row' + ' {\n'

        field_index = 1
        for index, key in enumerate(names_type_dict, start=1):
            if data[owner_index].get(key, '').strip() in ('client', 'design'):
                continue

            if key in data[map_type_index].keys():
                filedata = data[map_type_index][key]
                if filedata == gencommon.set_flag:
                    proto_content += f'\tmap <{names_type_dict[key]}, bool> {key} = {field_index};\n'
                elif filedata == gencommon.map_flag:
                    if key not in data[sheet_group_array_data_index]:
                        continue
                    value_type = data[file_type_index]
                    value = data[sheet_group_array_data_index][key]
                    key_name = column_names[value[0]]
                    value_name = column_names[value[1]]
                    obj_name = gencommon.column_name_to_obj_name(key_name, '_')
                    proto_content += f'\tmap <{value_type[key_name]}, {value_type[value_name]}> {obj_name} = {field_index};\n'
            elif key in data[sheet_array_data_index]:
                proto_content += f'\trepeated {names_type_dict[key]} {key} = {field_index};\n'
            elif gencommon.is_key_in_group_array(data[sheet_group_array_data_index], key, column_names):
                if key not in data[sheet_group_array_data_index]:
                    continue
                value = data[sheet_group_array_data_index][key]
                key_name = column_names[value[0]]
                obj_name = gencommon.column_name_to_obj_name(key_name, '_')
                proto_content += f'\trepeated {obj_name} {obj_name} = {field_index};\n'
            else:
                proto_content += f'\t{names_type_dict[key]} {key} = {field_index};\n'

            field_index += 1

        proto_content += '}\n\n'
        proto_content += f'message {sheet_name}_table' + ' {\n'
        proto_content += f'\trepeated {sheet_name}_row data = 1;\n'
        proto_content += '}\n'
        return proto_content
    except Exception as e:
        logger.error(f"Failed to generate proto content: {e}")
        return None


def process_file(file_path):
    # 打开Excel文件并获取数据
    try:
        workbook = openpyxl.load_workbook(file_path)
        workbook_data = get_workbook_data(workbook)

        # 生成.proto文件并写入对应的目录
        for sheet_name, data in workbook_data.items():
            sheet_name_lower = sheet_name.lower()
            proto_content = generate_proto_file(data, sheet_name_lower)
            if proto_content:
                proto_file_path = os.path.join(PROTO_DIR, f'{sheet_name_lower}_config.proto')
                with open(proto_file_path, 'w', encoding='utf-8') as proto_file:
                    proto_file.write(proto_content)
                    logger.info(f"Generated .proto file: {proto_file_path}")
    except Exception as e:
        logger.error(f"Failed to process file {file_path}: {e}")


def main():
    # 确保生成.proto文件的目录存在
    try:
        if not os.path.exists(PROTO_DIR):
            os.makedirs(PROTO_DIR)
    except Exception as e:
        logger.error(f"Failed to create proto directory: {e}")
        return

    # 获取所有xlsx文件
    try:
        xlsx_files = [os.path.join(XLSX_DIR, filename) for filename in os.listdir(XLSX_DIR) if
                      filename.endswith('.xlsx')]
    except Exception as e:
        logger.error(f"Failed to list .xlsx files: {e}")
        return

    # 使用线程池并发处理文件
    num_threads = min(multiprocessing.cpu_count(), len(xlsx_files))  # 根据CPU核心数和文件数量选择线程数
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        futures = [executor.submit(process_file, file_path) for file_path in xlsx_files]
        for future in concurrent.futures.as_completed(futures):
            try:
                future.result()
            except Exception as e:
                logger.error(f"An error occurred during processing: {e}")


if __name__ == "__main__":
    main()
