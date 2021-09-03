#!/usr/bin/env python
# coding=utf-8
import hashlib
import optparse
import os
import os.path
import sys
import shutil
from os import system

def get_file_info_dictionaries(walkdir, protobufdir, cppdir):
    for (dirpath, dirnames, filenames) in os.walk(walkdir):
        for each_filename in filenames:
            is_proto_file = (each_filename[-6:].lower() == '.proto')
            if not is_proto_file:
                continue
            commond = 'protoc  -I=./ -I=%s --cpp_out=%s %s' % (protobufdir, cppdir, each_filename)
            system(commond)

get_file_info_dictionaries("./", "../../third_party/protobuf/src", "../pbc/")