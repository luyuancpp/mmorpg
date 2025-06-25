#!/usr/bin/env python
# coding=utf-8
import logging
import os
import shutil

from pathlib import Path

BEGIN_ROW_IDX = 19
FIELD_INFO_END_ROW_INDEX = 18
md5_output_dir = "generated/xlsx/md5/"

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

MAP_KEY_CELL = 'map_key'
MAP_VALUE_CELL = 'map_value'
SET_CELL = 'set'
TABLE_KEY_CELL = 'table_key'
MULTI_TABLE_KEY_CELL = 'multi'

FILE_TYPE_INDEX = 0
MAP_TYPE_INDEX = 1
OWNER_INDEX = 2
MULTI_KEY_INDEX = 3

XLSX_TABLE_BIT_BEGIN_INDEX = 6

COL_OBJ_COLUMN_NAME = 0
COL_OBJ_COLUMN_TYPE = 1
COL_OBJ_TABLE_MULTI = 4
COL_OBJ_TABLE_KEY_INDEX = 5
COL_OBJ_TABLE_EXPRESSION_TYPE_INDEX = 6
COL_OBJ_TABLE_EXPRESSION_PARAM_NAMES_INDEX = 7


SHEET_ARRAY_DATA_INDEX = FIELD_INFO_END_ROW_INDEX - 1
SHEET_GROUP_ARRAY_DATA_INDEX = FIELD_INFO_END_ROW_INDEX
SHEET_COLUM_NAME_INDEX = FIELD_INFO_END_ROW_INDEX + 1

TEMPLATE_DIR = Path('templates')
def mywrite(content, filename):
    """将字符串写入指定文件"""
    with open(filename, "w", encoding="utf-8") as output_file:
        output_file.write(content)

def find_common_words(text1, text2, separator):
    """查找两个文本中的共同单词"""
    text1 = text1.lower()
    text2 = text2.lower()
    words1 = set(text1.split(separator))
    words2 = set(text2.split(separator))
    return words1.intersection(words2)

def set_to_string(s):
    """将集合中的元素拼接成一个字符串"""
    return ''.join(map(str, s))

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
    """检查一个键是否在分组数组中"""
    return any(column_names[cell_index] == key for v in data.values() for cell_index in v)

def is_key_in_map(data, key, map_field_data, column_names):
    """检查一个键是否在映射中"""
    for v in data.values():
        if key in (column_names[cell_index] for cell_index in v):
            for cell_index in v:
                if column_names[cell_index] in map_field_data:
                    column_name = column_names[cell_index]
                    return map_field_data[column_name] in (MAP_KEY_CELL, MAP_VALUE_CELL)
    return False

def column_name_to_obj_name(column_name, separator):
    """根据分隔符从列名中提取对象名"""
    return column_name.split(separator)[0]

def get_row_data(row, column_names):
    """将Excel表格的一行数据转换为字典形式，并验证数据"""
    row_data = {}
    for i, cell in enumerate(row):
        col_name = column_names[i]
        if col_name and cell is not None:
            cell_value = int(cell) if isinstance(cell, float) and cell.is_integer() else cell
            if cell_value in (None, ''):
                # Check if row[0] is not None before accessing its attributes
                row_number = row[0].row if row and row[0] else 'unknown'
                logger.warning(f"Row {row_number}, column '{col_name}' is empty or invalid.")
            row_data[col_name] = cell_value
    return row_data


def fill_map(group_data, map_field_data, column_names):
    """填充映射数据"""
    for k, v in group_data.items():
        if column_names[v[0]] in map_field_data and map_field_data[column_names[v[0]]] == MAP_KEY_CELL:
            map_field_data[column_names[v[0]]] = MAP_KEY_CELL
            if len(v) > 1:
                map_field_data[column_names[v[1]]] = MAP_VALUE_CELL

def get_sheet_data(sheet, column_names):
    """获取整个Excel表格的数据"""
    sheet_data = [get_row_data(row, column_names) for idx, row in enumerate(sheet.iter_rows(min_row=2, values_only=True), start=2) if idx <= FIELD_INFO_END_ROW_INDEX]
    try:
        array_data, group_data = get_group_column_names(column_names)
    except Exception as e:
        logger.error(f"Failed to get group column names: {e}")
        array_data, group_data = {}, {}

    fill_map(group_data, sheet_data[MAP_TYPE_INDEX] if sheet_data else {}, column_names)
    sheet_data.append(array_data)
    sheet_data.append(group_data)
    sheet_data.append(column_names)
    return sheet_data

def get_column_names(sheet):
    """获取Excel表格的列名"""
    try:
        return [cell.value for cell in sheet[1]]  # 获取第一行的列名
    except Exception as e:
        logger.error(f"Failed to get column names: {e}")
        return []


def get_first_19_rows_per_column(sheet):
    """获取Excel每列的前19行数据并将其转换为字典对象"""
    # 初始化存储每列前19行数据的字典
    columns_data = []

    # 获取列名（假设第一行为列名）
    column_names = [cell.value for cell in sheet[1]]

    # 遍历每列
    for col_idx, col_name in enumerate(column_names):
        if col_name:  # 确保列名存在
            column_data = []
            # 获取该列的前19行数据
            for row_idx in range(1, FIELD_INFO_END_ROW_INDEX):
                cell_value = sheet.cell(row=row_idx, column=col_idx + 1).value
                column_data.append(cell_value)
            columns_data.append(column_data)

    return columns_data


def check_cell_value(columns_data, column_name, row_number, target_value):
    cell_value = columns_data[column_name][row_number]

    return cell_value == target_value


def count_words_by_comma(text):
    # 使用逗号作为分隔符，split 方法返回一个列表
    words = text.split(',')

    # 去除可能的前后空格，并排除空字符串
    words = [word.strip() for word in words if word.strip()]

    # 返回单词数量和单词列表
    return len(words), words

def lower_first_letter(s):
    if not s:  # 检查字符串是否为空
        return s
    return s[0].lower() + s[1:]  # 将首字母小写并拼接其余部分

def copy_all(src_dir, dst_dir):
    if not os.path.exists(src_dir):
        return

    for root, dirs, files in os.walk(src_dir):
        rel_path = os.path.relpath(root, src_dir)
        target_dir = os.path.join(dst_dir, rel_path)
        os.makedirs(target_dir, exist_ok=True)
        for file in files:
            src_file = os.path.join(root, file)
            dst_file = os.path.join(target_dir, file)
            shutil.copy2(src_file, dst_file)
