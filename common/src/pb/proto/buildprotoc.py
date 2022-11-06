#!/usr/bin/env python
# coding=utf-8
import hashlib
import optparse
import os
import os.path
import sys
import shutil
import md5tool
import buildpublic
from os import system

md5str = 'md5/'
protobufdir = '../../../../third_party/protobuf/src/'

commoncppout = 'md5/common_proto/'

srcdir = '../pbc/'
if not os.path.exists(srcdir):
    os.makedirs(srcdir)

def gen_protoc(walkdir, dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    for filename in os.listdir(walkdir):
        if not (filename[-6:].lower() == '.proto'):
            continue
        proto_destfilename = walkdir +  filename
        proto_md5_destfilename = md5str + filename + '.md5'
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

def gen_common_protoc(walkdir,  dest_dir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    for filename in os.listdir(walkdir):
        if not (filename[-6:].lower() == '.proto'):
            continue
        proto_destfilename = walkdir +  filename
        proto_md5_destfilename = md5str + filename + '.md5'
        error = None
        first = False
        if not os.path.exists(proto_md5_destfilename):
            first = True
        else:
            error = md5tool.check_against_md5_file(proto_destfilename, proto_md5_destfilename)
        
        headmd5_destfilename = dest_dir + filename.replace('.proto', '') + '.pb.h'
        cppmd5_destfilename = dest_dir + filename.replace('.proto', '') + '.pb.cc'
        
        if error == None and os.path.exists(headmd5_destfilename) and os.path.exists(cppmd5_destfilename) :
            continue
        
        if not os.path.exists(proto_md5_destfilename):
            md5tool.generate_md5_file_for(proto_destfilename, proto_md5_destfilename)
        commond = 'protoc  -I=./ -I=./logic_proto/ -I=./component_proto/ -I=./common_proto/ -I=%s --cpp_out=%s %s' % (protobufdir, commoncppout, proto_destfilename)
        system(commond)
        
        md5name = (commoncppout + proto_destfilename.replace('./', '')).replace('.proto', '')
        headmd5pbh = md5name + '.pb.h'
        cppmd5pbh = md5name + '.pb.cc'
        shutil.copy(headmd5pbh, headmd5_destfilename)
        shutil.copy(cppmd5pbh, cppmd5_destfilename)
        print('copy %s %s' % (cppmd5pbh , cppmd5_destfilename))

buildpublic.makedirs()

def genmd5(walkdir):
    for filename in os.listdir(walkdir):
        if not (filename[-6:].lower() == '.proto'):
            continue            
        md5tool.generate_md5_file_for(walkdir + filename, md5str + filename + '.md5')

gen_protoc('./logic_proto/', '../pbc/')
gen_protoc('./component_proto/',  '../pbc/')
gen_protoc('./event_proto/',  '../pbc/')
gen_protoc('./common_proto/',  '../pbc/common_proto/')
genmd5('./component_proto/')
genmd5('./logic_proto/')
genmd5('./event_proto/')
