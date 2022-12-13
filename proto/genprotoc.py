#!/usr/bin/env python
# coding=utf-8
import hashlib
import optparse
import os
import os.path
import sys
import shutil
import md5tool
import genpublic
import threading
from multiprocessing import cpu_count
from os import system

md5str = 'md5/'
protobufdir = '../third_party/protobuf/src/'

local = threading.local()

commoncppout = 'md5/common_proto/'

threads = []

dest_dir = '../common/src/pb/pbc/'
if not os.path.exists(dest_dir):
    os.makedirs(dest_dir)

def gen(walkdir, filename):
    if not (filename[-6:].lower() == '.proto'):
        return
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
    if error == None and first == False:
        return

    commond = 'protoc  -I=./ -I=./logic_proto/ -I=./component_proto/ -I=./common_proto/ -I=%s --cpp_out=%s %s' % (protobufdir, dest_dir, proto_destfilename)
    if system(commond) != 0 :
        return

    print('copy %s %s' % (head_destfilename, cpp_destfilename))
    md5tool.generate_md5_file_for(proto_destfilename, proto_md5_destfilename)


class myThread (threading.Thread):
    def __init__(self, walkdir, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.walkdir = str(walkdir)
    def run(self):
        gen(self.walkdir, self.filename)

def gen_protoc(walkdir):
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    filelist = os.listdir(walkdir)
    
    filelen = len(filelist)
    global threads
    step = int(filelen / cpu_count() + 1)
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread(walkdir, filelist[i])
            threads.append(t)
            t.start()
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(walkdir, filelist[i])
                threads.append(t)
                t.start()
    for t in threads :
        t.join()
  

def genmd5(walkdir):
    for filename in os.listdir(walkdir):
        if not (filename[-6:].lower() == '.proto'):
            continue            
        md5tool.generate_md5_file_for(walkdir + filename, md5str + filename + '.md5')

        
genpublic.makedirs()
gen_protoc('./logic_proto/')
gen_protoc('./component_proto/')
gen_protoc('./event_proto/')
gen_protoc('./common_proto/')
genmd5('./component_proto/')
genmd5('./logic_proto/')
genmd5('./event_proto/')
