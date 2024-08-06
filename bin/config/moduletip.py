import xlrd

# Path to your Excel file
excel_file_path = 'xlsx/tip/Tip.xlsx'

# Open the workbook
workbook = xlrd.open_workbook(excel_file_path)

# Select the sheet you want to read (assuming the first sheet for example)
sheet = workbook.sheet_by_index(0)

# Number of rows in the sheet
num_rows = sheet.nrows

# Initialize variables for global unique IDs
global_group_id = 1
global_row_id = 1

# Initialize list to store groups
groups = []
current_group = []

# Starting from row 8 (index 7), read the data
for row_idx in range(8, num_rows):
    # Read each cell in the row
    row_cells = sheet.row(row_idx)

    # Check if the row is empty (assuming all cells are empty)
    if all(cell.value == '' for cell in row_cells):
        # Add current group to groups list if not empty
        if current_group:
            groups.append((global_group_id, current_group))
            current_group = []
            global_group_id += 1
    else:
        # Add row to current group with unique row ID
        current_group.append((global_row_id, [cell.value for cell in row_cells]))
        global_row_id += 1

# Add the last group if not empty
if current_group:
    groups.append((global_group_id, current_group))

# Close the workbook
workbook.release_resources()
del workbook

# Print groups with global unique IDs
for group_id, group in groups:
    print(f"Group {group_id}:")
    for row_id, row in group:
        print(f"Row {row_id}: {row}")
    print()
