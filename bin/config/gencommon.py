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
