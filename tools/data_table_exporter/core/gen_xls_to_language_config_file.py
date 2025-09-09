#!/usr/bin/env python
# coding=utf-8

import concurrent.futures
import os
import openpyxl
import logging
from pathlib import Path
from multiprocessing import cpu_count
from jinja2 import Environment, FileSystemLoader

import generate_common  # 你项目中的工具模块
from core.constants import PROJECT_GENERATED_CODE_CPP_DIR, PROJECT_GENERATED_CODE_GO_DIR, XLSX_DIR

# 日志配置
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# 类型转换函数
def get_cpp_type_name(type_name):
    if type_name == 'string':
        return 'std::string'
    elif 'int' in type_name:
        return f'{type_name}_t'
    return type_name

def get_cpp_type_param_name_with_ref(type_name):
    if type_name == 'string':
        return 'const std::string&'
    elif 'int' in type_name:
        return f'{type_name}_t'
    return type_name

def convert_to_go_type(col_type):
    if "int32" in col_type:
        return "int32"
    elif "int64" in col_type:
        return "int64"
    elif "float" in col_type:
        return "float32"
    elif "double" in col_type:
        return "float64"
    elif "bool" in col_type:
        return "bool"
    elif "string" in col_type:
        return "string"
    return "interface{}"

# 提取工作簿数据
def get_workbook_data(workbook):
    workbook_data = {}
    sheet_names = workbook.sheetnames
    if sheet_names:
        sheet = workbook[sheet_names[0]]
        cell_value = sheet['A5'].value
        use_flat_multimap = cell_value is not None and cell_value.lower() == 'multi'
        first_19_rows_per_column = generate_common.get_first_19_rows_per_column(sheet)
        workbook_data[sheet_names[0]] = {
            'multi': use_flat_multimap,
            'get_first_19_rows_per_column': first_19_rows_per_column
        }
    return workbook_data

# 处理单个 xlsx 文件
def process_workbook(filename):
    try:
        workbook = openpyxl.load_workbook(filename)
        workbook_data = get_workbook_data(workbook)
        for sheetname, data in workbook_data.items():
            header_filename = f"{sheetname.lower()}_table.h"
            cpp_filename = f"{sheetname.lower()}_table.cpp"
            go_filename = f"{sheetname.lower()}_table.go"

            env = Environment(loader=FileSystemLoader(generate_common.TEMPLATE_DIR, encoding='utf-8'), auto_reload=True)

            # === C++ 头文件 ===
            header_template = env.get_template('config_template.h.jinja')
            header_content = header_template.render(
                datastring=data['get_first_19_rows_per_column'],
                sheetname=sheetname,
                use_flat_multimap=data['multi'],
                generate_common=generate_common,
                get_cpp_type_param_name_with_ref=get_cpp_type_param_name_with_ref,
                get_cpp_type_name=get_cpp_type_name
            )
            generate_common.mywrite(header_content, PROJECT_GENERATED_CODE_CPP_DIR / header_filename)

            # === C++ 实现文件 ===
            implementation_template = env.get_template('config_template.cpp.jinja')
            implementation_content = implementation_template.render(
                datastring=data['get_first_19_rows_per_column'],
                sheetname=sheetname,
                generate_common=generate_common,
                get_cpp_type_param_name_with_ref=get_cpp_type_param_name_with_ref,
                get_cpp_type_name=get_cpp_type_name
            )
            generate_common.mywrite(implementation_content, PROJECT_GENERATED_CODE_CPP_DIR / cpp_filename)

            # === Go 文件 ===
            go_template = env.get_template("config_template.go.jinja")
            go_content = go_template.render(
                datastring=data['get_first_19_rows_per_column'],
                sheetname=sheetname,
                generate_common=generate_common,
                convert_to_go_type=convert_to_go_type,  # 动态转换
                proto_import_path="your/proto/package/path"  # ⚠️ 请修改为你真实路径
            )
            PROJECT_GENERATED_CODE_GO_DIR.mkdir(parents=True, exist_ok=True)
            generate_common.mywrite(go_content, PROJECT_GENERATED_CODE_GO_DIR / go_filename)

    except Exception as e:
        logging.error(f"Failed to load or process workbook {filename}: {e}")

# 生成 all_table.h / .cpp
def generate_all_config():
    sheetnames = set()
    xlsx_files = sorted(Path(XLSX_DIR).glob('*.xlsx'), key=lambda f: f.stat().st_size, reverse=True)
    for filepath in xlsx_files:
        try:
            workbook = openpyxl.load_workbook(filepath)
            workbook_data = get_workbook_data(workbook)
            sheetnames.update(workbook_data.keys())
        except Exception as e:
            logging.error(f"Failed to process file {filepath}: {e}")

    sheetnames = sorted(sheetnames)
    cpucount = cpu_count()

    env = Environment(loader=FileSystemLoader(generate_common.TEMPLATE_DIR, encoding='utf-8'))
    header_template = env.get_template("all_table.h.jinja")
    cpp_template = env.get_template("all_table.cpp.jinja")

    header_content = header_template.render()
    cpp_content = cpp_template.render(sheetnames=sheetnames, cpucount=cpucount)

    go_template = env.get_template("all_table.go.jinja")
    go_content = go_template.render(sheetnames=sheetnames)

    PROJECT_GENERATED_CODE_GO_DIR.mkdir(parents=True, exist_ok=True)
    generate_common.mywrite(go_content, PROJECT_GENERATED_CODE_GO_DIR / "all_table.go")

    return header_content, cpp_content

# 主函数
def main(XLS_DIR=None):
    PROJECT_GENERATED_CODE_CPP_DIR.mkdir(parents=True, exist_ok=True)
    PROJECT_GENERATED_CODE_GO_DIR.mkdir(parents=True, exist_ok=True)

    xlsx_files = [XLS_DIR / filename for filename in os.listdir(XLS_DIR) if filename.endswith('.xlsx')]

    with concurrent.futures.ProcessPoolExecutor() as executor:
        executor.map(process_workbook, xlsx_files)

    header_content, cpp_content = generate_all_config()
    generate_common.mywrite(header_content, PROJECT_GENERATED_CODE_CPP_DIR / "all_table.h")
    generate_common.mywrite(cpp_content, PROJECT_GENERATED_CODE_CPP_DIR / "all_table.cpp")

if __name__ == "__main__":
    main()
