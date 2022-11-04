#!/usr/bin/env python
# coding=utf-8
import buildcommon
import xlrd
import xlwt
import json
import md5tool
import os.path
from os import listdir
from os.path import isfile, join

beginrowidx = buildcommon.beginrowidx
cppdir = "cpp/"
xlsdir = "xlsx/"
genfilelist = ["global_variable"]

#todo 自己选择要生成id 的列
def genIdStr(sheet):
        nRows = sheet.nrows
        name = sheet.name
        filestr = "#pragma once\n"
        filestr += "enum e_%s_configid : uint32_t\n{\n" % (name)
        counter = 1
        for idx in range(1, nRows):
                if idx >= beginrowidx:
                        row = sheet.row(idx)
                        rowData = sheet.cell_value(idx,0)
                        filestr += '%s_config_id_h_%s,\n' % (name, int(rowData))
        filestr += "};\n"
        return filestr

def genIdCpp(workbook):
        nsheets = workbook.nsheets
        workbookdata = {}
        for idx in range(0, nsheets):
                worksheet = workbook.sheet_by_index(idx)
                if not worksheet.name in genfilelist:
                        continue
                rowData = genIdStr(worksheet)
                workbookdata[worksheet.name] = rowData
        return workbookdata

def main():
        if not os.path.exists(cppdir):
                os.makedirs(cppdir) 
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        workbook = xlrd.open_workbook(filename)
                        workbookdata = genIdCpp(workbook)
                        for sheetname in workbookdata :
                                buildcommon.mywrite(workbookdata[sheetname], cppdir + sheetname + "_config_id.h")
                       
main()
