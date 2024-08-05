import xlrd

# Path to your Excel file
excel_file_path = 'xlsx/tip/Tip.xlsx'

# Open the workbook
workbook = xlrd.open_workbook(excel_file_path)

# Select the sheet you want to read (assuming the first sheet for example)
sheet = workbook.sheet_by_index(0)

# Number of rows in the sheet
num_rows = sheet.nrows

# Starting from row 7 (index 6), read the data
for row_idx in range(6, num_rows):
    row = sheet.row_values(row_idx)
    # Process each row as needed
    print(row)  # Example: printing the row data

# Close the workbook
workbook.release_resources()
del workbook
