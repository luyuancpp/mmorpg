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
local.hfilename = ''
local.playerservice = ''
local.playerservicearray = []
local.fileservice = []

threads = []
local.pkg = ''
cpkg = 'package'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'
tabstr = '    '
cpprpcpart = 2
cppmaxpart = 4
controller = '(common::EntityPtr& entity'
servicedir = './md5/'
protodir = 'logic_proto/'
gsplayerservicedir = '../../../../game_server/src/service'
msplayerservicedir = '../../../../master_server/src/service'
client_player = 'client_player'
server_player = 'server_player'

filesrcdestpath = {}

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename):
    local.rpcarry = []
    local.pkg = ''
    local.playerservice = ''
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.rpcarry.append(fileline)
            elif fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservice = 'Player' + local.service

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

    servicestr += tabstr + tabstr + 'void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n'
    servicestr += tabstr + tabstr +  controller.replace('(', '') + ',\n'
    servicestr += tabstr + tabstr + 'const ::google::protobuf::Message* request,\n'
    servicestr += tabstr + tabstr + '::google::protobuf::Message* response)override\n'
    servicestr += tabstr + tabstr + '{\n'
    servicestr += tabstr + tabstr + 'switch(method->index()) {\n'
    index = 0
    for service in local.rpcarry:
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
    servicestr += tabstr + tabstr + '}\n'
    return servicestr

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.rpcarry[rpcindex]
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
def namespacebegin():
    return 'namespace ' + local.pkg + '{\n'
def classbegin():
    return 'class ' + local.playerservice + 'Impl : public game::PlayerService {\npublic:\n    using PlayerService::PlayerService;\n'  
def emptyfun():
    return ''

def genheadfile(filename, writedir):
    headfun = [emptyfun, namespacebegin, classbegin, genheadrpcfun]
    fullfilename = writedir + '/' + filename.replace('.proto', '.h')
    folder_path, local.hfilename = os.path.split(fullfilename)    
    newheadfilename = servicedir + local.hfilename.replace('.proto', '.h')
    headdefine = writedir.replace('/', '_').replace('.', '').upper().strip('_') + '_' + filename.replace('.proto', '').upper().replace('/', '_')
    newstr = '#ifndef ' + headdefine + '_H_\n'
    newstr += '#define ' + headdefine + '_H_\n'
    newstr += '#include "player_service.h"\n'
    newstr += '#include "' + protodir  + local.hfilename.replace('.h', '') + '.pb.h"\n'
    try:
        with open(fullfilename,'r+', encoding='utf-8') as file:
            part = 0
            owncode = 1 
            skipheadline = 0 
            partend = 0
            for fileline in file:
                if skipheadline < 4 :
                    skipheadline += 1
                    continue
                if fileline.find(yourcodebegin) >= 0:
                    owncode = 1
                    newstr += fileline
                    continue
                elif fileline.find(yourcodeend) >= 0:
                    owncode = 0
                    partend = 1
                    newstr += fileline
                    part += 1
                    continue
                if owncode == 1 :
                    newstr += fileline
                    continue
                if part > 0 and part < len(headfun) and owncode == 0 and partend == 1:
                    newstr += headfun[part]()
                    partend = 0
                    continue
                elif part >= len(headfun):
                    break

    except FileNotFoundError:
        for i in range(0, 4) :
            if i > 0:
                newstr += yourcode()
            newstr += headfun[i]()
    newstr += '};\n}// namespace ' + local.pkg + '\n'
    newstr += '#endif//' + headdefine + '_H_\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, writedir):
    global cppmaxpart
    cppfilename = writedir + '/' + filename.replace('.proto', '.cpp').replace(protodir, '')
    newcppfilename = servicedir + local.hfilename.replace('.h', '.cpp')
    newstr = '#include "' + local.hfilename + '"\n'
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            part = 0
            owncode = 1 
            skipheadline = 0 
            serviceidx = 0
            curservicename = ''
            nextrpcline = 0
            for fileline in file:
                if skipheadline < 1 :
                    skipheadline += 1
                    continue
                if part != cpprpcpart and fileline.find(yourcodebegin) >= 0:
                    generated = 0
                    owncode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcpart and fileline.find(yourcodeend) >= 0:
                    owncode = 0
                    newstr += fileline + '\n'
                    part += 1
                    if part == 1 :
                        newstr += namespacebegin()
                    continue     
                elif part == cpprpcpart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(local.rpcarry) and fileline.find(local.servicenames[serviceidx] + controller) >= 0 :
                        curservicename = local.servicenames[serviceidx]
                        owncode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin + ' ' + curservicename + '\n'
                        owncode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + ' ' + curservicename + '\n}\n\n'
                        owncode = 0
                        nextrpcline = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                        owncode = 0
                        while serviceidx < len(local.rpcarry) :
                            newstr += gencpprpcfunbegin(serviceidx)
                            newstr += yourcodebegin + ' ' + curservicename + '\n'
                            newstr += yourcodeend + ' ' + curservicename + '\n}\n\n'
                            serviceidx += 1 
                        newstr += fileline
                        part += 1 
                        continue
                if owncode == 1:
                    newstr += fileline
                    continue                
                if part > cppmaxpart :
                    break
    except FileNotFoundError:
            newstr += yourcode() + '\n'
            newstr += namespacebegin()
            newstr += yourcode() + '\n'
            serviceidx = 0
            newstr += rpcbegin + '\n'
            while serviceidx < len(local.rpcarry) :
                newstr += gencpprpcfunbegin(serviceidx)
                newstr += yourcodebegin + ' ' + local.servicenames[serviceidx] + '\n'
                newstr += yourcodeend + ' ' + local.servicenames[serviceidx] + '\n}\n\n'
                serviceidx += 1 
                newstr += rpcend + '\n'
    newstr += '}// namespace ' + local.pkg + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def generate(filename):
    if filename.find(client_player) >= 0:
        parsefile(filename)
        genheadfile(filename, gsplayerservicedir)
        gencppfile(filename, gsplayerservicedir)
    elif filename.find(server_player) >= 0:
        parsefile(filename)
        genheadfile(filename, gsplayerservicedir)
        gencppfile(filename, gsplayerservicedir)
        genheadfile(filename, msplayerservicedir)
        gencppfile(filename, msplayerservicedir)

def parseplayerservcie(filename):
    local.fileservice.append(filename.replace('.proto', ''))
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservicearray.append(local.pkg + '.' + local.service)
                
def genplayerservcielist(filename):
    fullfilename = servicedir + filename
    newstr =  '#include <memory>\n'
    newstr +=  '#include <unordered_map>\n'
    newstr += '#include "player_service.h"\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
        newstr += '#include "' + f.replace(protodir, '') + '.h"\n'
    newstr += 'namespace game\n{\n'
    newstr += 'std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;\n'
    newstr += 'std::unordered_set<std::string> g_open_player_services;\n'
    for service in local.playerservicearray:
        newstr += 'class ' + service.split('.')[1] + 'Impl : public ' + service.replace('.', '::')  + '{};\n'
    newstr += 'void InitPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        if service.find('playerservice') >= 0:
            newstr += tabstr + 'g_open_player_services.emplace("' + service + '");\n'
        newstr += tabstr + 'g_player_services.emplace("' + service + '"'
        newstr += ', std::make_unique<' + service.split('.')[0] + '::Player' + service.split('.')[1] + 'Impl>(new '
        newstr +=  service.split('.')[1] + 'Impl));\n'
    newstr += '}\n}//namespace game\n'
    with open(fullfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, writedir):
        if filename.find('md5') >= 0:
            return
        if not (filename.find(client_player) >= 0 or   
                filename.find(server_player) >= 0 or 
                filename == 'player_service.cpp' or
                filename == 'player_service.h'):
            return
        gennewfilename = servicedir + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        fullfilename = writedir + '/' + filename
        if error == None and os.path.exists(fullfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, fullfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, fullfilename)
def md5copydir():
    for (dirpath, dirnames, filenames) in os.walk(servicedir):
        for filename in filenames:    
            if filename.find(client_player) >= 0:
                md5copy(filename, gsplayerservicedir)
            elif filename.find(server_player) >= 0:
                md5copy(filename, gsplayerservicedir)
                md5copy(filename, msplayerservicedir)

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
    genplayerservcielist('player_service.cpp')

inputfile() 
main()
md5copydir()

