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

def gen_protoc(walkdir, protobufdir, cppdir):
    for each_filename in os.listdir(walkdir):
        if not (each_filename[-6:].lower() == '.proto'):
            continue
        fullname = walkdir +  each_filename
        filenamemd5 = md5str + "/" + each_filename + '.md5'
        error = None
        first = False
        if not os.path.exists(filenamemd5):
            first = True
        else:
            error = md5tool.check_against_md5_file(fullname, filenamemd5)

        hfilename = cppdir + walkdir.replace('./', '') + each_filename + ".pb.h"
        cfilename = cppdir + walkdir.replace('./', '') + each_filename + ".pb.cc"
        hfilename = hfilename.replace(".proto", "")
        cfilename = cfilename.replace(".proto", "")
        if error == None and os.path.exists(hfilename) and os.path.exists(cfilename) :
                continue
        print("copy %s %s" % (hfilename, cfilename))
        if not os.path.exists(filenamemd5):
                md5tool.generate_md5_file_for(fullname, filenamemd5)
        commond = 'protoc  -I=./ -I=./logic_proto -I=%s --cpp_out=%s %s' % (protobufdir, cppdir, fullname)
        print(commond)
        system(commond)

if not os.path.exists(md5str):
    os.makedirs(md5str)

def genmd5(walkdir):
    for each_filename in os.listdir(walkdir):
        if not (each_filename[-6:].lower() == '.proto'):
            continue            
        md5tool.generate_md5_file_for(each_filename, md5str + "/" + each_filename + '.md5')

gen_protoc("./", "../../../../third_party/protobuf/src", "../pbc/")
gen_protoc("./logic_proto/", "../../../../third_party/protobuf/src", "../pbc/")
genmd5("./")

