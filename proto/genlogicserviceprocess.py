import os

import md5tool
import shutil
import threading
import genpublic
from multiprocessing import cpu_count

local = threading.local()

local.filemethodarray = []
local.service = ''
threads = []
logicprotodir = 'logic_proto/'
tabstr = '    '
cpprpcservicepart = 1
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
    servicestr = 'class ' + local.service + 'Impl : public ' +  '::' + local.service + '{\npublic:\n'
    servicestr += 'public:\n'
    global controller
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        methodname = s[1]
        requestname = s[2]
        responsespb = s[4]
        line = tabstr + 'void ' + methodname + controller + ',\n'
        line += tabstr + tabstr + 'const ' + '::' + requestname.replace('(', '').replace(')', '') + '* request,\n'
        rsp = responsespb.replace('(', '').replace(')',  '').replace(';',  '').strip('\n')
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
    requestname = s[2]
    servicestr = 'void ' + local.service + 'Impl::' + s[1] + controller + ',\n'
    servicestr +=  tabstr + 'const ' +  '::' + requestname.replace('(', '').replace(')', '') + '* request,\n'
    responsespb = s[4]
    rsp = responsespb.replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr +  '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'
    return servicestr

def genyourcode():
    return genpublic.yourcodebegin + '\n' + genpublic.yourcodeend + '\n'

def getpbdir(writedir):
    if writedir.find(logicprotodir) >= 0:
        return 'src/pb/pbc/logic_proto/'
    return ''

def genheadfile(filename,  destdir,  md5dir):
    filename = os.path.basename(filename).replace('.proto', '.h') 
    md5filename = md5dir +   filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir( destdir) + filename.replace('.h', '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, destdir, md5dir):
    filename = os.path.basename(filename).replace('.proto', '.cpp') 
    destfilename =  destdir + filename
    genfilename = md5dir +   filename
    newstr = '#include "' + filename.replace('.cpp', '.h') + '"\n'
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
        newstr += genyourcode() + '\n'
        newstr += genpublic.rpcbegin + '\n'
    while serviceidx < len(local.filemethodarray) :
        newstr += gencpprpcfunbegin(serviceidx)
        newstr += genpublic.yourcodebegin +  '\n'
        newstr += genpublic.yourcodeend +  '\n}\n\n'
        serviceidx += 1 
    newstr += genpublic.rpcend + '\n'
    with open(genfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

class myThread (threading.Thread):
    def __init__(self, filename, destdir, md5dir):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.md5dir = str(md5dir)
        self.destdir = str(destdir)
    def run(self):
        checkheadmd5,_,_,_ = genpublic.md5check(self.filename, self.destdir, self.md5dir, '.proto', '.h')    
        checkcppmd5,_,_,_  = genpublic.md5check(self.filename, self.destdir, self.md5dir, '.proto', '.cpp' )    
        if checkheadmd5 == True and checkcppmd5 == True:
            return
        parsefile(self.filename)
        if checkheadmd5 == False:
            genheadfile(self.filename, self.destdir, self.md5dir)
            genpublic.md5copy(self.filename, self.destdir, self.md5dir, '.proto', '.h')
        if checkcppmd5 == False:
            gencppfile(self.filename, self.destdir, self.md5dir)
            genpublic.md5copy(self.filename, self.destdir, self.md5dir, '.proto', '.cpp')

def main():
    filelen = len(genfile)
    for i in range(0, filelen):
        t = myThread( genfile[i][0], genfile[i][1], genfile[i][2])
        threads.append(t)
        t.start()
    for t in threads :
        t.join()

def scanprotofile():
    dir_list  = os.listdir(logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        if genpublic.is_gs_and_controller_server_proto(filename) == True :
            genfile.append([logicprotodir + filename, genpublic.controllerlogicservicedir, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + logicprotodir ])
            genfile.append([logicprotodir + filename, genpublic.gslogicervicedir, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + logicprotodir])
        elif filename.find(genpublic.lobby_file_prefix) >= 0:
            genfile.append([logicprotodir + filename, genpublic.lobbylogicservicedir, genpublic.servermd5dirs[genpublic.lobbymd5dirindex] + logicprotodir])
            
genpublic.makedirs()
scanprotofile()
main()
