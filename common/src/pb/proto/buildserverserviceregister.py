import os
from os import system
import md5tool
import shutil

servicearray = []
servicefilenamearray = []
md5dir = './md5/logic_proto/'
logicprotodir = './logic_proto/'

gs_file_prefix = 'gs_'
ms_file_prefix = 'ms_'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

def scanservice():
    for (dirpath, dirnames, filenames) in os.walk(logicprotodir):
        for filename in filenames:  
            if filename.find('_normal.proto') < 0 :
                continue  
            with open(dirpath + filename,'r', encoding='utf-8') as file:
                for fileline in file:
                    if fileline.find('service ') < 0:
                        continue
                    servicearray.append(fileline.split(' ')[1])
                    servicefilenamearray.append(filename.replace('.proto', '.h'))
                    break;

def genheadfile(writedfilename):
    global servicearray
    newstr = '#pragma once\n'
    newstr += '#include <array>\n'
    newstr += '#include <memory>\n'
    newstr += '#include <google/protobuf/message.h>\n\n'
    newstr += 'extern std::array<std::unique_ptr<::google::protobuf::Service>, ' + str(len(servicearray)) + '> g_server_nomal_service;\n'
    with open(writedfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(writedfilename, includeprefix):
    global servicearray
    newstr = '#include <array>\n'
    newstr += '#include <memory>\n'
    newstr += '#include <google/protobuf/message.h>\n\n'
    for filename in servicefilenamearray:
        newstr += '#include "' + includeprefix + filename + '"\n'
    newstr += 'std::array<std::unique_ptr<::google::protobuf::Service>, ' + str(len(servicearray)) + '> g_server_nomal_service{\n'
    for service in servicearray:
        newstr += 'std::unique_ptr<::google::protobuf::Service>(new ' +  service.replace('\n', '') + 'Impl),\n'
    newstr = newstr.strip(',\n') + '};\n'
    with open(writedfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(writedfilename, destfilename):
    filenamemd5 = writedfilename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(filenamemd5):
        emptymd5 = True
    else:
        error = md5tool.check_against_md5_file(writedfilename, filenamemd5) 
    if error == None and os.path.exists(destfilename) and emptymd5 == False:
        return
    print("copy %s ---> %s" % (writedfilename, destfilename))
    md5tool.generate_md5_file_for(writedfilename, filenamemd5)
    shutil.copy(writedfilename, destfilename)

scanservice()
genheadfile('./md5/server_service.h')
gencppfile('./md5/ms_server_service.cpp', ms_file_prefix)
gencppfile('./md5/gs_server_service.cpp', gs_file_prefix)
md5copy('./md5/ms_server_service.cpp', '../../../../master_server/src/service/logic/ms_server_service.cpp')
md5copy('./md5/gs_server_service.cpp', '../../../../game_server/src/service/logic/gs_server_service.cpp')
md5copy('./md5/server_service.h', '../../../../master_server/src/service/logic/server_service.h')
md5copy('./md5/server_service.h', '../../../../game_server/src/service/logic/server_service.h')
