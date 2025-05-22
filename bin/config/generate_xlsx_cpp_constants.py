#!/usr/bin/env python
# coding=utf-8

import os
import logging
import concurrent.futures
from os import listdir
from os.path import isfile, join
import openpyxl
import multiprocessing
from typing import List, Optional
import  utils
from jinja2 import Environment, FileSystemLoader
from pathlib import Path

import gen_common  # Assuming gen_common contains the necessary functions
from common import constants

# Setup Logging
logging.basicConfig(level=logging.WARNING, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class ExcelToCppConverter:
    def __init__(self, excel_file: str):
        self.excel_file = excel_file
        self.workbook = openpyxl.load_workbook(excel_file)
        self.sheet = self.workbook.sheetnames[0]
        self.worksheet = self.workbook[self.sheet]
        self.constants_name_index = self._find_constants_name_index()

    def _find_constants_name_index(self) -> Optional[int]:
        headers = [cell.value for cell in self.worksheet[1]]
        return headers.index('constants_name') if 'constants_name' in headers else None

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

        env = Environment(loader=FileSystemLoader(gen_common.TEMPLATE_DIR))
        template = env.get_template("constants.h.j2")
        return template.render(constants=constants_list)

    def _generate_constant_name(self, row: tuple, id_value: int) -> str:
        if self.constants_name_index is not None and row[self.constants_name_index]:
            return f'k{self.sheet}_{row[self.constants_name_index]}'
        return f'k{self.sheet}_{id_value}'

    def save_cpp_constants_to_file(self, cpp_code: str) -> None:
        output_path = os.path.join(constants.GENERATOR_CONSTANTS_NAME_DIR,
                                   f"{self.sheet.lower()}_table_id_constants.h")
        with open(output_path, 'w') as f:
            f.write(cpp_code)



def get_xlsx_files(directory: str) -> List[str]:
    """List all .xlsx files in the specified directory."""
    return [join(directory, filename) for filename in listdir(directory)
            if isfile(join(directory, filename)) and filename.endswith('.xlsx')]

def process_file(file_path: str):
    converter = ExcelToCppConverter(file_path)
    if converter.should_process():
        cpp_code = converter.generate_cpp_constants()
        converter.save_cpp_constants_to_file(cpp_code)
        logger.info(f"Processed: {file_path}")
    else:
        logger.info(f"Skipped (no constants_name): {file_path}")

def main():
    os.makedirs(constants.GENERATOR_CONSTANTS_NAME_DIR, exist_ok=True)

    try:
        xlsx_files = utils.get_xlsx_files(constants.XLSX_DIR)
    except Exception as e:
        logger.error(f"Failed to list files: {e}")
        return

    num_threads = min(multiprocessing.cpu_count(), len(xlsx_files))
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        futures = [executor.submit(process_file, f) for f in xlsx_files]
        for future in concurrent.futures.as_completed(futures):
            try:
                future.result()
            except Exception as e:
                logger.error(f"Error during processing: {e}")

if __name__ == "__main__":
    main()


if __name__ == "__main__":
    main()
