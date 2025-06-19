# !/usr/bin/env python
# coding=utf-8

import os
import logging
import concurrent.futures
from os import listdir
from os.path import isfile, join
import openpyxl
import multiprocessing
from typing import List, Optional
import utils
from jinja2 import Environment, FileSystemLoader
from pathlib import Path

import generate_common
from common import constants

# 设置日志配置
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class ExcelToCppConverter:
    def __init__(self, excel_file: str):
        self.excel_file = excel_file
        self.workbook = openpyxl.load_workbook(excel_file)
        self.sheet = self.workbook.sheetnames[0]
        self.worksheet = self.workbook[self.sheet]
        self.constants_name_index = self._find_constants_name_index()

    def _find_constants_name_index(self) -> Optional[int]:
        """
        查找 constants_name 列的索引
        返回: 如果找到返回索引值，否则返回 None
        """
        try:
            # 确保第一行存在且有数据
            if not self.worksheet or self.worksheet.max_row < 1:
                logger.warning(f"工作表 '{self.sheet}' 为空或没有数据")
                return None

            # 获取第一行的所有单元格
            first_row = self.worksheet[1]
            if not first_row:
                logger.warning(f"工作表 '{self.sheet}' 第一行为空")
                return None

            # 获取所有列标题
            headers = []
            for cell in first_row:
                if cell.value is None:
                    headers.append('')
                else:
                    headers.append(str(cell.value).strip())

            # 查找 constants_name 列
            try:
                return headers.index('constants_name')
            except ValueError:
                logger.info(f"工作表 '{self.sheet}' 中未找到 'constants_name' 列")
                return None

        except Exception as e:
            logger.error(f"查找 constants_name 列时发生错误: {str(e)}")
            return None

    def should_process(self) -> bool:
        return self.constants_name_index is not None

    def generate_cpp_constants(self) -> str:
        constants_list = []
        for row in self.worksheet.iter_rows(min_row=20, values_only=True):
            id_value = row[0]
            if id_value is None:
                continue
            constant_name = self._generate_constant_name(row, id_value)
            constants_list.append({'name': constant_name, 'value': id_value})

        env = Environment(loader=FileSystemLoader(generate_common.TEMPLATE_DIR))
        template = env.get_template("constants.h.j2")
        return template.render(constants=constants_list)

    def _generate_constant_name(self, row: tuple, id_value: int) -> str:
        if self.constants_name_index is not None and row[self.constants_name_index]:
            return f'k{self.sheet}_{row[self.constants_name_index]}'
        return f'k{self.sheet}_{id_value}'

    def save_cpp_constants_to_file(self, cpp_code: str) -> None:
        output_path = os.path.join(
            constants.GENERATOR_CONSTANTS_NAME_DIR,
            f"{self.sheet.lower()}_table_id_constants.h"
        )
        with open(output_path, 'w') as f:
            f.write(cpp_code)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if hasattr(self, 'workbook'):
            self.workbook.close()


def process_file(file_path: str) -> None:
    """处理单个Excel文件并生成对应的C++常量文件"""
    if not os.path.isfile(file_path):
        logger.error(f"文件不存在: {file_path}")
        return

    try:
        # 在上下文语法前尝试初始化
        converter = ExcelToCppConverter(file_path)
    except Exception as e:
        logger.error(f"初始化 ExcelToCppConverter 时发生错误: {file_path}, 错误: {str(e)}")
        return

    try:
        with converter:
            if converter.should_process():
                cpp_code = converter.generate_cpp_constants()
                converter.save_cpp_constants_to_file(cpp_code)
                logger.info(f"已处理: {file_path}")
            else:
                logger.info(f"已跳过 (无constants_name): {file_path}")
    except Exception as e:
        logger.error(f"处理文件 {file_path} 时发生错误: {str(e)}")



def process_files_with_executor(files: List[str]) -> None:
    """使用线程池处理多个文件"""
    num_threads = min(multiprocessing.cpu_count(), len(files))
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        try:
            # 提交所有任务
            futures = [executor.submit(process_file, f) for f in files]

            # 等待所有任务完成并处理结果
            for future in concurrent.futures.as_completed(futures):
                try:
                    future.result()
                except Exception as e:
                    logger.error(f"任务执行失败: {str(e)}")
        except Exception as e:
            logger.error(f"线程池执行过程中发生错误: {str(e)}")
        finally:
            # 确保所有任务都完成
            executor.shutdown(wait=True)


def main() -> None:
    """主函数：处理所有Excel文件并生成C++常量文件"""
    try:
        # 确保输出目录存在
        os.makedirs(constants.GENERATOR_CONSTANTS_NAME_DIR, exist_ok=True)

        # 获取所有需要处理的Excel文件
        xlsx_files = utils.get_xlsx_files(constants.XLSX_DIR)
        if not xlsx_files:
            logger.warning("没有找到需要处理的Excel文件")
            return

        # 使用线程池处理文件
        process_files_with_executor(xlsx_files)

    except Exception as e:
        logger.error(f"程序执行过程中发生错误: {str(e)}")
    finally:
        # 确保日志系统正确关闭
        logging.shutdown()


if __name__ == "__main__":
    main()