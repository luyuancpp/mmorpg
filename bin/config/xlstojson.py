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

begin_row_idx = 9
json_dir = "generated/json/"
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
    Get row data as dictionary based on column names and validate cell formats.

    Args:
    - row: xlrd Row object representing Excel row data.
    - column_names: List of column names.

    Returns:
    - dict: Dictionary containing row data.
    """
    row_data = {}
    for counter, cell in enumerate(row):
        col_name = column_names[counter]
        if col_name.strip() != "":
            if cell.ctype == xlrd.XL_CELL_NUMBER and cell.value % 1 == 0.0:
                cell_value = int(cell.value)
            else:
                cell_value = cell.value

            # Check for empty or invalid data
            if cell_value in (None, ''):
                logger.warning(f"Cell at row {row.rownum + 1}, column '{col_name}' is empty or contains invalid data.")
            row_data[col_name] = cell_value
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
    for idx in range(begin_row_idx, n_rows):
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


def process_excel_file(file_path):
    """
    Process a single Excel file to generate JSON output.

    Args:
    - file_path: Path to the Excel file.
    """
    md5_file_path = file_path + '.md5'

    # Check MD5 value to ensure the file hasn't changed
    if not os.path.exists(md5_file_path):
        md5tool.generate_md5_file_for(file_path, md5_file_path)
    error = md5tool.check_against_md5_file(file_path, md5_file_path)
    if error is not None:
        logger.error(f"MD5 check failed for file: {file_path}")
        return

    # Open workbook and process data
    try:
        workbook = xlrd.open_workbook(file_path)
        workbook_data = get_workbook_data(workbook)

        # Write JSON files for each sheet
        for sheet_name, data in workbook_data.items():
            json_data = {"data": data}
            json_string = json.dumps(json_data, sort_keys=True, indent=4, separators=(',', ': '))
            json_string = json_string.replace('"[', '[').replace(']"', ']')  # Remove unnecessary quotes around lists
            json_file_path = os.path.join(json_dir, f"{sheet_name}.json")
            gencommon.mywrite(json_string, json_file_path)
            logger.info(f"Generated JSON file: {json_file_path}")

    except Exception as e:
        logger.error(f"Failed to process file {file_path}: {e}")


def main():
    """
    Main function to process all Excel files in the specified directory.
    """
    # Create output directory if it doesn't exist
    os.makedirs(json_dir, exist_ok=True)

    # Process each Excel file in xls_dir
    for filename in listdir(xls_dir):
        full_path = join(xls_dir, filename)
        if isfile(full_path) and (filename.endswith('.xlsx') or filename.endswith('.xls')):
            process_excel_file(full_path)


if __name__ == "__main__":
    main()
