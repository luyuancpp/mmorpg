#!/usr/bin/env python
# coding=utf-8
import logging

BEGIN_ROW_IDX = 19
FIELD_INFO_END_ROW_INDEX = 18
md5_output_dir = "generated/xlsx/md5/"

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

map_key_flag = 'map_key'
map_value_flag = 'map_value'
set_flag = 'set'

#第一行作为key 所以不用记录,后面的每一行用第一行作为key,自己的格子作为value
FILE_TYPE_INDEX = 0
MAP_TYPE_INDEX = 1
OWNER_INDEX = 2
MULTI_KEY_INDEX = 3

SHEET_ARRAY_DATA_INDEX = FIELD_INFO_END_ROW_INDEX - 1
SHEET_GROUP_ARRAY_DATA_INDEX = FIELD_INFO_END_ROW_INDEX
SHEET_COLUM_NAME_INDEX = FIELD_INFO_END_ROW_INDEX + 1

def mywrite(str, filename):
    outputh = open(filename, "w", encoding="utf-8")
    outputh.write(str)
    outputh.close()


def find_common_words(text1, text2, separator):
    # 将文本转换为小写以确保不区分大小写
    text1 = text1.lower()
    text2 = text2.lower()

    # 将文本拆分成单词列表
    words1 = set(text1.split(separator))
    words2 = set(text2.split(separator))

    # 找出两个集合的交集，即共同的单词
    common_words = words1.intersection(words2)

    return common_words


def set_to_string(s):
    """将集合中的元素拼接成一个字符串"""
    return ''.join(str(element) for element in s)


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
            if (col_name != column_names[prev_index] or next_index >= column_len) and same_begin_index > 0:
                array_column_names[column_names[prev_index]] = list(range(same_begin_index, prev_index + 1))
                same_begin_index = -1

        # 处理普通连续
        if next_index < column_len:
            if column_names[i] == column_names[next_index] and same_begin_index < 0:
                same_begin_index = i

        # 有相同组
        if col_name in column_names_dict and prev_index >= 0 and col_name != column_names[prev_index]:
            in_group = False

            for key, value in group_column_names.items():
                for k in value:
                    if column_names[k] == col_name:
                        in_group = True
                        break
                if in_group:
                    break

            if not in_group:
                group_begin_index = column_names_dict[col_name]
                group_index_list = list(range(group_begin_index, prev_index + 1))
                group_column_names[col_name] = group_index_list

        column_names_dict[col_name] = i

    return array_column_names, group_column_names


def is_key_in_group_array(data, key, column_names):
    for k, v in data.items():
        for cell_index in v:
            if column_names[cell_index] == key:
                return True
    return False


def is_key_in_map(data, key, map_field_data, column_names):
    for k, v in data.items():
        found = False
        for cell_index in v:
            if column_names[cell_index] == key:
                found = True
                break
        if found:
            for cell_index in v:
                if column_names[cell_index] in map_field_data:
                    column_name = map_field_data[cell_index]
                    return map_field_data[column_name] == map_key_flag or map_field_data[column_name] == map_value_flag
    return False


def column_name_to_obj_name(column_name, separator):
    return column_name.split(separator)[0]


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


def fill_map(group_data, map_field_data, column_names):
    for k, v in group_data.items():
        for cell_index in v:
            key_index = v[0]
            if column_names[key_index] in map_field_data and map_field_data[column_names[key_index]] == map_key_flag:
                map_field_data[column_names[key_index]] = map_key_flag
                map_field_data[column_names[v[1]]] = map_value_flag
                break


def get_sheet_data(sheet, column_names):
    """获取整个Excel表格的数据"""
    sheet_data = []
    for idx, row in enumerate(sheet.iter_rows(min_row=2, values_only=True), start=2):
        if idx <= FIELD_INFO_END_ROW_INDEX:
            row_data = get_row_data(row, column_names)
            sheet_data.append(row_data)

    try:
        array_data, group_data = get_group_column_names(column_names)
    except Exception as e:
        logger.error(f"Failed to get group column names: {e}")
        array_data, group_data = {}, {}

    fill_map(group_data, sheet_data[MAP_TYPE_INDEX], column_names)
    sheet_data.append(array_data)
    sheet_data.append(group_data)
    sheet_data.append(column_names)
    return sheet_data


def get_column_names(sheet):
    """获取Excel表格的列名"""
    try:
        column_names = [cell.value for cell in sheet[1]]  # 获取第一行的列名
        return column_names
    except Exception as e:
        logger.error(f"Failed to get column names: {e}")
        return []
