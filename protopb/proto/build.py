#!/usr/bin/env python
# coding=utf-8
import hashlib
import optparse
import os
import os.path
import sys
import shutil
import md5tool
from os import system

md5str = "md5"

def get_file_info_dictionaries(walkdir, protobufdir, cppdir):
    for (dirpath, dirnames, filenames) in os.walk(walkdir):
        for each_filename in filenames:
            is_proto_file = (each_filename[-6:].lower() == '.proto')
            if not is_proto_file:
                continue
            filenamemd5 = md5str + "/" + each_filename + '.md5'
            error = None
            first = False
            if not os.path.exists(filenamemd5):
                first = True
            else:
                error = md5tool.check_against_md5_file(each_filename, filenamemd5)

            hfilename = cppdir + each_filename + ".pb.h"
            cfilename = cppdir + each_filename + ".pb.cc"
            hfilename = hfilename.replace(".proto", "")
            cfilename = cfilename.replace(".proto", "")
            if error == None and os.path.exists(hfilename) and os.path.exists(cfilename) :
                    continue
            print("copy %s %s" % (hfilename, cfilename))
            if not os.path.exists(filenamemd5):
                    md5tool.generate_md5_file_for(each_filename, filenamemd5)
            commond = 'protoc  -I=./ -I=%s --cpp_out=%s %s' % (protobufdir, cppdir, each_filename)
            system(commond)

if not os.path.exists(md5str):
    os.makedirs(md5str)

def genmd5():
    for (dirpath, dirnames, filenames) in os.walk("./"):
        for each_filename in filenames:
            is_proto_file = (each_filename[-6:].lower() == '.proto')
            if not is_proto_file:
                continue            
            md5tool.generate_md5_file_for(each_filename, md5str + "/" + each_filename + '.md5')

get_file_info_dictionaries("./", "../../third_party/protobuf/src", "../pbc/")
genmd5()