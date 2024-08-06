import xlrd

# Path to your Excel file
excel_file_path = 'xlsx/tip/Tip.xlsx'

# Open the workbook
workbook = xlrd.open_workbook(excel_file_path)

# Select the sheet you want to read (assuming the first sheet for example)
sheet = workbook.sheet_by_index(0)

# Number of rows in the sheet
num_rows = sheet.nrows

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
            groups.append(current_group)
            current_group = []
    else:
        # Add row to current group
        current_group.append([cell.value for cell in row_cells])

# Add the last group if not empty
if current_group:
    groups.append(current_group)

# Close the workbook
workbook.release_resources()
del workbook

# Print groups
for group_idx, group in enumerate(groups, start=1):
    print(f"Group {group_idx}:")
    for row in group:
        print(row)
    print()
