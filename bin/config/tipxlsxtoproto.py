import os
import xlrd
from concurrent.futures import ThreadPoolExecutor, as_completed

# Path to your Excel file
excel_file_path = 'xlsx/tip/Tip.xlsx'

# Output directory for Proto files
output_dir = 'proto/tip'

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

# Initialize dictionary to store groups
groups = {}
current_group = None

# Starting from row 8 (index 7), read the data
for row_idx in range(7, num_rows):
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


# Function to generate Proto file for a group
def generate_proto_file(group_name, group_data):
    proto_content = f"// Proto file for {group_name}\n"
    proto_content += f"syntax = \"proto3\";\n\n"
    proto_content += f"enum {group_name} {{\n"
    for idx, (enum_name, enum_id) in enumerate(group_data):
        # Add 'k' prefix to enum_name without extra spaces
        enum_name_with_k = f"k{enum_name.strip()}"

        # Ensure there are no spaces between 'k' and enum_name
        proto_content += f"  {enum_name_with_k} = {enum_id}"
        if idx < len(group_data) - 1:
            proto_content += ","  # Add comma if not the last entry
        proto_content += "\n"
    proto_content += "}\n"

    # Write Proto content to file in the output directory
    proto_file_path = os.path.join(output_dir, f"{group_name.lower()}.proto")
    with open(proto_file_path, 'w') as proto_file:
        proto_file.write(proto_content)

    print(f"Proto enums file generated: {proto_file_path}")


# Function to generate Proto files using ThreadPoolExecutor
def generate_proto_files():
    with ThreadPoolExecutor() as executor:
        futures = []
        for group_name, group_data in groups.items():
            futures.append(executor.submit(generate_proto_file, group_name, group_data))

        for future in as_completed(futures):
            try:
                future.result()
            except Exception as e:
                print(f"Error occurred: {str(e)}")


# Call the function to generate Proto files
generate_proto_files()

print("Proto generation completed.")
