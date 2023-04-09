import os
from os import system
import md5tool
import shutil
import threading
import genpublic

local = threading.local()

local.filemethodarray = []
local.playerservice = ''
local.service = ''
local.playerservicearray = []
local.fileservice = []

genfile = []
threads = []
tabstr = '    '
clienservciemd5dir = genpublic.md5dirs[genpublic.clientmd5dirindex]
protodir = 'logic_proto/'
includedir = 'src/service/logic_proto/'
clientservicedir = '../client/src/service/logic_proto/'
fileprev = 'c_'
client_player = 'client_player'

if not os.path.exists(clienservciemd5dir):
    os.makedirs(clienservciemd5dir)

def parsefile(filename):
    local.filemethodarray = []
    local.playerservice = ''
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.filemethodarray.append(fileline)
            elif genpublic.is_service_fileline(fileline) == True:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservice = local.service

def classbegin():
    return 'class ' + local.playerservice + 'Service : public PlayerService {\npublic:\n    using PlayerService::PlayerService;\n'  

def genheadrpcfun():
    servicestr = 'public:\n'
    servicestr += tabstr + 'void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n'
    servicestr += tabstr + 'const ::google::protobuf::Message* request,\n'
    servicestr += tabstr + '::google::protobuf::Message* response)override\n'
    servicestr += tabstr + '{\n'
    servicestr += tabstr + tabstr + 'switch(method->index()) {\n'
    index = 0
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        servicestr += tabstr + tabstr + 'case ' + str(index) + ':\n'
        servicestr += tabstr + tabstr + tabstr + 'tls_lua_state["' + s[1] + 'Process"](\n'
        servicestr += tabstr + tabstr + tabstr + '::google::protobuf::internal::DownCast<const ' 
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            respone = '::google::protobuf::Empty*>(response'
        else :
            respone =  '::' + rsp + '*>(response'
        servicestr +=  '::' + s[2].replace('(', '').replace(')', '') + '*>( request),\n'
        servicestr += tabstr + tabstr + tabstr + '::google::protobuf::internal::DownCast<' 
        servicestr += respone + '));\n'
        servicestr += tabstr + tabstr +'break;\n'
        index += 1
    servicestr += tabstr + tabstr + 'default:\n'
    servicestr += tabstr + tabstr + tabstr + 'GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";\n'
    servicestr += tabstr + tabstr + 'break;\n'
    servicestr += tabstr + tabstr + '}\n'
    servicestr += tabstr + '}\n'
    return servicestr

def genheadfile(filename):
    destfilename =  os.path.basename(filename).replace('.proto', '.h')
    newheadfilename = clienservciemd5dir  + os.path.basename(filename).replace('.proto', '.h')
    if not os.path.exists(newheadfilename)  and os.path.exists(destfilename):
        shutil.copy(destfilename, newheadfilename)
        return
    newstr = '#pragma once\n'
    newstr += '#include <sol/sol.hpp>\n'  
    newstr += '#include "player_service.h"\n'  
    newstr += '#include "src/game_logic/thread_local/thread_local_storage_lua.h"\n'  
    newstr += '#include "' + protodir  + os.path.basename(filename).replace('.proto', '.pb.h') + '"\n'
    newstr += classbegin()
    newstr += genheadrpcfun()          
    newstr += '};\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def genplayerservcielist(filename):
    destfilename = clienservciemd5dir +  filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir +  os.path.basename(f) + '.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    for service in local.playerservicearray:
        newstr += 'class ' + service + 'Impl : public '  + service + '{};\n'
    newstr += 'void InitPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        newstr += tabstr + 'g_player_services.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service + 'Service>(new '
        newstr +=  service.replace('.', '') + 'Impl));\n'
    newstr += '}\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def parseplayerservcie(filename):
    if genpublic.is_not_client_proto(filename) == True :
        return
    local.fileservice.append(filename.replace('.proto', ''))
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if genpublic.is_service_fileline(fileline) == True:
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservicearray.append(local.service)

                
def md5copy(filename):
        if filename.find('md5') >= 0 or filename.find('.lua') >= 0:
            return
        gennewfilename = clienservciemd5dir  + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        destfilename =  clientservicedir + filename
        if  not os.path.exists(filenamemd5) or not os.path.exists(gennewfilename) or not os.path.exists(destfilename):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        if error == None and os.path.exists(destfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, destfilename))
        shutil.copy(gennewfilename, destfilename)
        md5tool.generate_md5_file_for(destfilename, filenamemd5)
def md5copydir():
    for (dirpath, dirnames, filenames) in os.walk(clienservciemd5dir):
        for filename in filenames:    
            if filename.find(client_player) >= 0:
                md5copy(filename)
            if filename.find('player_service') >= 0:
                md5copy(filename)


def scanprotofile():
    dir_list  = os.listdir(protodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        genfile.append(protodir  + filename)

class myThread (threading.Thread):
    def __init__(self, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
    def run(self):
        if self.filename.find(client_player) >= 0:
            parsefile(self.filename)
            genheadfile(self.filename)


def main():
    global threads
    for i in range(0, len(genfile)):
        t = myThread(genfile[i])
        threads.append(t)
        t.start()
    for t in threads:
        t.join()
    for file in genfile:
        parseplayerservcie(file)
    genplayerservcielist('player_service.cpp')

scanprotofile() 
main()
md5copydir()

