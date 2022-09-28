#!/usr/bin/env python
# coding=utf-8
import buildcommon
import xlrd
import xlwt
import json
import os.path
import md5tool
from os import listdir
from os.path import isfile, join

keyrowidx = 4
cppdir = "cpp/"
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
        s += '#include "game_config/%s_config.pb.h" \n' % (sheetname)
        s += 'class %s_config\n{\npublic:\n' % (sheetname)
        s += '  using row_type = const %s_row*;\n' % (sheetname)
        s += '  using kv_type = std::unordered_map<uint32_t, row_type>;\n'
        s += '  static %s_config& GetSingleton(){thread_local %s_config singleton; return singleton;}\n' % (sheetname,sheetname)
        s += '  const %s_table& all()const{return data_;}\n'% (sheetname)
        s += '  row_type get(uint32_t keyid);\n'
        counter = 0
        pd = ''
        for d in datastring:
                for v in d.values():
                        s += '  row_type key_%s(uint32_t keyid)const;\n' % (v) 
                        pd += ' kv_type key_data_%s_;\n'%(counter)
                        counter += 1
        s += '  void load();\n'
        s += 'private:\n %s_table data_;\n' % (sheetname)
        s += ' kv_type key_data_;\n'
        s += pd
        s += '};\n'
        s +=  ' const %s_config::row_type get_%s_conf(uint32_t keyid);\n' % (sheetname, sheetname)
        s += "#endif// %s_config_h_\n" % (sheetname)
        return s;

def getcpp(datastring, sheetname):
        s = '#include "google/protobuf/util/json_util.h"\n'
        s += '#include "src/util/file2string.h"\n'
        s += '#include "%s_config.h" \n' % (sheetname)
        s += 'using namespace common;\n' 
        s += 'using namespace std;\n'
        s += 'void %s_config::load()\n{\n data_.Clear();\n' % (sheetname)
        s += ' auto contents = File2String("config/json/%s.json");\n' % (sheetname)
        s += ' google::protobuf::StringPiece sp(contents.data(), contents.size());\n'
        s += ' auto result = google::protobuf::util::JsonStringToMessage(sp, &data_);\n'
        s += ' if (!result.ok()){cout << "%s " << result.message().data() << endl;}\n' % (sheetname)
        s += ' for (int32_t i = 0; i < data_.data_size(); ++i)\n {\n'
        counter = 0
        for d in datastring:
                s += '   key_data_.clear();\n'
                for v in d.values(): 
                        s += '   key_data_%s_.clear();\n' % (counter) 
                        counter += 1
                s += ' }\n'

        s += ' for (int32_t i = 0; i < data_.data_size(); ++i)\n {\n'
        s += '   auto& d = data_.data(i);\n'
        counter = 0
        for d in datastring:
                s += '   key_data_.emplace(d.id(), &d);\n'
                for v in d.values(): 
                        s += '   key_data_%s_.emplace(d.%s(), &d);\n' % (counter,v) 
                        counter += 1
                s += ' }\n'
        s += '}\n'
        
      
        s += ' const %s_row* %s_config::get(uint32_t keyid)\n{\n' % (sheetname,sheetname)
        s += '  auto it = key_data_.find(keyid);\n  return it == key_data_.end() ? nullptr : it->second;\n}\n'

        counter = 0
        for d in datastring:
                for v in d.values(): 
                        s += 'const %s_row* %s_config::key_%s(uint32_t keyid)const\n{\n' % (sheetname,sheetname,v)
                        s += '  auto it = key_data_%s_.find(keyid);\n  return it == key_data_%s_.end() ? nullptr : it->second;\n}\n'% (counter,counter) 
                        counter += 1
        s +=  ' const %s_config::row_type get_%s_conf(uint32_t keyid){ return %s_config::GetSingleton().get(keyid);}\n' % (sheetname,sheetname, sheetname)
        return s;

def getallconfig():
        sheetnames = []
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        workbook = xlrd.open_workbook(filename)
                        workbookdata = getWorkBookData(workbook)
                        for sheetname in workbookdata :
                                sheetnames.append(sheetname)
                        
        s =  '#pragma once\n'                      
        s += 'void LoadAllConfig();\n'
        s += 'void LoadAllConfigAsync();\n'
        scpp = '#include "all_config.h"\n'  
        for item in sheetnames :
                scpp += '#include "%s_config.h"\n' % (item)               
        scpp += 'void LoadAllConfig()\n{\n'
        for item in sheetnames :
                scpp += '%s_config::GetSingleton().load();\n' % (item)
                #print(item)
        scpp += '}\n'
        scpp += '\n'
        scpp += 'void LoadAllConfigAsync()\n{\n'
        for item in sheetnames :
                scpp += '%s_config::GetSingleton().load();\n' % (item)
                #print(item)
        scpp += '}\n'
        return s, scpp

def main():
        if not os.path.exists(cppdir):
                os.makedirs(cppdir) 
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        workbook = xlrd.open_workbook(filename)
                        workbookdata = getWorkBookData(workbook)
                        for sheetname in workbookdata :
                                hwfilename = sheetname + "_config.h"
                                cwfilename = sheetname + "_config.cpp"
                                s =getcpph(workbookdata[sheetname], sheetname)
                                buildcommon.mywrite(s, cppdir + hwfilename)
                                s =getcpp(workbookdata[sheetname], sheetname)
                                buildcommon.mywrite(s, cppdir + cwfilename)
        hs, cpps = getallconfig()
        buildcommon.mywrite(hs, cppdir + "all_config.h")
        buildcommon.mywrite(cpps, cppdir + "all_config.cpp")
                       
main()
