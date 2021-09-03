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
    """Walk the directories recursively and match up .md5 files to the files they describe."""

    # Recursively walk the directories, trying to pair up the .md5 files
    file_info_dicts = {}

    for (dirpath, dirnames, filenames) in os.walk(walkdir):
        for each_filename in filenames:
            full_file_path = os.path.join(dirpath, each_filename)
            is_proto_file = (full_file_path[-6:].lower() == '.proto')
            if not is_proto_file:
                continue
            commond = 'protoc  -I=./ -I=%s --cpp_out=%s %s' % (protobufdir, cppdir, each_filename)
            system(commond)

get_file_info_dictionaries("./", "../../../../../third_party/protobuf/src", "../../../protopb/pbc/")