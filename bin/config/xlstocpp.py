#!/usr/bin/env python
# coding=utf-8
import gencommon
import xlrd
import xlwt
import json
import os.path
import md5tool
from os import listdir
from os.path import isfile, join
from multiprocessing import cpu_count

keyrowidx = 4
cppdir = "cpp/"
xlsdir = "xlsx/"


def getColNames(sheet):
    rowSize = sheet.row_len(keyrowidx)
    colValues = sheet.row_values(keyrowidx, 0, rowSize)
    columnNames = []
    for value in colValues:
        columnNames.append(value)
    return columnNames


def getRowData(row, columnNames):
    rowData = {}
    counter = 0
    for cell in row:
        if columnNames[counter].strip() != "key":
            counter += 1
            continue
        rowData[str(cell.value)] = cell.value
        counter += 1
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
    sheetnamelower = sheetname.lower()

    s = "#pragma once\n"
    s += "#include <memory>\n"
    s += "#include <unordered_map>\n"
    s += '#include "%s_config.pb.h" \n' % (sheetnamelower)
    s += 'class %sConfigurationTable\n{\npublic:\n' % (sheetname)
    s += '  using row_type = const %s_row*;\n' % (sheetnamelower)
    s += '  using kv_type = std::unordered_map<uint32_t, row_type>;\n'
    s += '  static %sConfigurationTable& GetSingleton(){static %sConfigurationTable singleton; return singleton;}\n' % (
        sheetname, sheetname)
    s += '  const %s_table& All()const{return data_;}\n' % (sheetnamelower)
    s += '  row_type GetTable(uint32_t keyid);\n'
    counter = 0
    pd = ''
    for d in datastring:
        for v in d.values():
            s += '  row_type key_%s(uint32_t keyid)const;\n' % (v)
            pd += ' kv_type key_data_%s_;\n' % (counter)
            counter += 1
    s += '  void Load();\n'
    s += 'private:\n %s_table data_;\n' % (sheetnamelower)
    s += ' kv_type key_data_;\n'
    s += pd
    s += '};\n'
    s += ' const %sConfigurationTable::row_type Get%sTable(uint32_t keyid);\n' % (
        sheetname, sheetname)
    s += ' const %s_table& Get%sAllTable();\n' % (
        sheetnamelower, sheetname)
    return s;


def getcpp(datastring, sheetname):
    sheetnamelower = sheetname.lower()
    s = '#include "google/protobuf/util/json_util.h"\n'
    s += '#include "src/util/file2string.h"\n'
    s += '#include "%s_config.h"\n\n' % (sheetname)
    s += 'void %sConfigurationTable::Load()\n{\n data_.Clear();\n' % (sheetname)
    s += ' const auto contents = File2String("config/json/%s.json");\n' % sheetname
    s += ' if (const auto result = google::protobuf::util::JsonStringToMessage(contents.data(), &data_);\n'
    s += '    !result.ok())\n {\n  std::cout << "%s " << result.message().data() << std::endl;\n }\n' % (sheetname)
    s += ' for (int32_t i = 0; i < data_.data_size(); ++i)\n {\n'
    counter = 0
    for d in datastring:
        s += '   key_data_.clear();\n'
        for v in d.values():
            s += '   key_data_%s_.clear();\n' % counter
            counter += 1
        s += ' }\n'

    s += ' for (int32_t i = 0; i < data_.data_size(); ++i)\n {\n'
    s += '  const auto& row_data = data_.data(i);\n'
    counter = 0
    for d in datastring:
        s += '   key_data_.emplace(row_data.id(), &row_data);\n'
        for v in d.values():
            s += '   key_data_%s_.emplace(row_data.%s(), &row_data);\n' % (counter, v)
            counter += 1
        s += ' }\n'
    s += '}\n\n'

    s += 'const %s_row* %sConfigurationTable::GetTable(uint32_t keyid)\n{\n' % (
        sheetnamelower, sheetname)
    s += '  const auto it = key_data_.find(keyid);\n  return it == key_data_.end() ? nullptr : it->second;\n}\n'

    counter = 0
    for d in datastring:
        for v in d.values():
            s += 'const %s_row* %sConfigurationTable::key_%s(uint32_t keyid)const\n{\n' % (
                sheetnamelower, sheetname, v)
            s += '  const auto it = key_data_%s_.find(keyid);\n  return it == key_data_%s_.end() ? nullptr : it->second;\n}\n' % (
                counter, counter)
            counter += 1
    s += '\nconst %sConfigurationTable::row_type Get%sTable(uint32_t keyid) { return %sConfigurationTable::GetSingleton().GetTable(keyid); }\n' % (
        sheetname, sheetname, sheetname)
    s += '\nconst %s_table& Get%sAllTable() { return %sConfigurationTable::GetSingleton().All(); }\n' % (
        sheetnamelower, sheetname, sheetname)
    return s


def getallconfig():
    sheetnames = []
    dirfiles = listdir(xlsdir)
    files = sorted(dirfiles, key=lambda file: os.path.getsize(xlsdir + file), reverse=True)
    for filename in files:
        filename = xlsdir + filename
        if filename.endswith('.xlsx') or filename.endswith('.xls'):
            workbook = xlrd.open_workbook(filename)
            workbookdata = getWorkBookData(workbook)
            for sheetname in workbookdata:
                sheetnames.append(sheetname)

    s = '#pragma once\n'
    s += 'void LoadAllConfig();\n'
    s += 'void LoadAllConfigAsyncWhenServerLaunch();\n'
    scpp = '#include "all_config.h"\n\n'
    scpp = '#include <thread>\n'
    scpp += '#include "muduo/base/CountDownLatch.h"\n\n'

    for item in sheetnames:
        scpp += '#include "%s_config.h"\n' % (item)
    scpp += 'void LoadAllConfig()\n{\n'
    for item in sheetnames:
        scpp += '%sConfigurationTable::GetSingleton().Load();\n' % (item)
    scpp += '}\n'
    scpp += '\n'
    cpucount = cpu_count()
    scpp += 'void LoadAllConfigAsyncWhenServerLaunch()\n{\n'

    cpustr = []
    for i in range(cpucount):
        cpustr.append([])
    count = 0
    realthreadcount = 0
    for item in sheetnames:
        loadstr = '%sConfigurationTable::GetSingleton().Load();\n' % (item)
        if count >= cpucount:
            count = 0
        cpustr[count].append(loadstr)
        count += 1
        if realthreadcount < count:
            realthreadcount = count

    scpp += 'static muduo::CountDownLatch latch_(' + str(realthreadcount) + ');\n'

    for group in cpustr:
        if len(group) <= 0:
            continue
        scpp += '\n///begin\n{\n std::thread t([](){\n\n'
        for blockstr in group:
            scpp += blockstr
        scpp += '\nlatch_.countDown();\n});\nt.detach();\n}\n///end\n'
    scpp += 'latch_.wait();\n'
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
            for sheetname in workbookdata:
                sheetnamelower = sheetname.lower()
                hwfilename = sheetnamelower + "_config.h"
                cwfilename = sheetnamelower + "_config.cpp"
                s = getcpph(workbookdata[sheetname], sheetname)
                gencommon.mywrite(s, cppdir + hwfilename)
                s = getcpp(workbookdata[sheetname], sheetname)
                gencommon.mywrite(s, cppdir + cwfilename)
    hs, cpps = getallconfig()
    gencommon.mywrite(hs, cppdir + "all_config.h")
    gencommon.mywrite(cpps, cppdir + "all_config.cpp")


main()
