#!/usr/bin/env python
# coding=utf-8

import os
import openpyxl
import gencommon
import concurrent.futures
from os import listdir
from multiprocessing import cpu_count

# Global Variables
KEY_ROW_IDX = 4
CPP_DIR = "generated/cpp/"
XLS_DIR = "xlsx/"


def get_column_names(sheet):
    """Get column names from the Excel sheet."""
    return [cell.value for cell in sheet[KEY_ROW_IDX]]


def get_key_row_data(row, column_names):
    """Extract key row data from the Excel sheet."""
    return {str(row[i].value): row[i].value for i in range(len(row)) if column_names[i].strip() == "key"}


def get_sheet_key_data(sheet, column_names):
    """Extract key data from the sheet."""
    row = sheet[1]  # Assuming the key row is the second row
    return [get_key_row_data(row, column_names)]


def get_workbook_data(workbook):
    """Extract data from the entire workbook."""
    workbook_data = {}
    sheet_names = workbook.sheetnames
    for sheet_name in sheet_names:
        sheet = workbook[sheet_name]
        column_names = get_column_names(sheet)
        sheet_key_data = get_sheet_key_data(sheet, column_names)
        workbook_data[sheet_name] = sheet_key_data
    return workbook_data


def generate_cpp_header(datastring, sheetname):
    """Generate C++ header file content."""
    sheet_name_lower = sheetname.lower()
    header_content = [
        "#pragma once",
        "#include <memory>",
        "#include <unordered_map>",
        f'#include "{sheet_name_lower}_config.pb.h"',
        f'class {sheetname}ConfigurationTable {{',
        f'public:',
        f'    using row_type = const {sheet_name_lower}_row*;',
        f'    using kv_type = std::unordered_map<uint32_t, row_type>;',
        f'    static {sheetname}ConfigurationTable& GetSingleton() {{ static {sheetname}ConfigurationTable singleton; return singleton; }}',
        f'    const {sheet_name_lower}_table& All() const {{ return data_; }}',
        f'    const std::pair<row_type, uint32_t> GetTable(uint32_t keyid);',
        '    void Load();',
        'private:',
        f'    {sheet_name_lower}_table data_;',
        '    kv_type key_data_;',
    ]

    for d in datastring:
        for v in d.values():
            header_content.append(f'    row_type key_{v}(uint32_t keyid) const;')

    header_content.append('};')
    header_content.append(
        f'\ninline std::pair<{sheetname}ConfigurationTable::row_type, uint32_t> Get{sheetname}Table(uint32_t keyid) {{ return {sheetname}ConfigurationTable::GetSingleton().GetTable(keyid); }}')
    header_content.append(
        f'\ninline const {sheet_name_lower}_table& Get{sheetname}AllTable() {{ return {sheetname}ConfigurationTable::GetSingleton().All(); }}')

    return '\n'.join(header_content)


def generate_cpp_implementation(datastring, sheetname):
    """Generate C++ implementation file content."""
    sheet_name_lower = sheetname.lower()
    cpp_content = [
        '#include "google/protobuf/util/json_util.h"',
        '#include "src/util/file2string.h"',
        '#include "muduo/base/Logging.h"',
        '#include "common_error_tip.pb.h"',
        f'#include "{sheet_name_lower}_config.h"\n',
        f'void {sheetname}ConfigurationTable::Load() {{',
        '    data_.Clear();',
        f'    const auto contents = File2String("config/generated/json/{sheet_name_lower}.json");',
        f'    if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_); !result.ok()) {{',
        f'        LOG_FATAL << "{sheetname} " << result.message().data();',
        '    }',
        '    for (int32_t i = 0; i < data_.data_size(); ++i) {',
        '        const auto& row_data = data_.data(i);',
        '        key_data_.emplace(row_data.id(), &row_data);',
    ]

    for d in datastring:
        for v in d.values():
            cpp_content.append(f'        key_data_{v}_.emplace(row_data.{v}(), &row_data);')

    cpp_content.extend([
        '    }',
        '}\n\n',
        f'const std::pair<{sheetname}ConfigurationTable::row_type, uint32_t> {sheetname}ConfigurationTable::GetTable(uint32_t keyid) {{',
        '    const auto it = key_data_.find(keyid);',
        '    if (it == key_data_.end()) {',
        f'        LOG_ERROR << "{sheetname} table not found for ID: " << keyid;',
        '        return { nullptr, kInvalidTableId };',
        '    }',
        '    return { it->second, kOK };',
        '}',
    ])

    for d in datastring:
        for v in d.values():
            cpp_content.append(
                f'const {sheet_name_lower}_row* {sheetname}ConfigurationTable::key_{v}(uint32_t keyid) const {{')
            cpp_content.append(f'    const auto it = key_data_{v}_.find(keyid);')
            cpp_content.append(f'    return it == key_data_{v}_.end() ? nullptr : it->second;')
            cpp_content.append('}')

    return '\n'.join(cpp_content)


def process_workbook(filename):
    """Process a single workbook file and generate corresponding header and implementation files."""
    try:
        workbook = openpyxl.load_workbook(filename)
    except Exception as e:
        print(f"Failed to load workbook {filename}: {e}")
        return

    workbook_data = get_workbook_data(workbook)
    for sheetname, data in workbook_data.items():
        header_filename = f"{sheetname.lower()}_config.h"
        cpp_filename = f"{sheetname.lower()}_config.cpp"

        cpp_header_content = generate_cpp_header(data, sheetname)
        gencommon.mywrite(cpp_header_content, os.path.join(CPP_DIR, header_filename))

        cpp_implementation_content = generate_cpp_implementation(data, sheetname)
        gencommon.mywrite(cpp_implementation_content, os.path.join(CPP_DIR, cpp_filename))


def generate_all_config():
    """Generate header and implementation files for loading all configurations."""
    sheetnames = []
    dirfiles = listdir(XLS_DIR)
    files = sorted(dirfiles, key=lambda file: os.path.getsize(os.path.join(XLS_DIR, file)), reverse=True)

    for filename in files:
        filepath = os.path.join(XLS_DIR, filename)
        if filepath.endswith('.xlsx'):
            try:
                workbook = openpyxl.load_workbook(filepath)
                workbook_data = get_workbook_data(workbook)
                sheetnames.extend(workbook_data.keys())
            except Exception as e:
                print(f"Failed to process file {filepath}: {e}")

    header_content = '#pragma once\n'
    header_content += 'void LoadAllConfig();\n'
    header_content += 'void LoadAllConfigAsyncWhenServerLaunch();\n'

    cpp_content = '#include "all_config.h"\n\n'
    cpp_content += '#include <thread>\n'
    cpp_content += '#include "muduo/base/CountDownLatch.h"\n\n'

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
    os.makedirs(CPP_DIR, exist_ok=True)

    # List of Excel files
    xlsx_files = [os.path.join(XLS_DIR, filename) for filename in listdir(XLS_DIR)
                  if filename.endswith('.xlsx')]

    # Process Excel files in parallel
    with concurrent.futures.ProcessPoolExecutor() as executor:
        executor.map(process_workbook, xlsx_files)

    # Generate header and implementation files for all configurations
    header_content, cpp_content = generate_all_config()
    gencommon.mywrite(header_content, os.path.join(CPP_DIR, "all_config.h"))
    gencommon.mywrite(cpp_content, os.path.join(CPP_DIR, "all_config.cpp"))


if __name__ == "__main__":
    main()
