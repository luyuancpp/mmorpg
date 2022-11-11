import os
from os import system
import md5tool
import shutil
import buildpublic

servicearray = []
servicefilenamearray = []
md5dir = './md5/logic_proto/'
logicprotodir = './logic_proto/'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

def scanservice():
    for (dirpath, dirnames, filenames) in os.walk(logicprotodir):
        for filename in filenames:  
            if filename.find('client_player') >= 0 or filename.find('server_player') >= 0 or filename.find(buildpublic.lobby_file_prefix) >= 0 or filename.find(buildpublic.lobby_file_prefix) > 0:
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
    newstr += 'extern std::array<std::unique_ptr<::google::protobuf::Service>, ' + str(len(servicearray)) + '> g_server_service;\n'
    with open(writedfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(writedfilename):
    global servicearray
    newstr = '#include <array>\n'
    newstr += '#include <memory>\n'
    newstr += '#include <google/protobuf/message.h>\n\n'
    for filename in servicefilenamearray:
        newstr += '#include "'  + filename + '"\n'
    newstr += 'std::array<std::unique_ptr<::google::protobuf::Service>, ' + str(len(servicearray)) + '> g_server_service{\n'
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
gencppfile('./md5/controller_server/server_service.cpp')
gencppfile('./md5/game_server/server_service.cpp')
md5copy('./md5/controller_server/server_service.cpp', '../controller_server/src/service/logic_proto/server_service.cpp')
md5copy('./md5/game_server/server_service.cpp', '../game_server/src/service/logic_proto/server_service.cpp')
md5copy('./md5/server_service.h', '../controller_server/src/service/logic_proto/server_service.h')
md5copy('./md5/server_service.h', '../game_server/src/service/logic_proto/server_service.h')
