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
        for filename in listdir(xlsdir):
                filename = xlsdir + filename
                if filename.endswith('.xlsx') or filename.endswith('.xls'):
                        os.remove(filename + ".md5")
                     
main()
