#!/usr/bin/env python
# coding=utf-8
import xlrd
import xlwt
import json
import os.path
from os import listdir
from os.path import isfile, join

keyrowidx = 4
protodir = "cpp/"
xlsdir = "xlsx/"

def getColNames(sheet):
        rowSize = sheet.row_len(keyrowidx)
        colValues = sheet.row_values(keyrowidx, 0, rowSize )
        columnNames = []
        for value in colValues:
                columnNames.append(value)
        return columnNames

def getRowData(row, columnNames):
        rowData = {}
        counter = 0
        for cell in row:
                if columnNames[counter].strip() != "key":
                        counter +=1
                        continue
                rowData[columnNames[counter]] = cell.value
                counter +=1
        return rowData

def getSheetData(sheet, columnNames):
        nRows = sheet.nrows
        sheetData = []
        row = sheet.row(0)
        rowData = getRowData(row, columnNames)
        sheetData.append(rowData)

        return sheetData

def getWorkBookData(workbook):
        nsheets = workbook.nsheets
        workbookdata = {}
        for idx in range(0, nsheets):
                worksheet = workbook.sheet_by_index(idx)
                columnNames = getColNames(worksheet)
                sheetdata = getSheetData(worksheet, columnNames)
                workbookdata[worksheet.name] = sheetdata

        return workbookdata

def getcpph(datastring, sheetname):
        s = "#include <memory>\n"
        s += "#include <umordered_map>\n\n"
        s += '#include "%s.pb.h; \n\n' % (sheetname)
        s += 'class %sconfig\n{\npublic:\n' % (sheetname)
        s += '  void load();\n'
        s += '  using rowptr = std::shared_ptr< %s_row>;\n'% (sheetname)
        s += '  const %s_row* key_id(uint32 keyid);\n' % (sheetname)
        counter = 0
        for d in datastring:
                for v in d.values():
                        s += "  const %s_row* key_%s(uint32 keyid); " % (sheetname,v) + ";\n" 
                counter += 1
        s += 'private:\n std::umordered_map<uint32, rowptr> data_;\n'
        s += '};\n'
        return s;

def getcpp(datastring, sheetname):
        s = "#include <umordered_map>"
        s += '#include "%s.pb.h; \n' % (sheetname)
        s += 'class %sconfig\n{\npublic:\n' % (sheetname)
        s += '  const %s_row* key_id(uint32 keyid);\n' % (sheetname)
        counter = 0
        for d in datastring:
                for v in d.values():
                        s += "  const %s_row* key_%s(uint32 keyid); " % (sheetname,v) + ";\n" 
                counter += 1
        s += 'private:\n std::umordered_map<uint32, %s_row> data_;\n};\n' % (sheetname)
        return s;

def main():
        if not os.path.exists(protodir):
                os.makedirs(protodir) 
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        workbook = xlrd.open_workbook(filename)
                        workbookdata = getWorkBookData(workbook)
                        for sheetname in workbookdata :
                                output = open(protodir + sheetname + "_config.h", "w", encoding="utf-8")
                                s =getcpph(workbookdata[sheetname], sheetname)
                                output.write(s)
                                output.close()
                       
main()
