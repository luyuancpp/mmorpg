#!/usr/bin/env python
# coding=utf-8

import os
import xlrd
import gencommon
import concurrent.futures
from os import listdir
from multiprocessing import cpu_count

# 全局变量
keyrowidx = 4
cppdir = "generated/cpp/"
xlsdir = "xlsx/"


def get_column_names(sheet):
    """获取Excel表格的列名"""
    return sheet.row_values(keyrowidx, 0, sheet.row_len(keyrowidx))


def get_key_row_data(row, column_names):
    """将Excel表格的关键行数据转换为字典形式"""
    row_data = {}
    for i in range(len(row)):
        if column_names[i].strip() == "key":
            row_data[str(row[i].value)] = row[i].value
    return row_data


def get_sheet_key_data(sheet, column_names):
    """获取表格的关键数据"""
    row = sheet.row(0)
    key_row_data = get_key_row_data(row, column_names)
    return [key_row_data]


def get_workbook_data(workbook):
    """获取整个工作簿（Workbook）的数据"""
    workbook_data = {}
    for sheet_name in workbook.sheet_names():
        sheet = workbook.sheet_by_name(sheet_name)
        column_names = get_column_names(sheet)
        sheet_key_data = get_sheet_key_data(sheet, column_names)
        workbook_data[sheet_name] = sheet_key_data
    return workbook_data


def generate_cpp_header(datastring, sheetname):
    """生成C++头文件内容"""
    sheet_name_lower = sheetname.lower()
    s = "#pragma once\n"
    s += "#include <memory>\n"
    s += "#include <unordered_map>\n"
    s += '#include "%s_config.pb.h"\n' % sheet_name_lower
    s += 'class %sConfigurationTable\n{\npublic:\n' % sheetname
    s += '    using row_type = const %s_row*;\n' % sheet_name_lower
    s += '    using kv_type = std::unordered_map<uint32_t, row_type>;\n'
    s += '    static %sConfigurationTable& GetSingleton() { static %sConfigurationTable singleton; return singleton; }\n' % (sheetname, sheetname)
    s += '    const %s_table& All() const { return data_; }\n' % sheet_name_lower
    s += '    row_type GetTable(uint32_t keyid);\n'
    counter = 0
    for d in datastring:
        for v in d.values():
            s += '    row_type key_%s(uint32_t keyid) const;\n' % v
            counter += 1
    s += '    void Load();\n'
    s += 'private:\n'
    s += '    %s_table data_;\n' % sheet_name_lower
    s += '    kv_type key_data_;\n'
    for i in range(counter):
        s += '    kv_type key_data_%s_;\n' % i
    s += '};\n'
    s += 'const %sConfigurationTable::row_type Get%sTable(uint32_t keyid);\n' % (sheetname, sheetname)
    s += 'const %s_table& Get%sAllTable();\n' % (sheet_name_lower, sheetname)
    return s


def generate_cpp_implementation(datastring, sheetname):
    """生成C++实现文件内容"""
    sheet_name_lower = sheetname.lower()
    s = '#include "google/protobuf/util/json_util.h"\n'
    s += '#include "src/util/file2string.h"\n'
    s += '#include "muduo/base/Logging.h"\n'
    s += '#include "%s_config.h"\n\n' % sheetname
    s += 'void %sConfigurationTable::Load()\n{\n' % sheetname
    s += '    data_.Clear();\n'
    s += '    const auto contents = File2String("config/generated/json/%s.json");\n' % sheet_name_lower
    s += '    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_);\n'
    s += '        !result.ok())\n'
    s += '    {\n'
    s += '        LOG_FATAL << "%s " << result.message().data();\n' % sheetname
    s += '    }\n'
    s += '    for (int32_t i = 0; i < data_.data_size(); ++i)\n'
    s += '    {\n'
    counter = 0
    for d in datastring:
        for v in d.values():
            s += '        key_data_%s_.clear();\n' % counter
            counter += 1
    s += '    }\n'
    s += '    for (int32_t i = 0; i < data_.data_size(); ++i)\n'
    s += '    {\n'
    s += '        const auto& row_data = data_.data(i);\n'
    counter = 0
    for d in datastring:
        s += '        key_data_.emplace(row_data.id(), &row_data);\n'
        for v in d.values():
            s += '        key_data_%s_.emplace(row_data.%s(), &row_data);\n' % (counter, v)
            counter += 1
    s += '    }\n'
    s += '}\n\n'
    s += 'const %s_row* %sConfigurationTable::GetTable(uint32_t keyid)\n{\n' % (sheet_name_lower, sheetname)
    s += '    const auto it = key_data_.find(keyid);\n'
    s += '    return it == key_data_.end() ? nullptr : it->second;\n}\n'
    counter = 0
    for d in datastring:
        for v in d.values():
            s += 'const %s_row* %sConfigurationTable::key_%s(uint32_t keyid) const\n{\n' % (
                sheet_name_lower, sheetname, v)
            s += '    const auto it = key_data_%s_.find(keyid);\n' % counter
            s += '    return it == key_data_%s_.end() ? nullptr : it->second;\n}\n' % counter
            counter += 1
    s += '\nconst %sConfigurationTable::row_type Get%sTable(uint32_t keyid)\n{\n' % (sheetname, sheetname)
    s += '    return %sConfigurationTable::GetSingleton().GetTable(keyid);\n}\n\n' % sheetname
    s += '\nconst %s_table& Get%sAllTable()\n{\n' % (sheet_name_lower, sheetname)
    s += '    return %sConfigurationTable::GetSingleton().All();\n}\n' % sheetname
    return s


def process_workbook(filename):
    """处理单个工作簿文件，生成对应的头文件和实现文件"""
    workbook = xlrd.open_workbook(filename)
    workbook_data = get_workbook_data(workbook)
    for sheetname in workbook_data:
        header_filename = sheetname.lower() + "_config.h"
        cpp_filename = sheetname.lower() + "_config.cpp"

        cpp_header_content = generate_cpp_header(workbook_data[sheetname], sheetname)
        gencommon.mywrite(cpp_header_content, os.path.join(cppdir, header_filename))

        cpp_implementation_content = generate_cpp_implementation(workbook_data[sheetname], sheetname)
        gencommon.mywrite(cpp_implementation_content, os.path.join(cppdir, cpp_filename))


def generate_all_config():
    """生成加载所有配置的头文件和实现文件内容"""
    sheetnames = []
    dirfiles = listdir(xlsdir)
    files = sorted(dirfiles, key=lambda file: os.path.getsize(xlsdir + file), reverse=True)
    for filename in files:
        filename = xlsdir + filename
        if filename.endswith('.xlsx') or filename.endswith('.xls'):
            workbook = xlrd.open_workbook(filename)
            workbook_data = get_workbook_data(workbook)
            for sheetname in workbook_data:
                sheetnames.append(sheetname)

    header_content = '#pragma once\n'
    header_content += 'void LoadAllConfig();\n'
    header_content += 'void LoadAllConfigAsyncWhenServerLaunch();\n'

    cpp_content = '#include "all_config.h"\n\n'
    cpp_content += '#include <thread>\n'
    cpp_content += '#include "muduo/base/CountDownLatch.h"\n\n'

    for item in sheetnames:
        cpp_content += '#include "%s_config.h"\n' % item

    cpp_content += 'void LoadAllConfig()\n{\n'
    for item in sheetnames:
        cpp_content += '    %sConfigurationTable::GetSingleton().Load();\n' % item
    cpp_content += '}\n\n'

    cpucount = cpu_count()
    cpp_content += 'void LoadAllConfigAsyncWhenServerLaunch()\n{\n'

    cpustr = [[] for _ in range(cpucount)]
    count = 0
    realthreadcount = 0

    for item in sheetnames:
        loadstr = '    %sConfigurationTable::GetSingleton().Load();\n' % item
        if count >= cpucount:
            count = 0
        cpustr[count].append(loadstr)
        count += 1
        if realthreadcount < count:
            realthreadcount = count

    cpp_content += '    static muduo::CountDownLatch latch_(' + str(realthreadcount) + ');\n'

    for group in cpustr:
        if len(group) <= 0:
            continue
        cpp_content += '\n    /// Begin\n'
        cpp_content += '    {\n'
        cpp_content += '        std::thread t([&]() {\n\n'
        for blockstr in group:
            cpp_content += blockstr
        cpp_content += '            latch_.countDown();\n'
        cpp_content += '        });\n'
        cpp_content += '        t.detach();\n'
        cpp_content += '    }\n'
        cpp_content += '    /// End\n'

    cpp_content += '    latch_.wait();\n'
    cpp_content += '}\n'

    return header_content, cpp_content


def main():
    """主函数，生成所有配置相关文件"""
    if not os.path.exists(cppdir):
        os.makedirs(cppdir)

    # 获取xlsx文件列表
    xlsx_files = [os.path.join(xlsdir, filename) for filename in listdir(xlsdir)
                  if filename.endswith('.xlsx') or filename.endswith('.xls')]

    # 多线程处理xlsx文件
    with concurrent.futures.ProcessPoolExecutor() as executor:
        executor.map(process_workbook, xlsx_files)

    # 生成加载所有配置的头文件和实现文件
    header_content, cpp_content = generate_all_config()
    gencommon.mywrite(header_content, os.path.join(cppdir, "all_config.h"))
    gencommon.mywrite(cpp_content, os.path.join(cppdir, "all_config.cpp"))


if __name__ == "__main__":
    main()
