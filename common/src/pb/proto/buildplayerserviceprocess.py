import os
from os import system
import md5tool
import shutil
import threading
import _thread
import protofilearray
import buildpublic
from multiprocessing import cpu_count

local = threading.local()

local.rpcarry = []
local.servicenames = []
local.playerservice = ''
local.service = ''
local.playerservicearray = []
local.openplayerservicearray = []
local.fileservice = []

threads = []
local.pkg = ''
cpkg = 'package'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'
tabstr = '    '
cpprpcservicepart = 1
controller = '(entt::entity player'
servicedir = './md5/logic_proto/'
protodir = 'logic_proto/'
includedir = 'src/service/logic/'
gsplayerservicedir = '../../../../game_server/src/service/logic/'
lobbyplayerservicedir = '../../../../lobby_server/src/service/logic/'
controllerplayerservicedir = '../../../../controller_server/src/service/logic/'
client_player = 'client_player'
server_player = 'server_player'
filedirdestpath = {}

def isserverpushrpc(service):
    if service.find('S2C') >= 0 or service.find('Push')  >= 0 :
        return True
    return False

def parsefile(filename):
    local.rpcarry = []
    local.pkg = ''
    local.playerservice = ''
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                if isserverpushrpc(fileline) == True :
                    continue
                local.rpcarry.append(fileline)
            elif fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservice = local.service

def inputfiledestdir(filename):
    global filedirdestpath
    local.pkg = ''
    if filename.find(client_player) >= 0:
        filedirdestpath[filename] = gsplayerservicedir
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
                break
def genheadrpcfun():
    global controller
    servicestr = 'public:\n'
    local.servicenames = []
    for service in local.rpcarry:
        if isserverpushrpc(service) == True :
            continue
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        local.servicenames.append(s[1])
        line += tabstr + tabstr + 'const ' + local.pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response);\n'
        else :
            line += tabstr + tabstr + local.pkg + '::' + rsp + '* response);\n\n'
        servicestr += line

    servicestr += tabstr + 'void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n'
    servicestr += tabstr + controller.replace('(', '') + ',\n'
    servicestr += tabstr + 'const ::google::protobuf::Message* request,\n'
    servicestr += tabstr + '::google::protobuf::Message* response)override\n'
    servicestr += tabstr + '{\n'
    servicestr += tabstr + tabstr + 'switch(method->index()) {\n'
    index = 0
    for service in local.rpcarry:
        if isserverpushrpc(service) == True :
            index += 1
            continue
        s = service.strip(' ').split(' ')
        servicestr += tabstr + tabstr + 'case ' + str(index) + ':\n'
        servicestr += tabstr + tabstr + tabstr + s[1] + '(player,\n'
        servicestr += tabstr + tabstr + tabstr + '::google::protobuf::internal::DownCast<const ' 
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            respone = '::google::protobuf::Empty*>(response'
        else :
            respone = local.pkg + '::' + rsp + '*>(response'
        servicestr += local.pkg + '::' + s[2].replace('(', '').replace(')', '') + '*>( request),\n'
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

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.rpcarry[rpcindex]
    if isserverpushrpc(s) == True :
        return servicestr
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + local.playerservice + 'Impl::' + s[1] + controller + ',\n'
    servicestr +=  tabstr + 'const ' + local.pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response)\n{\n'
    else :
        servicestr +=  tabstr + local.pkg + '::' + rsp + '* response)\n{\n'
    return servicestr

def genyourcode():
    return yourcodebegin + '\n' + yourcodeend + '\n'
def classbegin():
    return 'class ' + local.playerservice + 'Impl : public PlayerService {\npublic:\n    using PlayerService::PlayerService;\n'  
def emptyfun():
    return ''

def getdestdir(serverstr):
    destdir = ''
    if serverstr == buildpublic.gs_file_prefix:
        destdir = gsplayerservicedir
    elif serverstr == buildpublic.controller_file_prefix:
        destdir = controllerplayerservicedir
    elif serverstr == buildpublic.lobby_file_prefix:
        destdir = lobbyplayerservicedir
    return destdir

def getsrcpathmd5dir(serverstr):
    srcdir = ''
    if serverstr == buildpublic.gs_file_prefix:
        srcdir = buildpublic.servermd5dirs[buildpublic.gamemd5dirindex]
    elif serverstr == buildpublic.controller_file_prefix:
        srcdir = buildpublic.servermd5dirs[buildpublic.conrollermd5dirindex]
    elif serverstr == buildpublic.lobby_file_prefix:
        srcdir = buildpublic.servermd5dirs[buildpublic.lobbymd5dirindex]
    return srcdir + protodir

def genheadfile(filename, serverstr):
    destdir = getdestdir(serverstr)
    headfunbodyarry = [classbegin, genheadrpcfun]
    destfilename = destdir +  serverstr + filename.replace('.proto', '.h').replace(protodir, '')
    newheadfilename = getsrcpathmd5dir(serverstr) + filename.replace('.proto', '.h').replace(protodir, '')
    if not os.path.exists(newheadfilename)  and os.path.exists(destfilename):
        shutil.copy(destfilename, newheadfilename)
        return
    newstr = '#pragma once\n'
    newstr += '#include "player_service.h"\n'
    newstr += '#include "' + protodir  + filename.replace('.proto', '.pb.h').replace(protodir, '') + '"\n'
    for i in range(0, len(headfunbodyarry)) :             
        newstr += headfunbodyarry[i]()
    newstr += '};\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, serverstr):
    destdir = getdestdir(serverstr)
    cppfilename = destdir  + serverstr + filename.replace('.proto', '.cpp').replace(protodir, '')
    newcppfilename = servicedir + serverstr + filename.replace('.proto', '.cpp').replace(protodir, '')
    if not os.path.exists(newcppfilename) and os.path.exists(cppfilename.replace(protodir, '')):
        shutil.copy(cppfilename.replace(protodir, ''), newcppfilename)
        return
    newstr = '#include "' +  serverstr + filename.replace('.proto', '.h').replace(protodir, '') + '"\n'
    newstr += '#include "src/game_logic/game_registry.h"\n'
    newstr += '#include "src/network/message_system.h"\n'
    serviceidx = 0
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            part = 0
            isyourcode = 1 
            skipheadline = 0 
            for fileline in file:
                if skipheadline < 3 :
                    skipheadline += 1
                    continue
                if part != cpprpcservicepart and fileline.find(yourcodebegin) >= 0:
                    isyourcode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcservicepart and fileline.find(yourcodeend) >= 0:
                    isyourcode = 0
                    newstr += fileline + '\n'
                    part += 1
                    continue     
                elif part == cpprpcservicepart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(local.rpcarry) and fileline.find(local.servicenames[serviceidx] + controller) >= 0 :
                        isyourcode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin  + '\n'
                        isyourcode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + '\n}\n\n'
                        isyourcode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                        break
                if isyourcode == 1:
                    newstr += fileline
                    continue                
    except FileNotFoundError:
        newstr += genyourcode() + '\n'
        newstr += rpcbegin + '\n'
    while serviceidx < len(local.rpcarry) :
        if isserverpushrpc(local.rpcarry[serviceidx]) == True :
            serviceidx += 1 
        else:
            newstr += gencpprpcfunbegin(serviceidx)
            newstr += yourcodebegin +  '\n'
            newstr += yourcodeend + '\n}\n\n'
            serviceidx += 1 
    newstr += rpcend + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def generate(filename):
    if filename.find(client_player) >= 0:
        parsefile(filename)
        genheadfile(filename, buildpublic.gs_file_prefix)
        gencppfile(filename, buildpublic.gs_file_prefix)
    elif filename.find(server_player) >= 0:
        parsefile(filename)
        genheadfile(filename, buildpublic.gs_file_prefix)
        gencppfile(filename, buildpublic.gs_file_prefix)
        genheadfile(filename, buildpublic.controller_file_prefix)
        gencppfile(filename, buildpublic.controller_file_prefix)
    elif filename.find(buildpublic.lobby_file_prefix) >= 0:
        pass

def parseplayerservcie(filename):
    if buildpublic.is_server_proto(filename) == True :
        return
    local.pkg = ''
    local.fileservice.append(filename.replace('.proto', ''))
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservicearray.append(local.service)
                if filename.find(client_player) >= 0:
                    local.openplayerservicearray.append(local.pkg + '.' + local.service)
                
def gengsplayerservcielist(filename):
    destfilename = servicedir + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir + buildpublic.gs_file_prefix + f.replace(protodir, '') + '.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    newstr += 'std::unordered_set<std::string> g_open_player_services;\n'
    for service in local.playerservicearray:
        newstr += 'class ' + service + 'OpenImpl : public '  + service + '{};\n'
    newstr += 'void InitPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        newstr += tabstr + 'g_player_services.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service + 'Impl>(new '
        newstr +=  service.replace('.', '') + 'OpenImpl));\n'
    for service in local.openplayerservicearray:
        newstr += tabstr + 'g_open_player_services.emplace("' + service.replace('.', '') + '");\n'
    newstr += '}\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencontrollerplayerservcielist(filename):
    destfilename = servicedir + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        if f.find(server_player) < 0:
            continue
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir + buildpublic.controller_file_prefix + f.replace(protodir, '') + '.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    newstr += 'std::unordered_set<std::string> g_open_player_services;\n'
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

def md5copy(filename, serverstr):
        destdir = getdestdir(serverstr)
        if filename.find('md5') >= 0 or filename.find('c_') >= 0 or filename.find('sol2') >= 0:
            return
        if serverstr == 'gs_player_service.cpp' or serverstr == 'controller_player_service.cpp':
            serverstr = ''
        gennewfilename = servicedir  + filename
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
    for (dirpath, dirnames, filenames) in os.walk(servicedir):
        for filename in filenames:    
            if filename.find(client_player) >= 0:
                md5copy(filename, buildpublic.gs_file_prefix)
            elif filename.find(server_player) >= 0 and filename.find(buildpublic.gs_file_prefix) >= 0:
                md5copy(filename, buildpublic.gs_file_prefix)
            elif filename.find(server_player) >= 0 and filename.find(buildpublic.controller_file_prefix) >= 0:
                md5copy(filename, buildpublic.controller_file_prefix)
            elif filename.find(buildpublic.lobby_file_prefix) >= 0: 
                pass
            elif filename == 'gs_player_service.cpp':
                md5copy(filename, buildpublic.gs_file_prefix)
            elif filename == 'controller_player_service.cpp':
                md5copy(filename, buildpublic.controller_file_prefix)

genfile = []

def inputfile():
    global filedirdestpath
    filedirdestpath['player_service.cpp'] = gsplayerservicedir
    filedirdestpath['player_service.h'] = gsplayerservicedir
    dir_list  = dir_list = os.listdir(protodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        inputfiledestdir(protodir + filename)
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
    step = filelen / cpu_count() + 1
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread(genfile[i])
            t.start()
            threads.append(t)
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0], genfile[j][1])
                t.start()
                threads.append(t)
    for t in threads :
        t.join()
    for file in genfile:
        parseplayerservcie(file)
    gengsplayerservcielist('gs_player_service.cpp')
    gencontrollerplayerservcielist('controller_player_service.cpp')

buildpublic.makedirs()
buildpublic.makedirsbypath(protodir)
inputfile() 
main()
md5copydir()

