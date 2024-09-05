#!/usr/bin/env python
# coding=utf-8

import os
import logging
from concurrent.futures import ThreadPoolExecutor, as_completed
from openpyxl import load_workbook  # Use openpyxl for .xlsx files

# Configure logging
logging.basicConfig(level=logging.WARNING, format='%(asctime)s - %(levelname)s - %(message)s')

# Path to your Excel file
excel_file_path = 'xlsx/tip/Tip.xlsx'

# Output directory for Proto files
output_dir = 'generated/proto/tip'

# Create the output directory if it doesn't exist
os.makedirs(output_dir, exist_ok=True)

def read_excel_data(file_path):
    """Read data from the first sheet of the provided Excel file path."""
    try:
        workbook = load_workbook(file_path, read_only=True)
        sheet = workbook.active  # Get the first (active) sheet
        num_rows = sheet.max_row

        groups = {}
        current_group = None
        current_group_data = []
        global_row_id = 0  # Start global_row_id at 0 for open enums

        for row_idx in range(18, num_rows + 1):  # Adjust for zero-based index
            row_cells = sheet[row_idx]

            # Check if the cell in the first column starts with '//'
            if row_cells[0].value and row_cells[0].value.startswith('//'):
                group_name = row_cells[0].value.strip('/').strip()

                if current_group:
                    # Save the previous group's data
                    groups[current_group] = current_group_data

                current_group = group_name
                current_group_data = []
            else:
                if current_group:
                    enum_name = row_cells[0].value
                    if enum_name:
                        current_group_data.append((enum_name.strip(), global_row_id))
                        global_row_id += 1

        if current_group:
            # Save the last group's data
            groups[current_group] = current_group_data

        return groups

    except Exception as e:
        logging.error(f"Error reading Excel file: {str(e)}")
        return {}

def generate_proto_file(group_name, group_data):
    """Generate a Proto file for a given group."""
    try:
        proto_content = f"// Proto file for {group_name}\n"
        proto_content += 'syntax = "proto3";\n\n'
        proto_content += 'option go_package = "pb/game";\n\n'
        proto_content += f"enum {group_name} {{\n"

        if group_name == "common_error":
            proto_content += '  option allow_alias = true;\n\n'

        proto_content += '  k{}OK = 0;\n'.format(group_name.capitalize())

        for enum_name, enum_id in group_data:
            enum_name_with_k = f"k{enum_name}"
            proto_content += f"  {enum_name_with_k} = {enum_id};\n"

        proto_content += '};\n'

        proto_file_path = os.path.join(output_dir, f"{group_name.lower()}_tip.proto")
        with open(proto_file_path, 'w', encoding='utf-8') as proto_file:
            proto_file.write(proto_content)

        logging.info(f"Proto enums file generated: {proto_file_path}")

    except Exception as e:
        logging.error(f"Error generating Proto file for group {group_name}: {str(e)}")

def generate_proto_files(groups):
    """Generate Proto files for all groups using ThreadPoolExecutor."""
    with ThreadPoolExecutor() as executor:
        futures = [executor.submit(generate_proto_file, group_name, group_data) for group_name, group_data in groups.items()]

        for future in as_completed(futures):
            try:
                future.result()
            except Exception as e:
                logging.error(f"Error occurred: {str(e)}")

def main():
    """Main function to read Excel data and generate Proto files."""
    groups = read_excel_data(excel_file_path)
    if groups:
        generate_proto_files(groups)
        logging.info("Proto generation completed.")

if __name__ == "__main__":
    main()
