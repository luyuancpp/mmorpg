#!/usr/bin/env python
# coding=utf-8

import os
import openpyxl
import json
import md5tool
import logging
from os import listdir
from os.path import isfile, join
import concurrent.futures  # For parallel processing

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

begin_row_idx = 9
json_dir = "generated/json/"
xlsx_dir = "xlsx/"
gen_type = "server"


def get_column_names(sheet):
    """
    Get column names from the sheet based on specified conditions.

    Args:
    - sheet: openpyxl Worksheet object representing Excel sheet data.

    Returns:
    - list: List of column names.
    """
    column_names = []
    for col_idx, cell in enumerate(sheet[1]):
        second_row_value = sheet.cell(row=4, column=col_idx + 1).value
        if second_row_value == "design":
            column_names.append("")
        elif second_row_value != "common" and gen_type != second_row_value:
            column_names.append("")
        else:
            column_names.append(cell.value)
    return column_names


def get_row_data(sheet, row, column_names):
    """
    Get row data as dictionary based on column names and validate cell formats.

    Args:
    - sheet: openpyxl Worksheet object representing the Excel sheet.
    - row: openpyxl Row object representing Excel row data.
    - column_names: List of column names.

    Returns:
    - dict: Dictionary containing row data.
    """
    row_data = {}
    for counter, cell in enumerate(row):
        if counter >= len(column_names):
            logger.warning(f"Row {cell.row} in sheet '{sheet.title}' has more cells than column names.")
            break

        col_name = column_names[counter]

        if col_name == 'designer':
            continue

        if col_name and col_name.strip():
            cell_value = cell.value
            if isinstance(cell_value, float) and cell_value.is_integer():
                cell_value = int(cell_value)

            # Construct the cell reference in A1 notation
            cell_reference = f"{cell.column_letter}{cell.row}"

            if cell_value in (None, '') and cell.row > begin_row_idx:
                logger.error(f"Sheet '{sheet.title}', Cell {cell_reference} is empty or contains invalid data.")
            row_data[col_name] = cell_value

    return row_data


def get_sheet_data(sheet, column_names):
    """
    Get sheet data as list of dictionaries.

    Args:
    - sheet: openpyxl Worksheet object representing Excel sheet data.
    - column_names: List of column names.

    Returns:
    - list: List of dictionaries containing sheet data.
    """
    sheet_data = []
    for row in sheet.iter_rows(min_row=begin_row_idx + 1, values_only=False):  # Skip header row
        row_data = get_row_data(sheet, row, column_names)
        sheet_data.append(row_data)
    return sheet_data


def get_workbook_data(workbook):
    """
    Get workbook data as dictionary of sheet names and their respective data.

    Args:
    - workbook: openpyxl Workbook object representing Excel workbook.

    Returns:
    - dict: Dictionary where keys are sheet names and values are lists of dictionaries containing sheet data.
    """
    workbook_data = {}
    for sheet_name in workbook.sheetnames:
        sheet = workbook[sheet_name]
        if sheet.cell(row=1, column=1).value != "id":
            logger.error(f"{sheet_name} first column must be 'id'")
            continue

        column_names = get_column_names(sheet)
        sheet_data = get_sheet_data(sheet, column_names)
        workbook_data[sheet_name] = sheet_data

    return workbook_data


def save_json_with_custom_newlines(data, file_path):
    """
    Save data to a JSON file with compact and readable formatting while ensuring small file size.

    Args:
    - data: Data to be saved as JSON.
    - file_path: Path where the JSON file will be saved.
    """
    # Convert data to JSON string with compact yet readable formatting
    json_data = json.dumps({"data": data}, sort_keys=True, indent=1, separators=(',', ': '))

    json_data = json_data.replace('"[', '[').replace(']"', ']')  # Remove unnecessary quotes around lists
    # Ensure only LF (\n) for newlines
    json_data = json_data.replace('\r\n', '\n')  # Replace CRLF with LF
    json_data = json_data.replace('\r', '\n')    # Ensure any remaining CR is replaced with LF

    # Write JSON data to file
    with open(file_path, 'w', encoding='utf-8', newline='') as f:
        f.write(json_data)
        logger.info(f"Generated JSON file: {file_path}")


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
        workbook = openpyxl.load_workbook(file_path)
        workbook_data = get_workbook_data(workbook)

        # Write JSON files for each sheet
        for sheet_name, data in workbook_data.items():
            json_file_path = os.path.join(json_dir, f"{sheet_name}.json")
            save_json_with_custom_newlines(data, json_file_path)

    except Exception as e:
        logger.error(f"Failed to process file {file_path}: {e}")


def main():
    """
    Main function to process all Excel files in the specified directory.
    """
    # Create output directory if it doesn't exist
    os.makedirs(json_dir, exist_ok=True)

    # Gather all Excel files
    files = [join(xlsx_dir, filename) for filename in listdir(xlsx_dir) if isfile(join(xlsx_dir, filename)) and filename.endswith('.xlsx')]

    # Use a ThreadPoolExecutor to process files in parallel
    num_threads = os.cpu_count()  # Number of threads is equal to the number of CPU cores
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        # Process files concurrently
        executor.map(process_excel_file, files)


if __name__ == "__main__":
    main()
