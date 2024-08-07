#!/usr/bin/env python
# coding=utf-8

import os
import xlrd
import logging
import json
from concurrent.futures import ThreadPoolExecutor, as_completed

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Path to your Excel file
excel_file_path = 'xlsx/operator/Operator.xlsx'

# Output directory for Proto files
output_dir = 'generated/proto/operator'

# Temporary file for storing existing ID mappings
temp_file_path = 'generated/proto/operatortemp_id_mapping.json'

# Create the output directory if it doesn't exist
os.makedirs(output_dir, exist_ok=True)

def read_temp_id_mapping():
    """Reads existing ID mappings from temp file."""
    if os.path.exists(temp_file_path):
        with open(temp_file_path, 'r') as f:
            return json.load(f)
    else:
        return {}

def write_temp_id_mapping(mapping):
    """Writes ID mappings to temp file."""
    with open(temp_file_path, 'w') as f:
        json.dump(mapping, f, indent=2)

def read_excel_data(file_path):
    """Read data from the provided Excel file path."""
    try:
        workbook = xlrd.open_workbook(file_path)
        sheet = workbook.sheet_by_index(0)
        num_rows = sheet.nrows

        groups = {}
        current_group = None
        global_row_id = 0  # Start global_row_id at 0 for open enums

        for row_idx in range(7, num_rows):
            row_cells = sheet.row(row_idx)

            if row_cells[0].value.startswith('//'):
                group_name = row_cells[0].value.strip('/').strip()

                if current_group:
                    groups[current_group] = current_group_data
                    current_group = None

                current_group = group_name
                current_group_data = []
            else:
                if current_group:
                    current_group_data.append((row_cells[0].value, global_row_id))
                    global_row_id += 1

        if current_group:
            groups[current_group] = current_group_data

        workbook.release_resources()
        del workbook

        return groups

    except Exception as e:
        logging.error(f"Error reading Excel file: {str(e)}")
        return {}

def generate_proto_file(group_name, group_data, existing_id_mapping):
    """Generate a Proto file for a given group."""
    try:
        proto_content = f"// Proto file for {group_name}\n"
        proto_content += f"syntax = \"proto3\";\n\n"
        proto_content += f"enum {group_name} {{\n"

        if group_name == "common_error":
            proto_content += f"  option allow_alias = true;\n\n"

        proto_content += f"  k{group_name.capitalize()}OK = 0;\n"

        for enum_name, enum_id in group_data:
            if enum_name in existing_id_mapping:
                enum_id = existing_id_mapping[enum_name]
            else:
                enum_id = max(existing_id_mapping.values(), default=-1) + 1
                existing_id_mapping[enum_name] = enum_id

            enum_name_with_k = f"k{enum_name.strip()}"
            proto_content += f"  {enum_name_with_k} = {enum_id};\n"

        proto_content += "};\n"

        proto_file_path = os.path.join(output_dir, f"{group_name.lower()}_tip.proto")
        with open(proto_file_path, 'w', encoding='utf-8') as proto_file:
            proto_file.write(proto_content)

        logging.info(f"Proto enums file generated: {proto_file_path}")

        # Write updated ID mapping to temp file
        write_temp_id_mapping(existing_id_mapping)

    except Exception as e:
        logging.error(f"Error generating Proto file for group {group_name}: {str(e)}")

def generate_proto_files(groups):
    """Generate Proto files for all groups using ThreadPoolExecutor."""
    existing_id_mapping = read_temp_id_mapping()

    with ThreadPoolExecutor() as executor:
        futures = [executor.submit(generate_proto_file, group_name, group_data, existing_id_mapping)
                   for group_name, group_data in groups.items()]

        for future in as_completed(futures):
            try:
                future.result()
            except Exception as e:
                logging.error(f"Error occurred: {str(e)}")

def main():
    groups = read_excel_data(excel_file_path)
    if groups:
        generate_proto_files(groups)
        logging.info("Proto generation completed.")

if __name__ == "__main__":
    main()
