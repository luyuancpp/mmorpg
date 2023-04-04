import os
from os import system
import md5tool
import shutil
import threading
import _thread
import protofilearray
import genpublic
from multiprocessing import cpu_count

local = threading.local()

local.filemethodarray = []
local.servicenames = []
local.playerservice = ''
local.service = ''
local.playerservicearray = []
local.fileservice = []

threads = []

tabstr = '    '
cpprpcservicepart = 1
controller = '(entt::entity player'
protodir = 'logic_proto/'
includedir = 'src/service/logic_proto/'
gslogicervicedir = '../game_server/src/service/logic_proto/'
lobbylogicservicedir = '../lobby_server/src/service/logic_proto/'
controllerlogicservicedir = '../controller_server/src/service/logic_proto/'

repliedmd5dir = genpublic.logicprotodir.replace('logic_proto', 'logic_proto_replied')

def parsefile(filename):
    if not genpublic.is_server_player_proto(filename):
        return
    local.filemethodarray = []
    local.service = ''
    local.playerservice = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.filemethodarray.append(fileline)
            elif genpublic.is_service_fileline(fileline) == True:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservice = local.service
                
def initservicenames():
    local.servicenames = []
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        local.servicenames.append(s[1])
        
def genheadrpcfun():
    global controller
    servicestr = 'class ' + local.playerservice + 'RepliedImpl : public PlayerServiceReplied {\npublic:\n    using PlayerServiceReplied::PlayerServiceReplied;\n'  
    servicestr += 'public:\n'
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n')
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response);\n'
        else :
            line += tabstr + tabstr +  '::' + rsp + '* response);\n\n'
        servicestr += line

    servicestr += tabstr + 'void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n'
    servicestr += tabstr + controller.replace('(', '') + ',\n'
    servicestr += tabstr + '::google::protobuf::Message* response)override\n'
    servicestr += tabstr + '{\n'
    servicestr += tabstr + tabstr + 'switch(method->index()) {\n'
    index = 0
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        servicestr += tabstr + tabstr + 'case ' + str(index) + ':\n'
        servicestr += tabstr + tabstr + tabstr + s[1] + '(player,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            respone = '::google::protobuf::Empty*>(response'
        else :
            respone = '::' + rsp + '*>(response'
        servicestr += tabstr + tabstr + tabstr + '::google::protobuf::internal::DownCast<' 
        servicestr += respone + '));\n'
        servicestr += tabstr + tabstr +'break;\n'
        index += 1
    servicestr += tabstr + tabstr + 'default:\n'
    servicestr += tabstr + tabstr + tabstr + 'GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";\n'
    servicestr += tabstr + tabstr + 'break;\n'
    servicestr += tabstr + tabstr + '}\n'
    servicestr += tabstr + '}\n'
    servicestr += '};\n'
    return servicestr

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.filemethodarray[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + local.playerservice + 'RepliedImpl::' + s[1] + controller + ',\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response)\n{\n'
    else :
        servicestr +=  tabstr +  '::' + rsp + '* response)\n{\n'
    return servicestr

def genyourcode():
    return genpublic.yourcodebegin + '\n' + genpublic.yourcodeend + '\n'
 
def getsrcpathmd5dir(dirpath):
    srcdir = ''
    if genpublic.isgamedir(dirpath):
        srcdir = genpublic.servermd5dirs[genpublic.gamemd5dirindex]
    elif genpublic.iscontrollerdir(dirpath):
        srcdir = genpublic.servermd5dirs[genpublic.conrollermd5dirindex]
    elif genpublic.islobbydir(dirpath):
        srcdir = genpublic.servermd5dirs[genpublic.lobbymd5dirindex]
    return srcdir + protodir

def genheadfile(filename, md5dir):
    newheadfilename = md5dir + filename.replace('.proto', '_replied.h').replace(protodir, '')
    newstr = '#pragma once\n'
    newstr += '#include "player_service_replied.h"\n'
    newstr += '#include "' + protodir  + filename.replace('.proto', '.pb.h').replace(protodir, '') + '"\n'           
    newstr += genheadrpcfun()
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        
def gencppfile(filename, destdir, md5dir):
    cppfilename = destdir  + filename.replace('.proto', '_replied.cpp').replace(protodir, '')
    newcppfilename = md5dir + filename.replace('.proto', '_replied.cpp').replace(protodir, '')
    newstr = '#include "'  + filename.replace('.proto', '_replied.h').replace(protodir, '') + '"\n'
    newstr += '#include "src/game_logic/thread_local/thread_local_storage.h"\n'
    newstr += '#include "src/network/message_system.h"\n'
    serviceidx = 0
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            service_begined = 0
            isyourcode = 1 
            skipheadline = 0 
            for fileline in file:
                if skipheadline < 3 :
                    skipheadline += 1
                    continue
                     #处理开始自定义文件
                if service_begined == 0 and fileline.find(genpublic.rpcbegin) >= 0:
                    newstr += fileline
                    service_begined = 1
                    continue 
                #开始处理RPC 
                if service_begined == 1:
                    if serviceidx < len(local.filemethodarray) and fileline.find(controller) >= 0 :
                        isyourcode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(genpublic.yourcodebegin) >= 0 :
                        newstr += fileline
                        isyourcode = 1
                        continue
                    elif fileline.find(genpublic.yourcodeend) >= 0 :
                        newstr += genpublic.yourcodeend + '\n}\n\n'
                        isyourcode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(genpublic.rpcend) >= 0:
                        break
                if isyourcode == 1 or service_begined == 0:
                    newstr += fileline
                    continue                
    except FileNotFoundError:
        newstr += genyourcode() + '\n'
        newstr += genpublic.rpcbegin + '\n'
    while serviceidx < len(local.filemethodarray) :
            newstr += gencpprpcfunbegin(serviceidx)
            newstr += genpublic.yourcodebegin +  '\n'
            newstr += genpublic.yourcodeend + '\n}\n\n'
            serviceidx += 1 
    newstr += genpublic.rpcend + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def parseplayerservcie(filename):
    if not genpublic.is_server_player_proto(filename):
        return
    local.fileservice.append(filename.replace('.proto', ''))
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if genpublic.is_service_fileline(fileline) == True:
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservicearray.append(local.service)

def genplayerservcierepliedlist(filename, md5dir):
    md5filename = md5dir   + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service_replied.h"\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir.replace('logic_proto', 'logic_proto_replied')  + f.replace(protodir, '') + '_replied.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replieds;\n'
    for service in local.playerservicearray:
        newstr += 'class ' + service + 'RepliedRegisterImpl : public '  + service + '{};\n'
    newstr += 'void InitPlayerServcieReplied()\n{\n'
    for service in local.playerservicearray:
        newstr += tabstr + 'g_player_service_replieds.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service + 'RepliedImpl>(new '
        newstr +=  service.replace('.', '') + 'RepliedRegisterImpl));\n'
    newstr += '}\n'
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, destdir, md5dir, fileextend):
        if filename.find('/') >= 0 :
            s = filename.split('/')
            filename = s[len(s) - 1]
        gennewfilename = md5dir + filename.replace('.proto', fileextend)
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)           
        destfilename =  destdir + filename.replace('.proto', fileextend)
        if error == None and os.path.exists(destfilename) and emptymd5 == False:
            return
        
        print("copy %s ---> %s" % (gennewfilename, destfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, destfilename)


def generate(filename):
    if not genpublic.is_server_player_proto(filename):
        return
    parsefile(filename)
    initservicenames()
    genheadfile(filename, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + repliedmd5dir)
    gencppfile(filename, genpublic.gslogicrepliedservicedir, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + repliedmd5dir)
    md5copy(filename, genpublic.gslogicrepliedservicedir, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + repliedmd5dir, '_replied.h')
    md5copy(filename, genpublic.gslogicrepliedservicedir, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + repliedmd5dir, '_replied.cpp')
    genheadfile(filename, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + repliedmd5dir)
    gencppfile(filename, genpublic.controllerlogicrepliedservicedir, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + repliedmd5dir)
    md5copy(filename, genpublic.controllerlogicrepliedservicedir, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + repliedmd5dir, '_replied.h')
    md5copy(filename, genpublic.controllerlogicrepliedservicedir, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + repliedmd5dir, '_replied.cpp')
        
        
genfile = []

def scanprotofile():
    dir_list  = dir_list = os.listdir(protodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        genfile.append(protodir  + filename)

class myThread (threading.Thread):
    def __init__(self, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
    def run(self):
        generate(self.filename)

def main():
    filelen = len(genfile)
    global threads
    for i in range(0, len(genfile)):
            t = myThread(genfile[i])
            threads.append(t)
            t.start()
    for t in threads :
        t.join()
    for file in genfile:
        parseplayerservcie(file)
    genplayerservcierepliedlist('player_service_replied.cpp', genpublic.servermd5dirs[genpublic.gamemd5dirindex] + repliedmd5dir)
    md5copy('player_service_replied.cpp', genpublic.gslogicrepliedservicedir, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + repliedmd5dir, '')
    genplayerservcierepliedlist('player_service_replied.cpp', genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + repliedmd5dir)    
    md5copy('player_service_replied.cpp', genpublic.controllerlogicrepliedservicedir, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + repliedmd5dir, '')

genpublic.makedirs()
genpublic.makedirsbypath(protodir)
scanprotofile() 
main()

