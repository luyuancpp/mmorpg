import os
from os import system
import md5tool
import shutil

servicearray = []
servicefilenamearray = []

md5dir = './md5/'
logicprotodir = './logic_proto/'
filename = 'server_service.cpp'
writedfilename = md5dir + filename
msdir = '../../../../master_server/src/service/logic/'
gsdir = '../../../../game_server/src/service/logic/'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

def gen():
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

def gencppfile():
    global servicearray
    newstr = '#include <array>\n'
    newstr += '#include <memory>\n'
    newstr += '#include <google/protobuf/message.h>\n'
    for service in servicearray:
        pass
    newstr += 'std::array<std::unique_pt<::google::protobuf::Service*>, ' + str(len(servicearray)) + '> g_server_nomal_service{\n'
    for service in servicearray:
        newstr += 'std::make_unique<::google::protobuf::Service>(new ' +  service + 'Impl),\n'
    newstr = newstr.strip(',\n') + '};\n'
    with open(writedfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(writedir):
    filenamemd5 = writedfilename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(filenamemd5):
        emptymd5 = True
    else:
        error = md5tool.check_against_md5_file(writedfilename, filenamemd5)              
    fullfilename = writedir  + filename
    if error == None and os.path.exists(fullfilename) and emptymd5 == False:
        return
    print("copy %s ---> %s" % (writedfilename, fullfilename))
    md5tool.generate_md5_file_for(writedfilename, filenamemd5)
    shutil.copy(writedfilename, fullfilename)

gen()
md5copy(msdir)
md5copy(gsdir)
