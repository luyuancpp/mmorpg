#!/usr/bin/env python
# coding=utf-8

import os
import xlrd
import json
import md5tool
import gencommon  # Assuming gencommon provides mywrite function
import logging
from os import listdir
from os.path import isfile, join

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

begin_row_idx = 7
json_dir = "json/"
xls_dir = "xlsx/"
gen_type = "server"


def get_column_names(sheet):
    """
    Get column names from the sheet based on specified conditions.

    Args:
    - sheet: xlrd Sheet object representing Excel sheet data.

    Returns:
    - list: List of column names.
    """
    row_size = sheet.row_len(0)
    col_values = sheet.row_values(0, 0, row_size)
    column_names = []
    for idx, value in enumerate(col_values):
        second_row_value = sheet.cell_value(3, idx)
        if second_row_value == "design":
            column_names.append("")
        elif second_row_value != "common" and gen_type != second_row_value:
            column_names.append("")
        else:
            column_names.append(value)
    return column_names


def get_row_data(row, column_names):
    """
    Get row data as dictionary based on column names.

    Args:
    - row: xlrd Row object representing Excel row data.
    - column_names: List of column names.

    Returns:
    - dict: Dictionary containing row data.
    """
    row_data = {}
    for counter, cell in enumerate(row):
        if column_names[counter].strip() != "":
            if cell.ctype == 2 and cell.value % 1 == 0.0:
                cell.value = int(cell.value)
            row_data[column_names[counter]] = cell.value
    return row_data


def get_sheet_data(sheet, column_names):
    """
    Get sheet data as list of dictionaries.

    Args:
    - sheet: xlrd Sheet object representing Excel sheet data.
    - column_names: List of column names.

    Returns:
    - list: List of dictionaries containing sheet data.
    """
    n_rows = sheet.nrows
    sheet_data = []
    for idx in range(1, n_rows):
        if idx >= begin_row_idx:
            row = sheet.row(idx)
            row_data = get_row_data(row, column_names)
            sheet_data.append(row_data)
    return sheet_data


def get_workbook_data(workbook):
    """
    Get workbook data as dictionary of sheet names and their respective data.

    Args:
    - workbook: xlrd Workbook object representing Excel workbook.

    Returns:
    - dict: Dictionary where keys are sheet names and values are lists of dictionaries containing sheet data.
    """
    workbook_data = {}
    for sheet_name in workbook.sheet_names():
        worksheet = workbook.sheet_by_name(sheet_name)
        if worksheet.cell_value(0, 0) != "id":
            logger.error(f"{sheet_name} first column must be 'id'")
            continue
        column_names = get_column_names(worksheet)
        sheet_data = get_sheet_data(worksheet, column_names)
        workbook_data[sheet_name] = sheet_data
    return workbook_data


def main():
    # Create output directory if it doesn't exist
    if not os.path.exists(json_dir):
        os.makedirs(json_dir)

    # Process each Excel file in xls_dir
    for filename in os.listdir(xls_dir):
        full_path = os.path.join(xls_dir, filename)
        if isfile(full_path) and (filename.endswith('.xlsx') or filename.endswith('.xls')):
            # Generate MD5 file if it doesn't exist
            md5_file_path = full_path + '.md5'
            if not os.path.exists(md5_file_path):
                md5tool.generate_md5_file_for(full_path, md5_file_path)
                error = md5tool.check_against_md5_file(full_path, md5_file_path)
                if error is None:
                    continue

            # Open workbook and process data
            workbook = xlrd.open_workbook(full_path)
            workbook_data = get_workbook_data(workbook)

            # Write JSON files for each sheet
            for sheet_name, data in workbook_data.items():
                json_data = {"data": data}
                json_string = json.dumps(json_data, sort_keys=True, indent=4, separators=(',', ': '))
                json_string = json_string.replace('"[', '[').replace(']"', ']')  # Remove unnecessary quotes around lists
                json_file_path = os.path.join(json_dir, f"{sheet_name}.json")
                gencommon.mywrite(json_string, json_file_path)
                logger.info(f"Generated JSON file: {json_file_path}")

if __name__ == "__main__":
    main()
