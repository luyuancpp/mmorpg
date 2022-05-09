import os
from os import system
import md5tool
import shutil
import threading
import _thread
import protofilearray
from multiprocessing import cpu_count

local = threading.local()

local.rpcarry = []
local.servicenames = []
local.playerservice = ''
local.service = ''
local.playerservice = ''
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
cpprpcpart = 1
controller = '(EntityPtr& entity'
servicedir = './md5/'
protodir = 'logic_proto/'
includedir = 'src/service/logic/'
gsplayerservicedir = '../../../../game_server/src/service/logic/'
rgplayerservicedir = '../../../../region_server/src/service/logic/'
msplayerservicedir = '../../../../master_server/src/service/logic/'
gsservicedir = '../../../../game_server/src/service/'
msservicedir = '../../../../master_server/src/service/'
client_player = 'client_player'
server_player = 'server_player'
rg = 'rg'

filesrcdestpath = {}

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

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
    global filesrcdestpath
    local.pkg = ''
    if filename.find(client_player) >= 0:
        filesrcdestpath[filename] = gsplayerservicedir
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
        servicestr += tabstr + tabstr + tabstr + s[1] + '(entity,\n'
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

def yourcode():
    return yourcodebegin + '\n' + yourcodeend + '\n'
def classbegin():
    return 'class ' + local.playerservice + 'Impl : public PlayerService {\npublic:\n    using PlayerService::PlayerService;\n'  
def emptyfun():
    return ''

def getwritedir(serverstr):
    writedir = ''
    if serverstr == 'gs':
        writedir = gsplayerservicedir
    elif serverstr == 'ms':
        writedir = msplayerservicedir
    elif serverstr == 'rg':
        writedir = rgplayerservicedir
    return writedir

def genheadfile(filename, serverstr):
    local.servicenames = []
    writedir = getwritedir(serverstr)
    headfun = [classbegin, genheadrpcfun]
    fullfilename = writedir +  serverstr + filename.replace('.proto', '.h').replace(protodir, '')
    newheadfilename = servicedir + serverstr + filename.replace('.proto', '.h').replace(protodir, '')
    if not os.path.exists(newheadfilename)  and os.path.exists(fullfilename):
        shutil.copy(fullfilename, newheadfilename)
        return
    newstr = '#pragma once\n'
    newstr += '#include "player_service.h"\n'
    newstr += '#include "' + protodir  + filename.replace('.proto', '.pb.h').replace(protodir, '') + '"\n'
    for i in range(0, len(headfun)) :             
        newstr += headfun[i]()
    newstr += '};\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, serverstr):
    writedir = getwritedir(serverstr)
    cppfilename = writedir  + serverstr + filename.replace('.proto', '.cpp').replace(protodir, '')
    newcppfilename = servicedir + serverstr + filename.replace('.proto', '.cpp').replace(protodir, '')
    if not os.path.exists(newcppfilename) and os.path.exists(cppfilename.replace(protodir, '')):
        shutil.copy(cppfilename.replace(protodir, ''), newcppfilename)
        return
    newstr = '#include "' +  serverstr + filename.replace('.proto', '.h').replace(protodir, '') + '"\n'
    newstr += '#include "src/game_logic/game_registry.h"\n'
    newstr += '#include "src/network/message_sys.h"\n'
    serviceidx = 0
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            part = 0
            owncode = 1 
            skipheadline = 0 
            for fileline in file:
                if skipheadline < 3 :
                    skipheadline += 1
                    continue
                if part != cpprpcpart and fileline.find(yourcodebegin) >= 0:
                    owncode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcpart and fileline.find(yourcodeend) >= 0:
                    owncode = 0
                    newstr += fileline + '\n'
                    part += 1
                    continue     
                elif part == cpprpcpart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(local.rpcarry) and fileline.find(local.servicenames[serviceidx] + controller) >= 0 :
                        owncode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin  + '\n'
                        owncode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + '\n}\n\n'
                        owncode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                        break
                if owncode == 1:
                    newstr += fileline
                    continue                
    except FileNotFoundError:
        newstr += yourcode() + '\n'
        serviceidx = 0
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
        genheadfile(filename, 'gs')
        gencppfile(filename, 'gs')
    elif filename.find(server_player) >= 0:
        parsefile(filename)
        genheadfile(filename, 'gs')
        gencppfile(filename, 'gs')
        genheadfile(filename, 'ms')
        gencppfile(filename, 'ms')
    elif filename.find(rg) >= 0:
        pass

def parseplayerservcie(filename):
    if filename.find('normal') >= 0  or filename.find(rg) >= 0:
        return
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
    fullfilename = servicedir + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir + 'gs' + f.replace(protodir, '') + '.h"\n'
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
    with open(fullfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def genmsplayerservcielist(filename):
    fullfilename = servicedir + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        if f.find(server_player) < 0:
            continue
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + includedir + 'ms' + f.replace(protodir, '') + '.h"\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    newstr += 'std::unordered_set<std::string> g_open_player_services;\n'
    for service in local.playerservicearray:
        if service.find('serverplayer') < 0:
            continue
        newstr += 'class ' + service.split('.')[1] + 'Impl : public ' + service.replace('.', '::')  + '{};\n'
    newstr += 'void InitPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        if service.find('serverplayer') < 0:
            continue
        newstr += tabstr + 'g_player_services.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service.split('.')[0] + '::' + service.split('.')[1] + 'Impl>(new '
        newstr +=  service.split('.')[1] + 'Impl));\n'
    newstr += '}\n'
    with open(fullfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, serverstr):
        writedir = getwritedir(serverstr)
        if filename.find('md5') >= 0 or filename.find('c_') >= 0:
            return
        if serverstr == 'gs_player_service.cpp' or serverstr == 'ms_player_service.cpp':
            serverstr = ''
        gennewfilename = servicedir  + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        fullfilename = writedir  + filename
        if  not os.path.exists(filenamemd5) or not os.path.exists(gennewfilename) or not os.path.exists(fullfilename):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        if error == None and os.path.exists(fullfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, fullfilename))
        shutil.copy(gennewfilename, fullfilename)
        md5tool.generate_md5_file_for(fullfilename, filenamemd5)
def md5copydir():
    for (dirpath, dirnames, filenames) in os.walk(servicedir):
        for filename in filenames:    
            if filename.find(client_player) >= 0:
                md5copy(filename, 'gs')
            elif filename.find(server_player) >= 0 and filename.find('gs') >= 0:
                md5copy(filename, 'gs')
            elif filename.find(server_player) >= 0 and filename.find('ms') >= 0:
                md5copy(filename, 'ms')
            elif filename.find(rg) >= 0 and filename.find('rg') >= 0 and filename.find('rg_node') < 0: 
                pass
            elif filename == 'gs_player_service.cpp':
                md5copy(filename, 'gs')
            elif filename == 'ms_player_service.cpp':
                md5copy(filename, 'ms')

genfile = []

def get_file_list(file_path):
    dir_list = os.listdir(file_path)
    if not dir_list:
        return
    else:
        dir_list = sorted(dir_list,key=lambda x: os.path.getmtime(os.path.join(file_path, x)))
        return dir_list

def inputfile():
    global filesrcdestpath
    filesrcdestpath['player_service.cpp'] = gsplayerservicedir
    filesrcdestpath['player_service.h'] = gsplayerservicedir
    dir_list  = get_file_list(protodir)
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
    genmsplayerservcielist('ms_player_service.cpp')

inputfile() 
main()
md5copydir()

