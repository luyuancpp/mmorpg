import os
from os import system
import md5tool
import shutil
import threading
import genpublic
local = threading.local()

local.filemethodarray = []
local.servicenames = []
local.playerservice = ''
local.service = ''
local.playerservicearray = []
local.fileservice = []
local.md5protodir = []

threads = []
tabstr = '    '
cpprpcservicepart = 1
controller = '(entt::entity player'
protodir = 'logic_proto/'
includedir = 'src/service/logic_proto/'
gslogicervicedir = '../game_server/src/service/logic_proto/'
client_player = 'client_player'
server_player = 'server_player'
filedirdestpath = {}

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


def initservicenames():
    local.servicenames = []
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        local.servicenames.append(s[1])

def genheadrpcfun():
    global controller
    servicestr = 'class ' + local.playerservice + 'Impl : public PlayerService {\npublic:\n    using PlayerService::PlayerService;\n'  
    servicestr += 'public:\n'
    for service in local.filemethodarray:

        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        line += tabstr + tabstr + 'const ' + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n')
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response);\n'
        else :
            line += tabstr + tabstr + '::' + rsp + '* response);\n\n'
        servicestr += line

    servicestr += tabstr + 'void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n'
    servicestr += tabstr + controller.replace('(', '') + ',\n'
    servicestr += tabstr + 'const ::google::protobuf::Message* request,\n'
    servicestr += tabstr + '::google::protobuf::Message* response)override\n'
    servicestr += tabstr + '{\n'
    servicestr += tabstr + tabstr + 'switch(method->index()) {\n'
    index = 0
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        servicestr += tabstr + tabstr + 'case ' + str(index) + ':\n'
        servicestr += tabstr + tabstr + tabstr + s[1] + '(player,\n'
        servicestr += tabstr + tabstr + tabstr + '::google::protobuf::internal::DownCast<const ' 
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n')
        if rsp == 'google.protobuf.Empty' :
            respone = '::google::protobuf::Empty*>(response'
        else :
            respone = '::' + rsp + '*>(response'
        servicestr += '::' + s[2].replace('(', '').replace(')', '') + '*>( request),\n'
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
    servicestr = 'void ' + local.playerservice + 'Impl::' + s[1] + controller + ',\n'
    servicestr +=  tabstr + 'const ' + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response)\n{\n'
    else :
        servicestr +=  tabstr + '::' + rsp + '* response)\n{\n'
    return servicestr

def genyourcode():
    return genpublic.yourcodebegin + '\n' + genpublic.yourcodeend + '\n'

def getsrcpathmd5dir(dirpath):
    srcdir = ''
    if genpublic.isgamedir(dirpath):
        srcdir = genpublic.md5dirs[genpublic.gamemd5dirindex]
    elif genpublic.iscontrollerdir(dirpath):
        srcdir = genpublic.md5dirs[genpublic.conrollermd5dirindex]
    elif genpublic.islobbydir(dirpath):
        srcdir = genpublic.md5dirs[genpublic.lobbymd5dirindex]
    return srcdir + protodir

def genheadfile(filename, destdir):
    destdir = genpublic.getdestdir(destdir)
    newheadfilename = getsrcpathmd5dir(destdir) + os.path.basename(filename).replace('.proto', '.h')
    newstr = '#pragma once\n'
    newstr += '#include "player_service.h"\n'
    newstr += '#include "' + protodir  + os.path.basename(filename).replace('.proto', '.pb.h') + '"\n'           
    newstr += genheadrpcfun()
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        
def parseplayerservcie(filename):
    if genpublic.is_server_proto(filename) == True :
        return
    local.fileservice.append(filename.replace('.proto', ''))
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if genpublic.is_service_fileline(fileline) == True:
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservicearray.append(local.service)

def gengsplayerservcielist(filename):
    destfilename = genpublic.md5dirs[genpublic.gamemd5dirindex] + protodir  + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir  + os.path.basename(f)+ '.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    for service in local.playerservicearray:
        newstr += 'class ' + service + 'OpenImpl : public '  + service + '{};\n'
    newstr += 'void InitPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        newstr += tabstr + 'g_player_services.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service + 'Impl>(new '
        newstr +=  service.replace('.', '') + 'OpenImpl));\n'
    newstr += '}\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencontrollerplayerservcielist(filename):
    destfilename = genpublic.md5dirs[genpublic.conrollermd5dirindex] + protodir + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        if f.find(server_player) < 0:
            continue
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir + os.path.basename(f) + '.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    for service in local.playerservicearray:
        if service.lower().find('serverplayer') < 0:
            continue
        newstr += 'class ' + service + 'OpenImpl : public '  + service + '{};\n'
    newstr += 'void InitPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        if service.lower().find('serverplayer') < 0:
            continue
        newstr += tabstr + 'g_player_services.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service + 'Impl>(new '
        newstr +=  service.replace('.', '') + 'OpenImpl));\n'
    newstr += '}\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, md5path):
        destdir = genpublic.getdestdir(md5path)
        if filename.find('md5') >= 0 or filename.find('c_') >= 0 or filename.find('sol2') >= 0:
            return
        gennewfilename = md5path  + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        destfilename = destdir  + filename
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
    for d in local.md5protodir:
        for (md5dir, dirnames, filenames) in os.walk(d):
            for filename in filenames:    
                if filename.find(client_player) >= 0:
                    md5copy(filename,  md5dir)
                elif (filename.find(server_player) >= 0 and genpublic.isgamedir(md5dir)) or\
                    (filename == 'player_service.cpp' and genpublic.isgamedir(md5dir)):    
                    md5copy(filename, md5dir)
                elif (filename.find(server_player) >= 0 and genpublic.iscontrollerdir(md5dir)) or\
                     (filename == 'player_service.cpp' and genpublic.iscontrollerdir(md5dir)):                   
                    md5copy(filename,  md5dir)

genfile = []

def scanprotofile():
    global filedirdestpath
    filedirdestpath['player_service.cpp'] = gslogicervicedir
    filedirdestpath['player_service.h'] = gslogicervicedir
    dir_list = os.listdir(protodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        genfile.append(protodir  + filename)

class myThread (threading.Thread):
    def __init__(self, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.basefilename = os.path.basename(filename)
    def run(self):
        parsefile(self.filename)
        initservicenames()
        newstr = '#include "'  + self.basefilename.replace('.proto', '.h') + '"\n'
        newstr += '#include "src/game_logic/thread_local/thread_local_storage.h"\n'
        newstr += '#include "src/network/message_system.h"\n'
        cppext = '.cpp'
        
        cppfile = genpublic.cpp()
        cppfile.includestr = newstr
        cppfile.filemethodarray = local.filemethodarray
        cppfile.begunfun = gencpprpcfunbegin
        cppfile.controller = controller
        
        if self.filename.find(client_player) >= 0:
            cppfile.destfilename = genpublic.getdestdir(genpublic.gamemd5dir())  + self.basefilename.replace('.proto', cppext) 
            cppfile.md5filename = getsrcpathmd5dir(genpublic.gamemd5dir()) + self.basefilename.replace('.proto', cppext) 
            genheadfile(self.filename, genpublic.gamemd5dir())
            genpublic.gencppfile(cppfile)
        elif self.filename.find(server_player) >= 0:
            genheadfile(self.filename, genpublic.gamemd5dir())
            cppfile.destfilename = genpublic.getdestdir(genpublic.gamemd5dir())  + self.basefilename.replace('.proto', cppext) 
            cppfile.md5filename = getsrcpathmd5dir(genpublic.gamemd5dir()) + self.basefilename.replace('.proto', cppext) 
            genpublic.gencppfile(cppfile)
            genheadfile(self.filename, genpublic.controllermd5dir())
            cppfile.destfilename = genpublic.getdestdir(genpublic.controllermd5dir())  + self.basefilename.replace('.proto', cppext) 
            cppfile.md5filename = getsrcpathmd5dir(genpublic.controllermd5dir()) + self.basefilename.replace('.proto', cppext) 
            genpublic.gencppfile(cppfile)

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
    gengsplayerservcielist('player_service.cpp')
    gencontrollerplayerservcielist('player_service.cpp')

genpublic.makedirs()
local.md5protodir = genpublic.makedirsbypath(protodir)
scanprotofile() 
main()
md5copydir()

