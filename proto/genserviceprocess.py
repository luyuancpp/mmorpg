import os

import md5tool
import shutil
import threading
import _thread
import protofilearray
import genpublic
from multiprocessing import cpu_count

local = threading.local()

local.filemethodarray = []
local.service = ''

threads = []

gsservicedir = '../game_server/src/service/logic_proto/'
lobbyservicedir = '../lobby_server/src/service/logic_proto/'
controllerservicedir = '../controller_server/src/service/logic_proto/'
logicprotodir = 'logic_proto/'
tabstr = '    '
controller = '(::google::protobuf::RpcController* controller'

genfile = []

def parsefile(filename):
    local.filemethodarray = []
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.filemethodarray.append(fileline)
            elif genpublic.is_service_fileline(fileline) == True:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')

def genheadrpcfun():
    servicestr = 'class ' + local.service + 'Impl : public ' + '::' + local.service + '{\npublic:\n'  
    servicestr += 'public:\n'
    global controller
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        rq =  s[2].replace('(', '').replace(')', '')
        line += tabstr + tabstr + 'const ' +  '::' + rq + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response,\n'
        else :
            line += tabstr + tabstr +  '::' + rsp + '* response,\n'
        line += tabstr + tabstr + '::google::protobuf::Closure* done)override;\n\n'
        servicestr += line
    servicestr += '};'
    return servicestr

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.filemethodarray[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + local.service + 'Impl::' + s[1] + controller + ',\n'
    rq =  s[2].replace('(', '').replace(')', '')
    servicestr +=  tabstr + 'const ' + '::' + rq + '* request,\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr + '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'

    return servicestr

def genyourcode():
    return genpublic.yourcodebegin + '\n' + genpublic.yourcodeend + '\n'

def classbegin():
    return 


def getprevfilename(filename, destdir):
    if filename.find(logicprotodir) >= 0:
        if destdir == gsservicedir:
            return genpublic.gs_file_prefix
        if destdir == controllerservicedir:
            return genpublic.controller_file_prefix
        if destdir == lobbyservicedir:
            return ''
    return ''

def getpbdir(filename):
    if filename.find(logicprotodir) >= 0:
        return 'src/pb/pbc/logic_proto/'
    return ''

def genheadfile(file,   md5dir):
    filename = file.replace('common_proto/', '').replace('.proto', '.h') 
    md5filename = md5dir +  filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir(filename) + filename.replace('.h', '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, destdir, md5dir):
    filename = filename.replace('common_proto/', '').replace('.proto', '.cpp') 
    destfilename = destdir + filename
    md5filename = md5dir +  filename
    newstr = '#include "' + getprevfilename(destfilename, destdir) + filename.replace('.cpp', '.h') + '"\n'
    newstr += '#include "src/network/rpc_msg_route.h"\n'
    serviceidx = 0
    try:
        with open(destfilename,'r+', encoding='utf-8') as file:
            service_begined = 0
            isyourcode = 1 
            skipheadline = 0 
            for fileline in file:
                if skipheadline < 2 :
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
        newstr += genpublic.rpcbegin + '\n'
    while serviceidx < len(local.filemethodarray) :
        newstr += gencpprpcfunbegin(serviceidx)
        newstr += genpublic.yourcodebegin +  '\n'
        newstr += genpublic.yourcodeend +  '\n}\n\n'
        serviceidx += 1 
    newstr += genpublic.rpcend + '\n'
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)


def md5copy(filename, destdir, md5dir, extreplacesrc, extreplacedest):
    check, destfilename, genfilename , genfilenamemd5 = genpublic.md5check(filename, destdir, md5dir, extreplacesrc, extreplacedest )    
    if check == True:
        return
    print("copy %s ---> %s" % (genfilename, destfilename))
    md5tool.generate_md5_file_for(genfilename, genfilenamemd5)
    shutil.copy(genfilename, destfilename)

def generate(filename, destdir, md5dir):
    parsefile(filename)
    genheadfile(filename,  md5dir)
    gencppfile(filename, destdir, md5dir)
    md5copy(filename, destdir, md5dir, '.proto', '.h')
    md5copy(filename, destdir, md5dir, '.proto', '.cpp')

class myThread (threading.Thread):
    def __init__(self, filename, destdir, md5dir):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.md5dir = str(md5dir)
        self.destdir = str(destdir)
    def run(self):
        generate(self.filename, self.destdir, self.md5dir)

def main():
    global threads
    for i in range(0, len(genfile)):
            t = myThread( genfile[i][0], genfile[i][1], genfile[i][2])
            threads.append(t)
            t.start()
    for t in threads :
        t.join()

genfile = protofilearray.genfile
genpublic.makedirs()
main()
