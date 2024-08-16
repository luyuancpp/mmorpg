#!/usr/bin/env python
# coding=utf-8

beginrowidx = 7


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

        #处理普通连续
        if next_index < column_len:
            if column_names[i] == column_names[next_index] and same_begin_index < 0:
                same_begin_index = i

        #有相同组
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