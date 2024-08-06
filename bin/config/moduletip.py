import os
import xlrd

# Path to your Excel file
excel_file_path = 'xlsx/tip/Tip.xlsx'

# Output directory for Proto files
output_dir = 'tip/enum/'

# Create the output directory if it doesn't exist
os.makedirs(output_dir, exist_ok=True)

# Open the workbook
workbook = xlrd.open_workbook(excel_file_path)

# Select the sheet you want to read (assuming the first sheet for example)
sheet = workbook.sheet_by_index(0)

# Number of rows in the sheet
num_rows = sheet.nrows

# Initialize variables for global unique IDs
global_row_id = 1

# Initialize list to store groups
groups = {}
current_group = None

# Starting from row 8 (index 7), read the data
for row_idx in range(8, num_rows):
    # Read each cell in the row
    row_cells = sheet.row(row_idx)

    # Check if the row starts with '//'
    if row_cells[0].value.startswith('//'):
        # Extract group name from the row
        group_name = row_cells[0].value.strip('/').strip()

        # If current group has started, add it to groups dictionary
        if current_group:
            groups[current_group] = current_group_data
            current_group = None

        # Initialize current group and its data
        current_group = group_name
        current_group_data = []
    else:
        # If group has started, add row to current group data with unique row ID
        if current_group:
            current_group_data.append((row_cells[0].value, global_row_id))
            global_row_id += 1

# Add the last group if not empty
if current_group:
    groups[current_group] = current_group_data

# Close the workbook
workbook.release_resources()
del workbook

# Generate Proto enum files for each group
for group_name, group_data in groups.items():
    proto_content = f"// Proto file for {group_name}\n"
    proto_content += f"syntax = \"proto3\";\n\n"
    proto_content += f"enum {group_name} {{\n"
    for enum_name, enum_id in group_data:
        proto_content += f"  {enum_name} = {enum_id};\n"
    proto_content += "}\n"

    # Write Proto content to file in the output directory
    proto_file_path = os.path.join(output_dir, f"{group_name.lower()}.proto")
    with open(proto_file_path, 'w') as proto_file:
        proto_file.write(proto_content)

    print(f"Proto enums file generated: {proto_file_path}")
