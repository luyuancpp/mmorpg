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

# Flag to check if the current group has started
group_started = False

# Starting from row 8 (index 7), read the data
for row_idx in range(8, num_rows):
    # Read each cell in the row
    row_cells = sheet.row(row_idx)

    # Check if the row starts with '//'
    if row_cells[0].value.startswith('//'):
        # If current group has started, add it to groups list
        if current_group:
            groups.append((global_group_id, current_group))
            current_group = []
            global_group_id += 1
        # Set group started flag to True
        group_started = True
    else:
        # If group has started, add row to current group with unique row ID
        if group_started:
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
    print(f"// Group {group_id}:")
    for row_id, row in group:
        print(f"// Row {row_id}: {row}")
    print("//")

# Clear all records and reset variables for a new process
global_group_id = 1
global_row_id = 1
groups = []
current_group = []
