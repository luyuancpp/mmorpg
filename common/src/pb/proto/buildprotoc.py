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

md5str = 'md5'

srcdir = '../pbc/'
if not os.path.exists(srcdir):
    os.makedirs(srcdir)

def gen_protoc(walkdir, protobufdir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    for filename in os.listdir(walkdir):
        if not (filename[-6:].lower() == '.proto'):
            continue
        proto_destfilename = walkdir +  filename
        proto_md5_destfilename = md5str + '/' + filename + '.md5'
        error = None
        first = False
        if not os.path.exists(proto_md5_destfilename):
            first = True
        else:
            error = md5tool.check_against_md5_file(proto_destfilename, proto_md5_destfilename)
        pbcdir = dest_dir + walkdir.replace('./', '')
        head_destfilename = pbcdir + filename.replace('.proto', '') + '.pb.h'
        cpp_destfilename = pbcdir + filename.replace('.proto', '') + '.pb.cc'
        if error == None and os.path.exists(head_destfilename) and os.path.exists(cpp_destfilename) :
                continue
        print('copy %s %s' % (head_destfilename, cpp_destfilename))
        if not os.path.exists(proto_md5_destfilename):
                md5tool.generate_md5_file_for(proto_destfilename, proto_md5_destfilename)
        commond = 'protoc  -I=./ -I=./logic_proto/ -I=./component_proto/ -I=./common_proto/ -I=%s --cpp_out=%s %s' % (protobufdir, dest_dir, proto_destfilename)
        system(commond)

if not os.path.exists(md5str):
    os.makedirs(md5str)

def genmd5(walkdir):
    for filename in os.listdir(walkdir):
        if not (filename[-6:].lower() == '.proto'):
            continue            
        md5tool.generate_md5_file_for(walkdir + filename, md5str + '/' + filename + '.md5')

gen_protoc('./logic_proto/', '../../../../third_party/protobuf/src/', '../pbc/')
gen_protoc('./component_proto/', '../../../../third_party/protobuf/src/', '../pbc/')
gen_protoc('./event_proto/', '../../../../third_party/protobuf/src/', '../pbc/')
genmd5('./component_proto/')
genmd5('./logic_proto/')
genmd5('./event_proto/')

#common proto
gen_protoc('./common_proto/controller/', '../../../../third_party/protobuf/src/', '../pbc/common_proto/controller/')
gen_protoc('./common_proto/database/', '../../../../third_party/protobuf/src/', '../pbc/common_proto/database/')
gen_protoc('./common_proto/deploy/', '../../../../third_party/protobuf/src/', '../pbc/deploy/')
gen_protoc('./common_proto/game/', '../../../../third_party/protobuf/src/', '../pbc/game/')
gen_protoc('./common_proto/gate/', '../../../../third_party/protobuf/src/', '../pbc/gate/')
gen_protoc('./common_proto/lobby/', '../../../../third_party/protobuf/src/', '../pbc/lobby/')
gen_protoc('./common_proto/login/', '../../../../third_party/protobuf/src/', '../pbc/login/')
gen_protoc('./common_proto/', '../../../../third_party/protobuf/src/', '../pbc/')
genmd5('./common_proto/controller/')
genmd5('./common_proto/database/')
genmd5('./common_proto/deploy/')
genmd5('./common_proto/game/')
genmd5('./common_proto/gate/')
genmd5('./common_proto/lobby/')
genmd5('./common_proto/login/')
genmd5('./common_proto/')