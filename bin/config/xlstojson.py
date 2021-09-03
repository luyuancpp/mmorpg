#!/usr/bin/env python
# coding=utf-8
import xlrd
import xlwt
import json
import md5tool
import os.path
from os import listdir
from os.path import isfile, join

beginrowidx = 6
jsondir = "json/"
xlsdir = "xlsx/"
gen_type="server"

def getColNames(sheet):
        rowSize = sheet.row_len(0)
        colValues = sheet.row_values(0, 0, rowSize )
        columnNames = []
        scdIndex = 0
        for value in colValues:
                v = sheet.cell_value(3,scdIndex)
                if v == "design":
                        columnNames.append("")
                elif v != "common" and gen_type != v:
                        columnNames.append("")
                else:
                        columnNames.append(value)
                scdIndex += 1
        return columnNames

def getRowData(row, columnNames):
        rowData = {}
        counter = 0
        for cell in row:
                if columnNames[counter].strip() == "":
                        continue
                if cell.ctype == 2 and cell.value % 1 == 0.0:
                        cell.value = int(cell.value)
                rowData[columnNames[counter]] = cell.value
                counter +=1
        return rowData

def getSheetData(sheet, columnNames):
        nRows = sheet.nrows
        sheetData = []
        counter = 1
        for idx in range(1, nRows):
                if idx >= beginrowidx:
                        row = sheet.row(idx)
                        rowData = getRowData(row, columnNames)
                        sheetData.append(rowData)
        return sheetData

def getWorkBookData(workbook):
        nsheets = workbook.nsheets
        workbookdata = {}
        for idx in range(0, nsheets):
                worksheet = workbook.sheet_by_index(idx)
                if worksheet.cell_value(0, 0) != "id":
                        print("%s firs col num must be id "%(worksheet.name))
                columnNames = getColNames(worksheet)
                sheetdata = getSheetData(worksheet, columnNames)
                workbookdata[worksheet.name] = sheetdata
        return workbookdata

def main():
        if not os.path.exists(jsondir):
                os.makedirs(jsondir) 
        
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        filenamemd5 = filename + '.md5'
                        if not os.path.exists(filenamemd5):
                                md5tool.generate_md5_file_for(filename, filenamemd5)
                        error = md5tool.check_against_md5_file(filename, filename + '.md5')
                        if error == None:
                                continue
                        workbook = xlrd.open_workbook(filename)
                        workbookdata = getWorkBookData(workbook)
                        for sheetname in workbookdata :
                                output = open(jsondir + sheetname + ".json", "w", encoding="utf-8")
                                datastring = '{"data":' + json.dumps(workbookdata[sheetname] , sort_keys=True, indent=4,  separators=(',', ": ")) + '}'
                                datastring = datastring.replace('"[','[');
                                datastring = datastring.replace("]\"","]");
                                output.write(datastring)
                                output.close()
                       
main()
