#!/usr/bin/env python
# coding=utf-8
import xlrd
import xlwt
import json
import md5tool
import os.path
from os import listdir
from os.path import isfile, join

beginrowidx = 7

def mywrite(str, filename):
        outputh = open(filename, "w", encoding="utf-8")
        outputh.write(str)
        outputh.close()
