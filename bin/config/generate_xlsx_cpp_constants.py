# !/usr/bin/env python
# coding=utf-8

import os
import logging
import openpyxl
from typing import Optional
from jinja2 import Environment, FileSystemLoader

import utils
import generate_common
from common import constants

# 设置日志
logging.basicConfig(
    level=logging.DEBUG,  # 提高日志级别便于调试
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class ExcelToCppConverter:
    def __init__(self, excel_file: str):
        self.excel_file = excel_file
        try:
            self.workbook = openpyxl.load_workbook(excel_file)
            self.sheet = self.workbook.sheetnames[0]
            self.worksheet = self.workbook[self.sheet]
            self.constants_name_index = self._find_constants_name_index()
        except Exception as e:
            logger.error(f"加载 Excel 文件失败: {excel_file}, 错误: {e}")
            raise

    def _find_constants_name_index(self) -> Optional[int]:
        try:
            if not self.worksheet or self.worksheet.max_row < 1:
                logger.warning(f"工作表 '{self.sheet}' 为空或没有数据")
                return None

            first_row = self.worksheet[1]
            headers = [str(cell.value).strip() if cell.value else '' for cell in first_row]
            return headers.index('constants_name') if 'constants_name' in headers else None
        except Exception as e:
            logger.error(f"查找 constants_name 列时发生错误: {e}")
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
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(cpp_code)

    def close(self):
        if hasattr(self, 'workbook'):
            self.workbook.close()


def process_file(file_path: str):
    logger.info(f"开始处理文件: {file_path}")

    if not os.path.isfile(file_path):
        logger.error(f"文件不存在: {file_path}")
        return

    try:
        converter = ExcelToCppConverter(file_path)
        if converter.should_process():
            cpp_code = converter.generate_cpp_constants()
            converter.save_cpp_constants_to_file(cpp_code)
            logger.info(f"处理完成: {file_path}")
        else:
            logger.info(f"跳过文件 (无 constants_name 列): {file_path}")
    except Exception as e:
        logger.error(f"处理文件出错: {file_path}，错误: {e}")
    finally:
        if 'converter' in locals():
            converter.close()


def main():
    try:
        os.makedirs(constants.GENERATOR_CONSTANTS_NAME_DIR, exist_ok=True)

        xlsx_files = utils.get_xlsx_files(constants.XLSX_DIR)
        if not xlsx_files:
            logger.warning("没有找到需要处理的Excel文件")
            return

        for file in xlsx_files:
            process_file(file)

    except Exception as e:
        logger.error(f"程序执行过程中发生错误: {e}")
    finally:
        logging.shutdown()


if __name__ == "__main__":
    main()
