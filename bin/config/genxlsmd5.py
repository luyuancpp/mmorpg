#!/usr/bin/env python
# coding=utf-8
import xlrd
import xlwt
import json
import md5tool
import os.path
from os import listdir
from os.path import isfile, join

xlsdir = "xlsx/"

def main():
        if not os.path.exists(xlsdir):
                os.makedirs(xlsdir) 
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        filenamemd5 = filename + '.md5'
                        md5tool.generate_md5_file_for(filename, filenamemd5)                 
main()
