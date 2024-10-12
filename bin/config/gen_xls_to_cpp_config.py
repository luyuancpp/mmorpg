#!/usr/bin/env python
# coding=utf-8

import concurrent.futures
import os
import openpyxl
import gen_common
import concurrent.futures
from pathlib import Path
from multiprocessing import cpu_count
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Global Variables
KEY_ROW_IDX = 4
CPP_DIR = Path("generated/cpp")
XLS_DIR = Path("xlsx")


def get_column_names(sheet):
    """Get column names from the Excel sheet."""
    return [cell.value for cell in sheet[KEY_ROW_IDX] if cell.value is not None]


def get_key_row_data(row, column_names):
    """Extract key row data from the Excel sheet."""
    return {str(row[i].value): row[i].value for i in range(len(row)) if column_names[i].strip().lower() == "key"}


def get_workbook_data(workbook):
    """Extract data from the first sheet of the workbook."""
    workbook_data = {}
    sheet_names = workbook.sheetnames
    if sheet_names:  # Read only the first sheet
        sheet = workbook[sheet_names[0]]

        # Check if A5 cell value is 'multi' or None
        cell_value = sheet['A5'].value
        use_flat_multimap = cell_value is not None and cell_value.lower() == 'multi'
        first_19_rows_per_column = gen_common.get_first_19_rows_per_column(sheet)
        workbook_data[sheet_names[0]] = {
            'multi': use_flat_multimap,
            'get_first_19_rows_per_column': first_19_rows_per_column
        }
    return workbook_data


def get_cpp_type_name(type_name):
    """Return C++ type name based on the provided type_name."""
    if type_name == 'string':
        return 'std::string'
    elif 'int' in type_name:
        return f'{type_name}_t'
    return type_name


def get_cpp_type_param_name_with_ref(type_name):
    """Return C++ type parameter name with reference."""
    if type_name == 'string':
        return 'const std::string&'
    elif 'int' in type_name:
        return f'{type_name}_t'
    return type_name


def generate_cpp_header(datastring, sheetname, use_flat_multimap):
    """Generate C++ header file content."""
    sheet_name_lower = sheetname.lower()
    container_type = "unordered_multimap" if use_flat_multimap else "unordered_map"

    table_type = f'{sheetname}Table*'
    const_table_type = f'const {table_type}'
    table_data_name = f'{sheetname}TabledData'
    get_table_return_type = f'std::pair<{const_table_type}, uint32_t>'

    header_content = [
        "#pragma once",
        "#include <cstdint>",
        "#include <memory>",
        "#include <unordered_map>",
        '#include "config_expression/config_expression.h"',
        f'#include "{sheet_name_lower}_config.pb.h"\n\n',
        f'class {sheetname}ConfigurationTable {{',
        'public:',
        f'    using KVDataType = std::{container_type}<uint32_t, {const_table_type}>;',
        f'    static {sheetname}ConfigurationTable& GetSingleton() {{ static {sheetname}ConfigurationTable singleton; return singleton; }}',
        f'    const {table_data_name}& All() const {{ return data_; }}',
        f'    {get_table_return_type} GetTable(uint32_t keyId);',
        f'    const KVDataType& KVData() const {{ return kv_data_; }}',
        '    void Load();\n',
    ]

    for data in datastring:
        column_name = data[gen_common.COL_OBJ_COLUMN_NAME]
        if data[gen_common.COL_OBJ_TABLE_KEY_INDEX] == gen_common.TABLE_KEY_CELL:
            column_map_type = 'unordered_map'
            if data[gen_common.COL_OBJ_TABLE_MULTI] == gen_common.MULTI_TABLE_KEY_CELL:
                column_map_type = "unordered_multimap"
            header_content.extend([
                f'    {get_table_return_type} GetBy{column_name.title()}({get_cpp_type_param_name_with_ref(data[gen_common.COL_OBJ_COLUMN_TYPE])} keyId) const;',
                f'    const std::{column_map_type}<{get_cpp_type_name(data[gen_common.COL_OBJ_COLUMN_TYPE])}, {const_table_type}>& Get{column_name.title()}Data() const {{ return kv_{column_name}data_; }}'
            ])

        if data[gen_common.COL_OBJ_TABLE_EXPRESSION_TYPE_INDEX] is not None:
            type_name = data[gen_common.COL_OBJ_TABLE_EXPRESSION_TYPE_INDEX]
            header_content.extend([
                f'    void Set{column_name.title()}Param(const std::vector<{type_name}>& paramList){{',
                f'      expression_{column_name}_.SetParam(paramList); ',
                f'    }}\n ',
                f'    {type_name} Get{column_name.title()}(const uint32_t keyId){{',
                f'      auto [table, ok] = GetTable(keyId);',
                f'      if ( table == nullptr){{{{return {type_name}(); }}}}',
                f'      return expression_{column_name}_.Value(table->{column_name}());',
                f'     }} ',
            ])

    header_content.extend(
        [
            '\nprivate:',
            f'    {table_data_name} data_;',
            '    KVDataType kv_data_;\n',
        ]
    )

    for data in datastring:
        column_name = data[gen_common.COL_OBJ_COLUMN_NAME]
        if data[gen_common.COL_OBJ_TABLE_KEY_INDEX] == gen_common.TABLE_KEY_CELL:
            column_map_type = 'unordered_map'
            if data[gen_common.COL_OBJ_TABLE_MULTI] == gen_common.MULTI_TABLE_KEY_CELL:
                column_map_type = "unordered_multimap"
            type_name = get_cpp_type_name(data[gen_common.COL_OBJ_COLUMN_TYPE])
            header_content.append(f'    std::{column_map_type}<{type_name}, {const_table_type}>  kv_{column_name}data_;')
        if data[gen_common.COL_OBJ_TABLE_EXPRESSION_TYPE_INDEX] is not None:
            header_content.append(
                f'    ExcelExpression<{data[gen_common.COL_OBJ_TABLE_EXPRESSION_TYPE_INDEX]}> expression_{column_name}_;')

    header_content.append('};')
    header_content.append(
        f'\ninline {get_table_return_type} Get{sheetname}Table(const uint32_t keyId) {{ return {sheetname}ConfigurationTable::GetSingleton().GetTable(keyId); }}')
    header_content.append(
        f'\ninline const {table_data_name}& Get{sheetname}AllTable() {{ return {sheetname}ConfigurationTable::GetSingleton().All(); }}')

    return '\n'.join(header_content)


def generate_cpp_implementation(datastring, sheetname, use_flat_multimap):
    """Generate C++ implementation file content."""
    sheet_name_lower = sheetname.lower()
    container_type = "unordered_multimap" if use_flat_multimap else "unordered_map"
    table_type = f'{sheetname}Table*'
    const_table_type = f'const {table_type}'
    get_table_return_type = f'std::pair<{const_table_type}, uint32_t>'

    cpp_content = [
        '#include "google/protobuf/util/json_util.h"',
        '#include "src/util/file2string.h"',
        '#include "muduo/base/Logging.h"',
        '#include "common_error_tip.pb.h"',
        f'#include "{sheet_name_lower}_config.h"\n',
        f'void {sheetname}ConfigurationTable::Load() {{',
        '    data_.Clear();',
        f'    const auto contents = File2String("config/generated/json/{sheet_name_lower}.json");',
        f'    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); '
        f'!result.ok()) {{',
        f'        LOG_FATAL << "{sheetname} " << result.message().data();',
        '    }\n'
    ]

    cpp_content.extend(
        [  f'    for (int32_t i = 0; i < data_.data_size(); ++i) {{ ',
        f'        const auto& row_data = data_.data(i);',
        f'        kv_data_.emplace(row_data.id(), &row_data);\n\n',
        ]
    )

    for data in datastring:
        column_name = data[gen_common.COL_OBJ_COLUMN_NAME]
        if data[gen_common.COL_OBJ_TABLE_KEY_INDEX] == gen_common.TABLE_KEY_CELL:
            cpp_content.append(f'        kv_{column_name}data_.emplace(row_data.{column_name}(), &row_data);')

    cpp_content.extend([
        f'    }}\n',
    ])

    for data in datastring:
        cell_value = data[gen_common.COL_OBJ_TABLE_EXPRESSION_PARAM_NAMES_INDEX]

        # 跳过空值
        if cell_value is None:
            continue

        # 统计单词并按逗号分隔
        word_count, word_list = gen_common.count_words_by_comma(cell_value)

        # 如果没有有效的单词，继续下一个循环
        if word_count <= 0:
            continue

        # 提取列名，但这里暂时没有使用
        column_name = data[gen_common.COL_OBJ_COLUMN_NAME]

        # 开始生成 C++ 内容
        cpp_content.append('    {')  # 添加一个块的起始括号

        # 构造 StringVector 并添加到 C++ 代码
        cpp_content.append(f'      expression_{column_name}_.Init({{')

        # 按照单词列表生成 C++ 代码，最后一个单词不需要逗号
        for i, word in enumerate(word_list):
            if i == len(word_list) - 1:
                cpp_content.append(f'   "{word}"')  # 最后一个单词不加逗号
            else:
                cpp_content.append(f'   "{word}", ')

        cpp_content.append('     });')  # 结束 StringVector 的定义
        cpp_content.append('    }')  # 结束整个块

    cpp_content.extend([
        '}\n\n',
        f'{get_table_return_type} {sheetname}ConfigurationTable::GetTable(const uint32_t keyId) {{',
        '    const auto it = kv_data_.find(keyId);',
        '    if (it == kv_data_.end()) {',
        f'        LOG_ERROR << "{sheetname} table not found for ID: " << keyId;',
        '        return { nullptr, kInvalidTableId };',
        '    }',
        '    return { it->second, kOK };',
        '}\n\n',
    ])

    for data in datastring:
        column_name = data[gen_common.COL_OBJ_COLUMN_NAME]
        if data[gen_common.COL_OBJ_TABLE_KEY_INDEX] == gen_common.TABLE_KEY_CELL:
            type_name = get_cpp_type_param_name_with_ref(data[gen_common.COL_OBJ_COLUMN_TYPE])
            cpp_content.extend([
                f'{get_table_return_type} '
                f'{sheetname}ConfigurationTable::GetBy{column_name.title()}({type_name} keyId) const {{',
                f'    const auto it = kv_{column_name}data_.find(keyId);',
                f'    if (it == kv_{column_name}data_.end()) {{',
                f'        LOG_ERROR << "{sheetname} table not found for ID: " << keyId;',
                '        return { nullptr, kInvalidTableId };',
                '    }',
                '    return { it->second, kOK };',
                '}\n',
            ])

    return '\n'.join(cpp_content)


def process_workbook(filename):
    """Process a single workbook file and generate corresponding header and implementation files."""
    try:
        workbook = openpyxl.load_workbook(filename)
        workbook_data = get_workbook_data(workbook)
        for sheetname, data in workbook_data.items():
            header_filename = f"{sheetname.lower()}_config.h"
            cpp_filename = f"{sheetname.lower()}_config.cpp"

            cpp_header_content = generate_cpp_header(data['get_first_19_rows_per_column'], sheetname, data['multi'])
            gen_common.mywrite(cpp_header_content, CPP_DIR / header_filename)

            cpp_implementation_content = generate_cpp_implementation(data['get_first_19_rows_per_column'], sheetname,
                                                                     data['multi'])
            gen_common.mywrite(cpp_implementation_content, CPP_DIR / cpp_filename)
    except Exception as e:
        logging.error(f"Failed to load or process workbook {filename}: {e}")


def generate_all_config():
    """Generate header and implementation files for loading all configurations."""
    sheetnames = []
    xlsx_files = sorted(XLS_DIR.glob('*.xlsx'), key=lambda f: f.stat().st_size, reverse=True)

    for filepath in xlsx_files:
        try:
            workbook = openpyxl.load_workbook(filepath)
            workbook_data = get_workbook_data(workbook)
            sheetnames.extend(workbook_data.keys())
        except Exception as e:
            logging.error(f"Failed to process file {filepath}: {e}")

    header_content = '#pragma once\nvoid LoadAllConfig();\nvoid LoadAllConfigAsyncWhenServerLaunch();\n'
    cpp_content = '#include "all_config.h"\n\n#include <thread>\n#include "muduo/base/CountDownLatch.h"\n\n'

    for item in sheetnames:
        cpp_content += f'#include "{item.lower()}_config.h"\n'

    cpp_content += 'void LoadAllConfig()\n{\n'
    for item in sheetnames:
        cpp_content += f'    {item}ConfigurationTable::GetSingleton().Load();\n'
    cpp_content += '}\n\n'

    cpucount = cpu_count()
    cpp_content += 'void LoadAllConfigAsyncWhenServerLaunch()\n{\n'
    cpp_content += f'    static muduo::CountDownLatch latch_({len(sheetnames)});\n'

    load_blocks = [[] for _ in range(cpucount)]
    for idx, item in enumerate(sheetnames):
        load_blocks[idx % cpucount].append(f'    {item}ConfigurationTable::GetSingleton().Load();\n')

    for block in load_blocks:
        if block:
            cpp_content += '\n    /// Begin\n'
            cpp_content += '    {\n'
            cpp_content += '        std::thread t([&]() {\n\n'
            cpp_content += ''.join(block)
            cpp_content += '            latch_.countDown();\n'
            cpp_content += '        });\n'
            cpp_content += '        t.detach();\n'
            cpp_content += '    }\n'
            cpp_content += '    /// End\n'

    cpp_content += '    latch_.wait();\n'
    cpp_content += '}\n'

    return header_content, cpp_content


def main():
    """Main function to generate all configuration files."""
    CPP_DIR.mkdir(parents=True, exist_ok=True)

    # List of Excel files
    xlsx_files = [XLS_DIR / filename for filename in os.listdir(XLS_DIR) if filename.endswith('.xlsx')]

    # Process Excel files in parallel
    with concurrent.futures.ProcessPoolExecutor() as executor:
        executor.map(process_workbook, xlsx_files)

    # Generate header and implementation files for all configurations
    header_content, cpp_content = generate_all_config()
    gen_common.mywrite(header_content, CPP_DIR / "all_config.h")
    gen_common.mywrite(cpp_content, CPP_DIR / "all_config.cpp")


if __name__ == "__main__":
    main()
