#!/usr/bin/env python
# coding=utf-8
import xlrd
import xlwt
import json
import os.path
import md5tool
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
                rowData[str(cell.value)] = cell.value
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
        s = "#ifndef %s_config_h_\n"% (sheetname)
        s += "#define %s_config_h_\n"% (sheetname)
        s += "#include <memory>\n"
        s += "#include <unordered_map>\n"
        s += '#include "%s_config.pb.h" \n' % (sheetname)
        s += 'class %sconfig\n{\npublic:\n' % (sheetname)
        s += '  using rowptr = const %s_row*;\n' % (sheetname)
        s += '  using keydatastype = std::unordered_map<uint32_t, rowptr>;\n'
        s += '  static %sconfig& GetSingleton(){static %sconfig singleton; return singleton;}\n' % (sheetname,sheetname)
        s += '  const %s_table& all()const{return data_;}\n'% (sheetname)
        s += '  rowptr key_id(uint32_t keyid);\n'
        counter = 0
        pd = ''
        for d in datastring:
                for v in d.values():
                        s += '  rowptr key_%s(uint32_t keyid)const;\n' % (v) 
                        pd += ' keydatastype key_data_%s_;\n'%(counter)
                        counter += 1
        s += '  void load();\n'
        s += 'private:\n %s_table data_;\n' % (sheetname)
        s += ' keydatastype key_data_;\n'
        s += pd
        s += '};\n'
        s += "#endif// %s_config_h_\n"% (sheetname)
        return s;

def getcpp(datastring, sheetname):
        s = '#include "google/protobuf/util/json_util.h"\n'
        s += '#include "src/file2string/file2string.h"\n'
        s += '#include "%s_config.h" \n' % (sheetname)
        s += 'using namespace common;\n' 
        s += 'void %sconfig::load()\n{\n data_.Clear();\n' % (sheetname)
        s += ' auto contents = File2String("config/json/%s.json");\n' % (sheetname)
        s += ' google::protobuf::StringPiece sp(contents.data(), contents.size());\n'
        s += ' google::protobuf::util::JsonStringToMessage(sp, &data_);\n'
        
        s += ' for (int32_t i = 0; i < data_.data_size(); ++i)\n {\n'
        s += '   auto& d = data_.data(i);\n'
        counter = 0
        for d in datastring:
                for v in d.values(): 
                        s += '   key_data_%s_.clear();\n' % (counter) 
                        counter += 1
                s += ' }\n'

        s += ' for (int32_t i = 0; i < data_.data_size(); ++i)\n {\n'
        s += '   auto& d = data_.data(i);\n'
        counter = 0
        for d in datastring:
                for v in d.values(): 
                        s += '   key_data_%s_.emplace(d.%s(), &d);\n' % (counter,v) 
                        counter += 1
                s += ' }\n'
        s += '}\n'
        
      
        s += ' const %s_row* %sconfig::key_id(uint32_t keyid)\n{\n' % (sheetname,sheetname)
        s += '  auto it = key_data_.find(keyid);\n  return it == key_data_.end() ? nullptr : it->second;\n}\n'

        counter = 0
        for d in datastring:
                for v in d.values(): 
                        s += 'const %s_row* %sconfig::key_%s(uint32_t keyid)const\n{\n' % (sheetname,sheetname,v)
                        s += '  auto it = key_data_%s_.find(keyid);\n  return it == key_data_%s_.end() ? nullptr : it->second;\n}\n'% (counter,counter) 
                        counter += 1
        return s;

def main():
        if not os.path.exists(protodir):
                os.makedirs(protodir) 
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
                                outputh = open(protodir + sheetname + "_config.h", "w", encoding="utf-8")
                                s =getcpph(workbookdata[sheetname], sheetname)
                                outputh.write(s)
                                outputh.close()
                                outputcpp = open(protodir + sheetname + "_config.cpp", "w", encoding="utf-8")
                                s =getcpp(workbookdata[sheetname], sheetname)
                                outputcpp.write(s)
                                outputcpp.close()
                       
main()
